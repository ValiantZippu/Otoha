/*
    DspEngineTests — headless verification of the M5 audio processing:
    deterministic synthetic signals (sine, silence, noise, impulse), no
    listening required for regression detection.
*/
#include "../Source/Dsp/DspChain.h"
#include "../Source/Dsp/Presets.h"

#include <cmath>
#include <cstdio>
#include <vector>

using otoha::DspChain;
using otoha::ProcessingState;

namespace
{
bool expect (bool condition, const char* message)
{
    if (! condition)
        std::printf ("FAIL: %s\n", message);
    return condition;
}

/** Deterministic LCG white noise — same sequence on every run/platform path. */
float pseudoNoise (unsigned& seed)
{
    seed = seed * 1664525u + 1013904223u;
    return ((float) ((seed >> 8) & 0xFFFF) / 32768.0f) - 1.0f;
}

std::vector<float> makeSine (int frames, double freqHz, double sampleRate, float amplitude)
{
    std::vector<float> v ((size_t) frames);
    for (int i = 0; i < frames; ++i)
        v[(size_t) i] = amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freqHz * i / sampleRate);
    return v;
}

std::vector<float> makeNoise (int frames, float amplitude)
{
    unsigned seed = 12345u;
    std::vector<float> v ((size_t) frames);
    for (int i = 0; i < frames; ++i)
        v[(size_t) i] = amplitude * pseudoNoise (seed);
    return v;
}

/** Runs one mono chain pass, returning the processed samples. */
std::vector<float> runChain (DspChain& chain, const std::vector<float>& input)
{
    auto work = input;
    float* ch[1] = { work.data() };
    chain.process (ch, (int) work.size());
    return work;
}

float peakOf (const std::vector<float>& v)
{
    float p = 0.0f;
    for (float s : v) p = std::max (p, std::abs (s));
    return p;
}

float rmsOf (const std::vector<float>& v)
{
    double sum = 0.0;
    for (float s : v) sum += (double) s * s;
    return (float) std::sqrt (sum / (double) juce::jmax (1, (int) v.size()));
}

bool allFinite (const std::vector<float>& v)
{
    for (float s : v)
        if (! std::isfinite (s)) return false;
    return true;
}
} // namespace

int main()
{
    bool ok = true;
    constexpr double rate = 48000.0;

    // --- bypass: disabled chain must be a perfect passthrough ------------------
    {
        ProcessingState off;   // enabled = false
        DspChain chain;
        chain.prepare (rate, 1);
        chain.setParameters (off);

        const auto in = makeSine (48000, 220.0, rate, 0.5f);
        const auto out = runChain (chain, in);
        ok &= expect (out == in, "bypass leaves samples bit-identical");
    }

    // --- EQ ---------------------------------------------------------------------
    {
        ProcessingState eqOn;
        eqOn.enabled = true;
        eqOn.noiseReduction.mode = otoha::NoiseReductionMode::off;
        eqOn.compressor.enabled = false;
        eqOn.limiter.enabled = false;

        // Neutral gains: sine amplitude preserved within tolerance after settle.
        {
            DspChain chain;
            chain.prepare (rate, 1);
            chain.setParameters (eqOn);

            auto in = makeSine (48000, 1000.0, rate, 0.25f);
            auto out = runChain (chain, in);
            const auto tailOut = std::vector<float> (out.end() - 8000, out.end());
            ok &= expect (std::abs (rmsOf (tailOut) - 0.25f * 0.7071f) < 0.01f,
                          "neutral EQ keeps a 1 kHz sine at unity");
            ok &= expect (allFinite (out), "neutral EQ output is finite");
        }

        // Treble boost raises a high sine's level.
        {
            auto boosted = eqOn;
            boosted.eq.gainsDb[4] = 12.0f;
            DspChain chain;
            chain.prepare (rate, 1);
            chain.setParameters (boosted);

            auto in = makeSine (48000, 9000.0, rate, 0.2f);
            auto out = runChain (chain, in);
            const auto tailOut = std::vector<float> (out.end() - 8000, out.end());
            ok &= expect (rmsOf (tailOut) > 0.2f * 0.7071f * 2.0f,
                          "+12 dB shelf audibly boosts the high band");
            ok &= expect (allFinite (out), "boosted EQ output is finite");
        }
    }

    // --- compressor ---------------------------------------------------------------
    {
        auto compOn = [] { ProcessingState s; s.enabled = true;
                           s.compressor.enabled = true; s.limiter.enabled = false;
                           s.compressor.thresholdDb = -20.0f; s.compressor.ratio = 4.0f;
                           s.compressor.makeupGainDb = 0.0f; return s; } ();

        // Loud signal above threshold gets reduced.
        DspChain loud;
        loud.prepare (rate, 1);
        loud.setParameters (compOn);
        auto loudOut = runChain (loud, makeSine (48000, 220.0, rate, 0.9f));
        const auto loudTail = std::vector<float> (loudOut.end() - 16000, loudOut.end());
        ok &= expect (peakOf (loudTail) < 0.75f,
                      "compressor reduces a signal well above threshold");

        // Quiet signal below threshold passes essentially untouched.
        DspChain quiet;
        quiet.prepare (rate, 1);
        quiet.setParameters (compOn);
        auto quietOut = runChain (quiet, makeSine (48000, 220.0, rate, 0.05f));
        const auto quietTail = std::vector<float> (quietOut.end() - 16000, quietOut.end());
        ok &= expect (std::abs (peakOf (quietTail) - 0.05f) < 0.006f,
                      "below-threshold signal is not compressed");

        // No runaway gain with silence in -> near-silence out.
        DspChain silent;
        silent.prepare (rate, 1);
        silent.setParameters (compOn);
        auto silenceOut = runChain (silent, std::vector<float> (48000, 0.0f));
        ok &= expect (peakOf (silenceOut) < 1.0e-6f, "silence through compressor stays silent");
        ok &= expect (allFinite (silenceOut), "compressor output finite on silence");
    }

    // --- limiter ----------------------------------------------------------------------
    {
        auto limOn = [] { ProcessingState s; s.enabled = true;
                          s.limiter.enabled = true; s.limiter.ceilingDb = -1.0f;
                          s.compressor.enabled = false; return s; } ();
        const float ceilingLin = std::pow (10.0f, -1.0f / 20.0f);

        DspChain chain;
        chain.prepare (rate, 1);
        chain.setParameters (limOn);

        // Loud noise bursts must never exceed the ceiling (within tolerance).
        auto noisy = makeNoise (48000, 2.0f);   // deliberately +6 dBFS hot
        auto out = runChain (chain, noisy);
        const auto tail = std::vector<float> (out.begin() + 4000, out.end());
        ok &= expect (peakOf (tail) <= ceilingLin * 1.05f,
                      "limiter holds output under the configured ceiling");
        ok &= expect (allFinite (out), "limiter output finite on overs");

        DspChain silenceChain;
        silenceChain.prepare (rate, 1);
        silenceChain.setParameters (limOn);
        auto silence = runChain (silenceChain, std::vector<float> (48000, 0.0f));
        ok &= expect (peakOf (silence) == 0.0f, "silence stays exactly silent through the limiter");
    }

    // --- noise reduction -----------------------------------------------------------------
    {
        // Stability: pure noise and pure silence produce finite output.
        ProcessingState nrGentle;
        nrGentle.enabled = true;
        nrGentle.noiseReduction.mode = otoha::NoiseReductionMode::gentle;
        nrGentle.noiseReduction.strength = 0.5f;

        DspChain chain;
        chain.prepare (rate, 1);
        chain.setParameters (nrGentle);
        auto noiseOut = runChain (chain, makeNoise (96000, 0.004f));  // low-level hiss
        ok &= expect (allFinite (noiseOut), "noise reduction stable on noise");

        DspChain silenceChain;
        silenceChain.prepare (rate, 1);
        silenceChain.setParameters (nrGentle);
        auto silenceOut = runChain (silenceChain, std::vector<float> (96000, 0.0f));
        ok &= expect (allFinite (silenceOut), "noise reduction stable on silence");
        ok &= expect (peakOf (silenceOut) < 1.0e-4f, "expander does not invent sound from silence");

        // Stronger setting reduces low-level noise more than gentle.
        auto measureRms = [&] (otoha::NoiseReductionMode mode)
        {
            ProcessingState s = nrGentle;
            s.noiseReduction.mode = mode;
            s.noiseReduction.strength = mode == otoha::NoiseReductionMode::strong ? 1.0f : 0.5f;

            DspChain c;
            c.prepare (rate, 1);
            c.setParameters (s);
            auto out = runChain (c, makeNoise (96000, 0.004f));
            return rmsOf (std::vector<float> (out.end() - 48000, out.end()));   // settled portion
        };

        const auto gentleRms = measureRms (otoha::NoiseReductionMode::gentle);
        const auto strongRms = measureRms (otoha::NoiseReductionMode::strong);
        ok &= expect (strongRms < gentleRms * 0.95f,
                      "strong noise reduction ducks steady noise more than gentle");
    }

    // --- chain order & determinism ---------------------------------------------------------
    {
        auto state = otoha::presetToState (otoha::DspPreset::voice);
        state.enabled = true;

        DspChain a, b;
        a.prepare (rate, 1); b.prepare (rate, 1);
        a.setParameters (state); b.setParameters (state);

        const auto in = makeNoise (48000, 0.3f);
        const auto outA = runChain (a, in);
        const auto outB = runChain (b, in);

        ok &= expect (outA == outB, "identical state+input gives identical output (deterministic)");
        ok &= expect (allFinite (outA), "full voice-chain output is finite");
        ok &= expect (peakOf (outA) <= 1.0f, "voice chain never clips past full scale");
    }

    // --- stereo linked processing ------------------------------------------------------------
    {
        auto state = otoha::presetToState (otoha::DspPreset::natural);
        state.enabled = true;

        DspChain chain;
        chain.prepare (rate, 2);
        chain.setParameters (state);

        std::vector<float> left (48000), right (48000);
        for (int i = 0; i < 48000; ++i)
        {
            left[(size_t) i]  = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 300.0 * i / rate);
            right[(size_t) i] = 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 500.0 * i / rate);
        }
        float* chs[2] = { left.data(), right.data() };
        chain.process (chs, 48000);

        ok &= expect (allFinite (left) && allFinite (right), "stereo chain output finite");
        ok &= expect (peakOf (left) > 0.01f && peakOf (right) > 0.01f,
                      "stereo channels both survive processing (no collapse)");
    }

    // --- every preset produces sane output ------------------------------------------------------
    {
        for (int p = 1; p <= 7; ++p)   // skip 'off' (= bypass identity, covered above)
        {
            const auto preset = (otoha::DspPreset) p;
            auto state = otoha::presetToState (preset);

            DspChain chain;
            chain.prepare (44100.0, 1);   // also exercises a second sample rate
            chain.setParameters (state);

            auto out = runChain (chain, makeSine (22050, 440.0, 44100.0, 0.4f));
            ok &= expect (allFinite (out), (otoha::presetToString (preset) + ": finite at 44.1 kHz").toRawUTF8());
        }
    }

    if (! ok) return 1;
    std::printf ("PASS: dsp engine\n");
    return 0;
}
