#include "DspChain.h"

#include <algorithm>
#include <cmath>

namespace otoha
{
namespace
{
float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }

// One-pole smoothing coefficient for a time constant in milliseconds.
float coefficientFor (float milliseconds, double sampleRate)
{
    const float seconds = juce::jmax (0.5f, milliseconds) * 0.001f;
    return std::exp (-1.0f / (seconds * (float) sampleRate));
}

struct NrTuning
{
    float thresholdDb;   // below this envelope level, treat as background
    float depthDb;       // maximum downward expansion
    float attackMs;
    float releaseMs;
};

NrTuning tuningFor (NoiseReductionMode mode, float strength)
{
    const float s = juce::jlimit (0.0f, 1.0f, strength);

    switch (mode)
    {
        case NoiseReductionMode::gentle:
            return { -52.0f + 4.0f * s,
                     juce::jmap (s, 6.0f, 14.0f),
                     15.0f, 260.0f };
        case NoiseReductionMode::strong:
            return { -46.0f + 4.0f * s,
                     juce::jmap (s, 12.0f, 24.0f),
                     8.0f, 180.0f };
        case NoiseReductionMode::off:
            break;
    }
    return { -120.0f, 0.0f, 10.0f, 200.0f };
}
} // namespace

// =============================================================================
// Lifecycle
// =============================================================================
void DspChain::prepare (double newSampleRate, int newNumChannels)
{
    sampleRate  = juce::jmax (8000.0, newSampleRate);
    numChannels = juce::jmax (1, juce::jmin (2, newNumChannels));   // mono/stereo only

    nrEnvelope.assign ((size_t) numChannels, 0.0f);
    nrGain.assign ((size_t) numChannels, 1.0f);
    compEnvelope = 0.0f;
    compGainSmoothed = 1.0f;
    limGain = 1.0f;
    eqDirty = true;
    nrHighPassDirty = true;

    const juce::SpinLock::ScopedLockType sl (paramLock);
    nrHighPass.reset();
    for (auto& f : eqFilters)
        f.reset();
}

void DspChain::reset()
{
    nrHighPass.reset();
    for (auto& f : eqFilters)
        f.reset();
    std::fill (nrEnvelope.begin(), nrEnvelope.end(), 0.0f);
    std::fill (nrGain.begin(), nrGain.end(), 1.0f);
    compEnvelope = 0.0f;
    compGainSmoothed = 1.0f;
    limGain = 1.0f;
}

void DspChain::setParameters (const ProcessingState& p)
{
    // Message thread only: publish the new snapshot for the audio side.
    const juce::SpinLock::ScopedLockType sl (paramLock);
    incoming = p;
}

// =============================================================================
// EQ coefficients (rebuilt lazily when gains change)
// =============================================================================
void DspChain::rebuildEqCoefficients()
{
    // Bands: low shelf, three peaking, high shelf.
    eqFilters[0].setCoefficients (juce::IIRCoefficients::makeLowShelf (
        sampleRate, EqParams::frequencies[0], EqParams::q, dbToGain (working.eq.gainsDb[0])));

    for (int band = 1; band <= 3; ++band)
        eqFilters[band].setCoefficients (juce::IIRCoefficients::makePeakFilter (
            sampleRate, EqParams::frequencies[band], EqParams::q, dbToGain (working.eq.gainsDb[band])));

    eqFilters[4].setCoefficients (juce::IIRCoefficients::makeHighShelf (
        sampleRate, EqParams::frequencies[4], EqParams::q, dbToGain (working.eq.gainsDb[4])));

    for (int band = 0; band < 5; ++band)
        appliedEqGains[band] = working.eq.gainsDb[band];

    eqDirty = false;
}

// =============================================================================
// Per-sample processors
// =============================================================================
float DspChain::processNoiseReduction (int ch, float sample)
{
    if (working.noiseReduction.mode == NoiseReductionMode::off)
        return sample;

    // First line of defence against steady rumble/fan noise.
    float x = nrHighPass.processSingleSampleRaw (sample);

    const auto tuning = tuningFor (working.noiseReduction.mode, working.noiseReduction.strength);
    const float absX = std::abs (x);

    // Envelope follower: fast up, slow down.
    nrEnvelope[(size_t) ch] = std::max (absX,
        nrEnvelope[(size_t) ch] * coefficientFor (tuning.releaseMs, sampleRate));

    // Downward expander: quiet passages are ducked towards -depthDb.
    const float thresholdLin = dbToGain (tuning.thresholdDb);
    const float targetGain = nrEnvelope[(size_t) ch] >= thresholdLin
        ? 1.0f
        : dbToGain (-tuning.depthDb);

    // One-pole smoothing towards the target (fast when opening, slow when closing).
    const float smoothingUp   = 1.0f - coefficientFor (tuning.attackMs, sampleRate);
    const float smoothingDown = 1.0f - coefficientFor (tuning.releaseMs, sampleRate);
    const float amount = targetGain > nrGain[(size_t) ch] ? smoothingUp : smoothingDown;
    nrGain[(size_t) ch] += (targetGain - nrGain[(size_t) ch]) * amount;

    return x * nrGain[(size_t) ch];
}

float DspChain::eqStage (float sample)
{
    for (auto& f : eqFilters)
        sample = f.processSingleSampleRaw (sample);
    return sample;
}

void DspChain::updateCompressorGain (float linkedPeak)
{
    if (! working.compressor.enabled)
    {
        compGainSmoothed = 1.0f;
        return;
    }

    // Peak detector with attack/release shaping.
    const float attackCoef  = coefficientFor (working.compressor.attackMs, sampleRate);
    const float releaseCoef = coefficientFor (working.compressor.releaseMs, sampleRate);
    compEnvelope = linkedPeak > compEnvelope
        ? linkedPeak + (compEnvelope - linkedPeak) * attackCoef
        : linkedPeak + (compEnvelope - linkedPeak) * releaseCoef;

    // Hard-knee gain computer.
    const float thresholdLin = dbToGain (working.compressor.thresholdDb);
    float targetGain = 1.0f;
    if (compEnvelope > thresholdLin && compEnvelope > 0.0f)
    {
        const float overDb    = 20.0f * std::log10 (compEnvelope / thresholdLin);
        const float outOverDb = overDb / juce::jmax (1.2f, working.compressor.ratio);
        targetGain = dbToGain (outOverDb - overDb);
    }

    // Extra smoothing so parameter moves never step the gain.
    compGainSmoothed += (targetGain - compGainSmoothed) * 0.2f;
}

void DspChain::updateLimiterGain (float linkedPeak)
{
    if (! working.limiter.enabled || ! working.limiterEngaged)
    {
        limGain = 1.0f;
        return;
    }

    const float ceilingLin = dbToGain (working.limiter.ceilingDb);

    // Instant attack (grab overs immediately), exponential release back to unity.
    const float target = linkedPeak > ceilingLin ? ceilingLin / std::max (linkedPeak, 1.0e-9f) : 1.0f;
    limGain = target < limGain ? target
                               : target + (limGain - target) * coefficientFor (working.limiter.releaseMs, sampleRate);
}

// =============================================================================
// Chain
// =============================================================================
void DspChain::process (float* const* channels, int numFrames)
{
    if (numFrames <= 0 || channels == nullptr || ! isPrepared() || numChannels == 0)
        return;

    // Pull the latest parameter snapshot once per block (tiny POD copy).
    {
        const juce::SpinLock::ScopedLockType sl (paramLock);
        working = incoming;
    }

    if (! working.enabled)
        return;   // bypass: the timeline flows through untouched

    if (eqDirty)
        rebuildEqCoefficients();

    if (working.noiseReduction.mode != NoiseReductionMode::off)
    {
        if (nrHighPassDirty)
        {
            nrHighPass.setCoefficients (juce::IIRCoefficients::makeHighPass (sampleRate, 85.0, 0.7));
            nrHighPassDirty = false;
        }
    }
    else
    {
        nrHighPass.reset();
        nrHighPassDirty = true;
    }

    const float makeup = dbToGain (working.compressor.makeupGainDb);

    for (int i = 0; i < numFrames; ++i)
    {
        // --- stage 1+2: noise reduction then EQ, per channel -------------------
        float stage[2] = { 0.0f, 0.0f };
        float linkedPeak = 0.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float x = processNoiseReduction (ch, channels[ch][i]);
            x = eqStage (x);

            stage[ch & 1] = x;
            linkedPeak = std::max (linkedPeak, std::abs (x));
        }

        // --- stage 3: compressor (linked detector, equal gain per channel) -----
        updateCompressorGain (linkedPeak);
        for (int ch = 0; ch < numChannels; ++ch)
            stage[ch & 1] *= compGainSmoothed * makeup;

        // --- stage 4: limiter (linked, final safety stage) ---------------------
        linkedPeak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            linkedPeak = std::max (linkedPeak, std::abs (stage[ch & 1]));

        updateLimiterGain (linkedPeak);
        for (int ch = 0; ch < numChannels; ++ch)
            channels[ch][i] = stage[ch & 1] * limGain;
    }
}
} // namespace otoha
