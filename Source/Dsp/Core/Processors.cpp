#include "Processors.h"

#include <algorithm>
#include <cmath>

namespace otoha::dsp
{
namespace
{
float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }

float coefficientFor (float milliseconds, double sampleRate)
{
    const float seconds = juce::jmax (0.5f, milliseconds) * 0.001f;
    return std::exp (-1.0f / (seconds * (float) sampleRate));
}
} // namespace

// =============================================================================
// GainProcessor
// =============================================================================
void GainProcessor::prepare (const ProcessingContext& context)
{
    inputGain.setTimeConstant (30.0f, context.sampleRate);
    outputGain.setTimeConstant (30.0f, context.sampleRate);
}

void GainProcessor::reset() {}

void GainProcessor::setParameters (const ProcessingState& state)
{
    inputGainDbTarget  = state.inputGainDb;
    outputGainDbTarget = state.outputGainDb;
    inputGain.setTarget (dbToGain (state.inputGainDb));
    outputGain.setTarget (dbToGain (state.outputGainDb));
}

void GainProcessor::process (AudioBlock& block)
{
    for (int i = 0; i < block.numFrames; ++i)
    {
        const float g = inputGain.next() * outputGain.next();
        if (juce::approximatelyEqual (g, 1.0f))
            continue;   // neutral gains: zero cost, zero coloration

        for (int ch = 0; ch < block.numChannels; ++ch)
            block.channelData[ch][i] *= g;
    }
}

// =============================================================================
// BassProcessor
// =============================================================================
void BassProcessor::prepare (const ProcessingContext& context)
{
    sampleRate = context.sampleRate;
    shelf.clear();
    for (int ch = 0; ch < juce::jmax (1, context.numChannels); ++ch)
        shelf.push_back (std::make_unique<juce::IIRFilter>());
    gainDb.setTimeConstant (60.0f, context.sampleRate);
    gainDb.reset (0.0f);
    dirtyShelf = true;
}

void BassProcessor::reset()
{
    for (auto& f : shelf) f->reset();
}

void BassProcessor::setParameters (const ProcessingState& state)
{
    amountTarget = juce::jlimit (0.0f, 1.0f, state.bassAmount);
    const float target = amountTarget * 6.0f;   // up to +6 dB low shelf
    if (std::abs (target - gainDb.getTarget()) > 0.01f)
    {
        gainDb.setTarget (target);
        dirtyShelf = true;
    }
}

void BassProcessor::process (AudioBlock& block)
{
    if (amountTarget <= 0.0f)
        return;

    if (dirtyShelf)
    {
        const auto coefficients = juce::IIRCoefficients::makeLowShelf (
            sampleRate, 90.0, 0.8, dbToGain (gainDb.getTarget()));
        for (auto& f : shelf)
            f->setCoefficients (coefficients);
        dirtyShelf = false;
    }

    for (int i = 0; i < block.numFrames; ++i)
        for (int ch = 0; ch < (int) shelf.size() && ch < block.numChannels; ++ch)
            block.channelData[ch][i] = shelf[(size_t) ch]->processSingleSampleRaw (block.channelData[ch][i]);

    gainDb.next();   // keep the smoother advancing toward its target
}

// =============================================================================
// ClarityProcessor
// =============================================================================
void ClarityProcessor::prepare (const ProcessingContext& context)
{
    sampleRate = context.sampleRate;
    const auto channels = (size_t) juce::jmax (1, context.numChannels);
    presence.clear();
    air.clear();
    for (int ch = 0; ch < channels; ++ch)
    {
        presence.push_back (std::make_unique<juce::IIRFilter>());
        air.push_back (std::make_unique<juce::IIRFilter>());
    }
    presenceGainDb.setTimeConstant (60.0f, context.sampleRate);
    airGainDb.setTimeConstant (60.0f, context.sampleRate);
    presenceGainDb.reset (0.0f);
    airGainDb.reset (0.0f);
    dirty = true;
}

void ClarityProcessor::reset()
{
    for (auto& f : presence) f->reset();
    for (auto& f : air) f->reset();
}

void ClarityProcessor::setParameters (const ProcessingState& state)
{
    amountTarget = juce::jlimit (0.0f, 1.0f, state.clarityAmount);

    const float presenceTarget = amountTarget * 4.5f;   // controlled presence
    const float airTarget      = amountTarget * 2.0f;   // gentle, never harsh

    if (std::abs (presenceTarget - presenceGainDb.getTarget()) > 0.01f
        || std::abs (airTarget - airGainDb.getTarget()) > 0.01f)
    {
        presenceGainDb.setTarget (presenceTarget);
        airGainDb.setTarget (airTarget);
        dirty = true;
    }
}

void ClarityProcessor::process (AudioBlock& block)
{
    if (amountTarget <= 0.0f)
        return;

    if (dirty)
    {
        const auto presenceCoeffs = juce::IIRCoefficients::makePeakFilter (
            sampleRate, 3500.0, 0.9, dbToGain (presenceGainDb.getTarget()));
        const auto airCoeffs = juce::IIRCoefficients::makeHighShelf (
            sampleRate, 9000.0, 0.8, dbToGain (airGainDb.getTarget()));

        for (size_t ch = 0; ch < presence.size(); ++ch)
        {
            presence[ch]->setCoefficients (presenceCoeffs);
            air[ch]->setCoefficients (airCoeffs);
        }
        dirty = false;
    }

    for (int i = 0; i < block.numFrames; ++i)
        for (int ch = 0; ch < (int) presence.size() && ch < block.numChannels; ++ch)
        {
            float x = presence[(size_t) ch]->processSingleSampleRaw (block.channelData[ch][i]);
            block.channelData[ch][i] = air[(size_t) ch]->processSingleSampleRaw (x);
        }

    presenceGainDb.next();
    airGainDb.next();
}

// =============================================================================
// StereoWidthProcessor
// =============================================================================
void StereoWidthProcessor::prepare (const ProcessingContext& context)
{
    widthFactor.setTimeConstant (50.0f, context.sampleRate);
    widthFactor.reset (1.0f);   // 1.0 == normal stereo
}

void StereoWidthProcessor::reset() {}

void StereoWidthProcessor::setParameters (const ProcessingState& state)
{
    // state.stereoWidth: 0..1 where 0.5 is normal. Factor maps to 0..2 side gain.
    widthFactor.setTarget (juce::jlimit (0.0f, 1.0f, state.stereoWidth) * 2.0f);
}

void StereoWidthProcessor::process (AudioBlock& block)
{
    if (block.numChannels < 2)
        return;   // mono must remain mono — untouched by definition

    for (int i = 0; i < block.numFrames; ++i)
    {
        const float factor = widthFactor.next();
        if (juce::approximatelyEqual (factor, 1.0f))
            continue;

        const float left  = block.channelData[0][i];
        const float right = block.channelData[1][i];
        const float mid   = 0.5f * (left + right);
        const float side  = 0.5f * (left - right) * factor;

        block.channelData[0][i] = mid + side;
        block.channelData[1][i] = mid - side;
    }
}

// =============================================================================
// EqProcessor (migrated from M5 DspChain — same bands/coefficients/Q)
// =============================================================================
namespace
{
/** Filters are created lazily so prepare()'s value-init arrays stay cheap. */
juce::IIRFilter* ensureFilter (std::unique_ptr<juce::IIRFilter>& f)
{
    if (f == nullptr)
        f = std::make_unique<juce::IIRFilter>();
    return f.get();
}
} // namespace

void EqProcessor::prepare (const ProcessingContext& context)
{
    sampleRate = context.sampleRate;
    numChannelsPrepared = juce::jmax (1, context.numChannels);
    filters.assign ((size_t) numChannelsPrepared, {});
    dirty = true;
}

void EqProcessor::reset()
{
    for (auto& channelFilters : filters)
        for (auto& f : channelFilters)
            if (f != nullptr)
                f->reset();
}

void EqProcessor::setParameters (const ProcessingState& state)
{
    for (int band = 0; band < 5; ++band)
        if (std::abs (state.eq.gainsDb[band] - appliedGains[band]) > 0.001f)
        {
            dirty = true;
            break;
        }
    pendingState = state;
}

void EqProcessor::rebuildCoefficients()
{
    for (auto& channelFilters : filters)
    {
        ensureFilter (channelFilters[0])->setCoefficients (juce::IIRCoefficients::makeLowShelf (
            sampleRate, EqParams::frequencies[0], EqParams::q, dbToGain (pendingState.eq.gainsDb[0])));
        for (int band = 1; band <= 3; ++band)
            ensureFilter (channelFilters[band])->setCoefficients (juce::IIRCoefficients::makePeakFilter (
                sampleRate, EqParams::frequencies[band], EqParams::q,
                dbToGain (pendingState.eq.gainsDb[band])));
        ensureFilter (channelFilters[4])->setCoefficients (juce::IIRCoefficients::makeHighShelf (
            sampleRate, EqParams::frequencies[4], EqParams::q, dbToGain (pendingState.eq.gainsDb[4])));
    }

    for (int band = 0; band < 5; ++band)
        appliedGains[band] = pendingState.eq.gainsDb[band];

    dirty = false;
}

void EqProcessor::process (AudioBlock& block)
{
    if (dirty)
        rebuildCoefficients();

    for (int ch = 0; ch < block.numChannels && ch < numChannelsPrepared; ++ch)
    {
        auto& channelFilters = filters[(size_t) ch];
        for (int i = 0; i < block.numFrames; ++i)
            for (auto& f : channelFilters)
                block.channelData[ch][i] = f->processSingleSampleRaw (block.channelData[ch][i]);
    }
}

// =============================================================================
// CompressorProcessor (migrated: linked detector, hard knee, smoothed gain)
// =============================================================================
void CompressorProcessor::prepare (const ProcessingContext& context)
{
    preparedSampleRate = context.sampleRate;
    envelope = 0.0f;
    gainSmoothed = 1.0f;
}

void CompressorProcessor::reset()
{
    envelope = 0.0f;
    gainSmoothed = 1.0f;
}

void CompressorProcessor::setParameters (const ProcessingState& state)
{
    params = state;
}

void CompressorProcessor::process (AudioBlock& block)
{
    if (! params.compressor.enabled)
        return;

    const float makeup = dbToGain (params.compressor.makeupGainDb);

    for (int i = 0; i < block.numFrames; ++i)
    {
        // Linked peak across channels.
        float linkedPeak = 0.0f;
        for (int ch = 0; ch < block.numChannels; ++ch)
            linkedPeak = std::max (linkedPeak, std::abs (block.channelData[ch][i]));

        const float attackC  = coefficientFor (params.compressor.attackMs, preparedSampleRate);
        const float releaseC = coefficientFor (params.compressor.releaseMs, preparedSampleRate);
        envelope = linkedPeak > envelope
            ? linkedPeak + (envelope - linkedPeak) * attackC
            : linkedPeak + (envelope - linkedPeak) * releaseC;

        const float thresholdLin = dbToGain (params.compressor.thresholdDb);
        float targetGain = 1.0f;
        if (envelope > thresholdLin && envelope > 0.0f)
        {
            const float overDb    = 20.0f * std::log10 (envelope / thresholdLin);
            const float outOverDb = overDb / juce::jmax (1.2f, params.compressor.ratio);
            targetGain = dbToGain (outOverDb - overDb);
        }

        gainSmoothed += (targetGain - gainSmoothed) * 0.2f;

        for (int ch = 0; ch < block.numChannels; ++ch)
            block.channelData[ch][i] *= gainSmoothed * makeup;
    }
}

// =============================================================================
// LimiterProcessor (migrated: instant attack, exponential release)
// =============================================================================
void LimiterProcessor::prepare (const ProcessingContext& context) { preparedSampleRate = context.sampleRate; gain = 1.0f; }
void LimiterProcessor::reset()                            { gain = 1.0f; }

void LimiterProcessor::setParameters (const ProcessingState& state)
{
    params = state;
}

void LimiterProcessor::process (AudioBlock& block)
{
    if (! params.limiter.enabled || ! params.limiterEngaged)
        return;

    const float ceilingLin = dbToGain (params.limiter.ceilingDb);
    const float releaseCoef = coefficientFor (params.limiter.releaseMs, preparedSampleRate);
    float minGainThisBlock = 1.0f;

    for (int i = 0; i < block.numFrames; ++i)
    {
        float linkedPeak = 0.0f;
        for (int ch = 0; ch < block.numChannels; ++ch)
            linkedPeak = std::max (linkedPeak, std::abs (block.channelData[ch][i]));

        const float target = linkedPeak > ceilingLin
            ? ceilingLin / std::max (linkedPeak, 1.0e-9f) : 1.0f;

        gain = target < gain ? target : target + (gain - target) * releaseCoef;
        minGainThisBlock = std::min (minGainThisBlock, gain);

        for (int ch = 0; ch < block.numChannels; ++ch)
            block.channelData[ch][i] *= gain;
    }

    // Gain reduction in dB: how far below unity the limiter is working.
    if (minGainThisBlock < 0.999f)
        meters.limiterReductionDb.store (
            -20.0f * std::log10 (std::max (minGainThisBlock, 1.0e-4f)),
            std::memory_order_relaxed);
}

// =============================================================================
// NoiseReductionProcessor (migrated: high-pass + downward expander, per-channel)
// =============================================================================
NoiseReductionProcessor::NrTuning
NoiseReductionProcessor::tuningFor (NoiseReductionMode mode, float strength)
{
    const float s = juce::jlimit (0.0f, 1.0f, strength);

    switch (mode)
    {
        case NoiseReductionMode::gentle:
            return { -52.0f + 4.0f * s, juce::jmap (s, 6.0f, 14.0f), 15.0f, 260.0f };
        case NoiseReductionMode::strong:
            return { -46.0f + 4.0f * s, juce::jmap (s, 12.0f, 24.0f), 8.0f, 180.0f };
        case NoiseReductionMode::off:
            break;
    }
    return { -120.0f, 0.0f, 10.0f, 200.0f };
}

void NoiseReductionProcessor::prepare (const ProcessingContext& context)
{
    const auto channels = (size_t) juce::jmax (1, context.numChannels);
    highPass.clear();
    for (size_t ch = 0; ch < channels; ++ch)
        highPass.push_back (std::make_unique<juce::IIRFilter>());
    envelope.assign (channels, 0.0f);
    gain.assign (channels, 1.0f);
    highPassDirty = true;
    sampleRateStored = context.sampleRate;
}

void NoiseReductionProcessor::reset()
{
    for (auto& f : highPass) f->reset();
    std::fill (envelope.begin(), envelope.end(), 0.0f);
    std::fill (gain.begin(), gain.end(), 1.0f);
}

void NoiseReductionProcessor::setParameters (const ProcessingState& state)
{
    params = state;
}

void NoiseReductionProcessor::process (AudioBlock& block)
{
    if (params.noiseReduction.mode == NoiseReductionMode::off)
        return;

    if (highPassDirty)
    {
        const auto coeffs = juce::IIRCoefficients::makeHighPass (sampleRateStored, 85.0, 0.7);
        for (auto& f : highPass)
            f->setCoefficients (coeffs);
        highPassDirty = false;
    }

    const auto tuning = tuningFor (params.noiseReduction.mode, params.noiseReduction.strength);
    const float thresholdLin = dbToGain (tuning.thresholdDb);
    const float floorGain    = dbToGain (-tuning.depthDb);

    for (int ch = 0; ch < block.numChannels && ch < (int) highPass.size(); ++ch)
    {
        const float smoothingUp   = 1.0f - coefficientFor (tuning.attackMs, sampleRateStored);
        const float smoothingDown = 1.0f - coefficientFor (tuning.releaseMs, sampleRateStored);

        for (int i = 0; i < block.numFrames; ++i)
        {
            float x = highPass[(size_t) ch]->processSingleSampleRaw (block.channelData[ch][i]);

            const float absX = std::abs (x);
            envelope[(size_t) ch] = std::max (absX,
                envelope[(size_t) ch] * coefficientFor (tuning.releaseMs, sampleRateStored));

            const float targetGain = envelope[(size_t) ch] >= thresholdLin ? 1.0f : floorGain;
            const float amount = targetGain > gain[(size_t) ch] ? smoothingUp : smoothingDown;
            gain[(size_t) ch] += (targetGain - gain[(size_t) ch]) * amount;

            block.channelData[ch][i] = x * gain[(size_t) ch];
        }
    }
}

// =============================================================================
// MeterProcessor
// =============================================================================
void MeterProcessor::prepare (const ProcessingContext& context) { sampleRate = context.sampleRate; }
void MeterProcessor::reset()                                    { holdCounter = 0; }

void MeterProcessor::process (AudioBlock& block)
{
    float peak = 0.0f;
    double sumSquares = 0.0;
    long count = 0;

    for (int ch = 0; ch < block.numChannels; ++ch)
        for (int i = 0; i < block.numFrames; ++i)
        {
            const float s = block.channelData[ch][i];
            peak = std::max (peak, std::abs (s));
            sumSquares += (double) s * s;
            ++count;
        }

    const float rms = count > 0 ? (float) std::sqrt (sumSquares / (double) count) : 0.0f;

    meters.peak.store (peak, std::memory_order_relaxed);
    meters.rms.store (rms, std::memory_order_relaxed);
}

} // namespace otoha::dsp
