#pragma once

#include <juce_core/juce_core.h>

/*
    ProcessingState — everything the DSP chain needs, as plain data.

    UI never touches DSP code directly:  UI -> ProcessingState -> DspChain.
    This struct is trivially copyable enough to cross to the audio path and
    serializes into the editor sidecar so Enhance settings survive reopening.
*/

namespace otoha
{
enum class NoiseReductionMode { off, gentle, strong };

inline juce::String noiseReductionToString (NoiseReductionMode m)
{
    switch (m)
    {
        case NoiseReductionMode::gentle: return "gentle";
        case NoiseReductionMode::strong: return "strong";
        case NoiseReductionMode::off:    break;
    }
    return "off";
}

inline NoiseReductionMode noiseReductionFromString (const juce::String& s)
{
    return s.equalsIgnoreCase ("gentle") ? NoiseReductionMode::gentle
         : s.equalsIgnoreCase ("strong") ? NoiseReductionMode::strong
                                         : NoiseReductionMode::off;
}

// Five bands: low shelf / low-mid / mid / high-mid / high shelf.
struct EqParams
{
    float gainsDb[5] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };   // neutral by design
    static constexpr float frequencies[5] = { 90.0f, 280.0f, 1000.0f, 3500.0f, 9000.0f };
    static constexpr float q = 0.9f;
};

struct CompressorParams
{
    bool     enabled     = false;
    float    thresholdDb = -24.0f;
    float    ratio       = 2.0f;
    float    attackMs    = 15.0f;
    float    releaseMs   = 180.0f;
    float    makeupGainDb = 0.0f;
};

struct LimiterParams
{
    bool  enabled  = true;
    float ceilingDb = -1.0f;    // conservative output protection
    float releaseMs = 60.0f;
};

struct NoiseReductionParams
{
    NoiseReductionMode mode = NoiseReductionMode::off;
    float strength = 0.5f;      // 0..1, scales expansion depth within the mode
};

struct ProcessingState
{
    bool enabled = false;                 // master bypass (Original <-> Enhanced)
    bool limiterEngaged = true;           // kept even when other modules are off

    EqParams              eq;
    CompressorParams      compressor;
    LimiterParams         limiter;
    NoiseReductionParams  noiseReduction;

    // --- M7 additions (neutral defaults: enabling DSP never colors audio) ----
    float bassAmount   = 0.0f;    // 0..1, low-shelf lift up to +6 dB @ 90 Hz
    float clarityAmount = 0.0f;   // 0..1, presence lift up to +4.5 dB @ 3.5 kHz
    float stereoWidth  = 0.5f;    // 0..1 where 0.5 == normal stereo
    float inputGainDb  = 0.0f;
    float outputGainDb = 0.0f;

    // --- serialization -------------------------------------------------------
    juce::var toJSON() const
    {
        auto* root = new juce::DynamicObject();
        root->setProperty ("enabled", enabled);

        auto* eqObj = new juce::DynamicObject();
        juce::Array<juce::var> gains;
        for (float g : eq.gainsDb) gains.add ((double) g);
        eqObj->setProperty ("gains", gains);
        root->setProperty ("eq", juce::var (eqObj));

        auto* compObj = new juce::DynamicObject();
        compObj->setProperty ("enabled", compressor.enabled);
        compObj->setProperty ("thresholdDb", (double) compressor.thresholdDb);
        compObj->setProperty ("ratio", (double) compressor.ratio);
        compObj->setProperty ("attackMs", (double) compressor.attackMs);
        compObj->setProperty ("releaseMs", (double) compressor.releaseMs);
        compObj->setProperty ("makeupGainDb", (double) compressor.makeupGainDb);
        root->setProperty ("compressor", juce::var (compObj));

        auto* limObj = new juce::DynamicObject();
        limObj->setProperty ("enabled", limiter.enabled);
        limObj->setProperty ("ceilingDb", (double) limiter.ceilingDb);
        limObj->setProperty ("releaseMs", (double) limiter.releaseMs);
        root->setProperty ("limiter", juce::var (limObj));

        auto* nrObj = new juce::DynamicObject();
        nrObj->setProperty ("mode", noiseReductionToString (noiseReduction.mode));
        nrObj->setProperty ("strength", (double) noiseReduction.strength);
        root->setProperty ("noiseReduction", juce::var (nrObj));

        root->setProperty ("bassAmount", (double) bassAmount);
        root->setProperty ("clarityAmount", (double) clarityAmount);
        root->setProperty ("stereoWidth", (double) stereoWidth);
        root->setProperty ("inputGainDb", (double) inputGainDb);
        root->setProperty ("outputGainDb", (double) outputGainDb);

        return juce::var (root);
    }

    static ProcessingState fromJSON (const juce::var& v)
    {
        ProcessingState s;   // defaults survive any missing field

        if (v.isVoid()) return s;

        s.enabled = (bool) (int) v.getProperty ("enabled", 0);

        const auto eqVar = v.getProperty ("eq", {});
        if (const auto* gains = eqVar.getProperty ("gains", {}).getArray())
            for (int i = 0; i < juce::jmin (5, gains->size()); ++i)
                s.eq.gainsDb[i] = (float) (double) gains->getReference (i);

        const auto compVar = v.getProperty ("compressor", {});
        s.compressor.enabled      = (bool) (int) compVar.getProperty ("enabled", 0);
        s.compressor.thresholdDb  = (float) (double) compVar.getProperty ("thresholdDb", (double) s.compressor.thresholdDb);
        s.compressor.ratio        = (float) (double) compVar.getProperty ("ratio", (double) s.compressor.ratio);
        s.compressor.attackMs     = (float) (double) compVar.getProperty ("attackMs", (double) s.compressor.attackMs);
        s.compressor.releaseMs    = (float) (double) compVar.getProperty ("releaseMs", (double) s.compressor.releaseMs);
        s.compressor.makeupGainDb = (float) (double) compVar.getProperty ("makeupGainDb", (double) s.compressor.makeupGainDb);

        const auto limVar = v.getProperty ("limiter", {});
        s.limiter.enabled   = (bool) (int) limVar.getProperty ("enabled", 1);
        s.limiter.ceilingDb = (float) (double) limVar.getProperty ("ceilingDb", (double) s.limiter.ceilingDb);
        s.limiter.releaseMs = (float) (double) limVar.getProperty ("releaseMs", (double) s.limiter.releaseMs);

        const auto nrVar = v.getProperty ("noiseReduction", {});
        s.noiseReduction.mode     = noiseReductionFromString (nrVar.getProperty ("mode", "off").toString());
        s.noiseReduction.strength = (float) (double) nrVar.getProperty ("strength", 0.5);

        // M7 fields default to neutral when absent (older sidecars stay valid).
        s.bassAmount    = (float) (double) v.getProperty ("bassAmount", 0.0);
        s.clarityAmount = (float) (double) v.getProperty ("clarityAmount", 0.0);
        s.stereoWidth   = (float) (double) v.getProperty ("stereoWidth", 0.5);
        s.inputGainDb   = (float) (double) v.getProperty ("inputGainDb", 0.0);
        s.outputGainDb  = (float) (double) v.getProperty ("outputGainDb", 0.0);

        return s;
    }
};
} // namespace otoha
