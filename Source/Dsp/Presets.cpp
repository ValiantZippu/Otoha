#include "Presets.h"

#include <cmath>

namespace otoha
{
juce::Array<DspPreset> allDspPresets()
{
    return { DspPreset::off, DspPreset::natural, DspPreset::voice, DspPreset::vocal,
             DspPreset::music, DspPreset::acoustic, DspPreset::live, DspPreset::podcast,
             DspPreset::bass, DspPreset::clarity };
}

juce::String presetToString (DspPreset p)
{
    switch (p)
    {
        case DspPreset::off:      return "Off";
        case DspPreset::natural:  return "Natural";
        case DspPreset::voice:    return "Voice";
        case DspPreset::vocal:    return "Vocal";
        case DspPreset::music:    return "Music";
        case DspPreset::acoustic: return "Acoustic";
        case DspPreset::live:     return "Live";
        case DspPreset::podcast:  return "Podcast";
        case DspPreset::bass:     return "Bass";
        case DspPreset::clarity:  return "Clarity";
    }
    return "Off";
}

DspPreset presetFromString (const juce::String& s)
{
    if (s.equalsIgnoreCase ("natural"))  return DspPreset::natural;
    if (s.equalsIgnoreCase ("voice"))    return DspPreset::voice;
    if (s.equalsIgnoreCase ("vocal"))    return DspPreset::vocal;
    if (s.equalsIgnoreCase ("music"))    return DspPreset::music;
    if (s.equalsIgnoreCase ("acoustic")) return DspPreset::acoustic;
    if (s.equalsIgnoreCase ("live"))     return DspPreset::live;
    if (s.equalsIgnoreCase ("podcast"))  return DspPreset::podcast;
    if (s.equalsIgnoreCase ("bass"))     return DspPreset::bass;
    if (s.equalsIgnoreCase ("clarity"))  return DspPreset::clarity;
    return DspPreset::off;
}

namespace
{
ProcessingState baseEnhance()
{
    ProcessingState s;
    s.enabled          = true;
    s.limiterEngaged   = true;
    s.limiter.enabled  = true;
    s.limiter.ceilingDb = -1.0f;
    s.limiter.releaseMs = 60.0f;
    return s;
}
} // namespace

ProcessingState presetToState (DspPreset p)
{
    // All values intentionally conservative; tune by LISTENING, not theory.
    switch (p)
    {
        case DspPreset::off:
        {
            ProcessingState s;   // neutral + bypassed: EQ flat, everything off
            s.enabled = false;
            s.noiseReduction.mode = NoiseReductionMode::off;
            return s;
        }

        case DspPreset::natural:
        {
            auto s = baseEnhance();
            s.eq.gainsDb[0] = -1.0f;              // tiny rumble trim
            s.compressor.enabled      = true;
            s.compressor.thresholdDb  = -18.0f;   // barely-there glue
            s.compressor.ratio        = 1.5f;
            s.compressor.attackMs     = 20.0f;
            s.compressor.releaseMs    = 220.0f;
            s.noiseReduction.mode     = NoiseReductionMode::gentle;
            s.noiseReduction.strength = 0.35f;
            return s;
        }

        case DspPreset::voice:
        {
            auto s = baseEnhance();
            s.eq.gainsDb[0] = -3.0f;              // cut rumble/handling noise
            s.eq.gainsDb[2] = 1.5f;               // presence around 1 kHz
            s.eq.gainsDb[3] = 2.0f;               // intelligibility 3.5 kHz
            s.compressor.enabled      = true;
            s.compressor.thresholdDb  = -22.0f;
            s.compressor.ratio        = 2.5f;
            s.compressor.attackMs     = 10.0f;
            s.compressor.releaseMs    = 160.0f;
            s.compressor.makeupGainDb = 2.0f;
            s.noiseReduction.mode     = NoiseReductionMode::gentle;
            s.noiseReduction.strength = 0.5f;
            return s;
        }

        case DspPreset::vocal:
        {
            auto s = baseEnhance();
            s.eq.gainsDb[0] = -4.0f;
            s.eq.gainsDb[1] = -1.0f;              // clear some boxiness
            s.eq.gainsDb[2] = 2.0f;
            s.eq.gainsDb[3] = 2.5f;
            s.eq.gainsDb[4] = 1.0f;               // a little air
            s.compressor.enabled      = true;
            s.compressor.thresholdDb  = -24.0f;
            s.compressor.ratio        = 3.0f;
            s.compressor.attackMs     = 8.0f;
            s.compressor.releaseMs    = 140.0f;
            s.compressor.makeupGainDb = 3.0f;
            s.noiseReduction.mode     = NoiseReductionMode::gentle;
            s.noiseReduction.strength = 0.55f;
            return s;
        }

        case DspPreset::music:
        {
            auto s = baseEnhance();
            s.compressor.enabled      = true;
            s.compressor.thresholdDb  = -14.0f;   // dynamics preserved
            s.compressor.ratio        = 1.6f;
            s.compressor.attackMs     = 25.0f;    // transients through
            s.compressor.releaseMs    = 260.0f;
            s.noiseReduction.mode     = NoiseReductionMode::off;   // never mangle music beds
            return s;
        }

        case DspPreset::acoustic:
        {
            auto s = baseEnhance();
            s.eq.gainsDb[0] = -1.5f;
            s.eq.gainsDb[3] = -1.0f;              // soften harshness
            s.compressor.enabled      = true;
            s.compressor.thresholdDb  = -16.0f;
            s.compressor.ratio        = 1.8f;
            s.compressor.attackMs     = 30.0f;    // natural transients first
            s.compressor.releaseMs    = 280.0f;
            s.noiseReduction.mode     = NoiseReductionMode::gentle;
            s.noiseReduction.strength = 0.3f;
            return s;
        }

        case DspPreset::live:
        {
            auto s = baseEnhance();
            s.eq.gainsDb[0] = -3.0f;              // room rumble / crowd low end
            s.eq.gainsDb[2] = 1.5f;
            s.compressor.enabled      = true;
            s.compressor.thresholdDb  = -20.0f;
            s.compressor.ratio        = 2.8f;     // level consistency in messy rooms
            s.compressor.attackMs     = 12.0f;
            s.compressor.releaseMs    = 150.0f;
            s.noiseReduction.mode     = NoiseReductionMode::gentle;   // moderate only — NR can't fix crowds
            s.noiseReduction.strength = 0.45f;
            return s;
        }

        case DspPreset::podcast:
        {
            auto s = baseEnhance();
            s.eq.gainsDb[0] = -3.5f;
            s.eq.gainsDb[1] = -1.0f;
            s.eq.gainsDb[2] = 1.5f;
            s.eq.gainsDb[3] = 2.5f;
            s.compressor.enabled      = true;
            s.compressor.thresholdDb  = -23.0f;   // consistent spoken level
            s.compressor.ratio        = 3.2f;
            s.compressor.attackMs     = 8.0f;
            s.compressor.releaseMs    = 130.0f;
            s.compressor.makeupGainDb = 3.0f;
            s.noiseReduction.mode     = NoiseReductionMode::gentle;
            s.noiseReduction.strength = 0.55f;
            return s;
        }

        // --- M8: Otoha Sound presets --------------------------------------------
        // Deliberately minimal: single-purpose tonal characters for live playback.
        case DspPreset::bass:
        {
            auto s = baseEnhance();
            s.bassAmount    = 0.7f;               // gain-staged lift, limiter protects
            s.eq.gainsDb[0] = 1.0f;               // gentle shelf support
            s.stereoWidth   = 0.5f;               // untouched imaging
            return s;
        }

        case DspPreset::clarity:
        {
            auto s = baseEnhance();
            s.clarityAmount = 0.65f;              // presence without harshness
            s.eq.gainsDb[4] = 1.0f;               // slight air
            s.compressor.enabled      = true;
            s.compressor.thresholdDb  = -18.0f;
            s.compressor.ratio        = 1.6f;
            s.compressor.attackMs     = 20.0f;
            s.compressor.releaseMs    = 220.0f;
            return s;
        }
    }
    return baseEnhance();
}

bool stateDiffersFromPreset (const ProcessingState& state, DspPreset p)
{
    const auto canonical = presetToState (p);

    if (state.enabled                 != canonical.enabled
        || state.limiterEngaged       != canonical.limiterEngaged
        || state.compressor.enabled   != canonical.compressor.enabled
        || std::abs (state.compressor.thresholdDb  - canonical.compressor.thresholdDb)  > 0.01f
        || std::abs (state.compressor.ratio        - canonical.compressor.ratio)        > 0.01f
        || std::abs (state.compressor.attackMs     - canonical.compressor.attackMs)     > 0.01f
        || std::abs (state.compressor.releaseMs    - canonical.compressor.releaseMs)    > 0.01f
        || std::abs (state.compressor.makeupGainDb - canonical.compressor.makeupGainDb) > 0.01f
        || state.limiter.enabled      != canonical.limiter.enabled
        || std::abs (state.limiter.ceilingDb - canonical.limiter.ceilingDb) > 0.01f
        || std::abs (state.limiter.releaseMs - canonical.limiter.releaseMs) > 0.01f
        || state.noiseReduction.mode  != canonical.noiseReduction.mode
        || std::abs (state.noiseReduction.strength - canonical.noiseReduction.strength) > 0.01f)
        return true;

    for (int i = 0; i < 5; ++i)
        if (std::abs (state.eq.gainsDb[i] - canonical.eq.gainsDb[i]) > 0.01f)
            return true;

    // M7 parameters participate in "(Modified)" detection too.
    return std::abs (state.bassAmount    - canonical.bassAmount)    > 0.005f
        || std::abs (state.clarityAmount - canonical.clarityAmount) > 0.005f
        || std::abs (state.stereoWidth   - canonical.stereoWidth)   > 0.005f
        || std::abs (state.inputGainDb   - canonical.inputGainDb)   > 0.01f
        || std::abs (state.outputGainDb  - canonical.outputGainDb)  > 0.01f;
}
} // namespace otoha
