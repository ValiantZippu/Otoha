/*
    SoundEngineTests — Milestone 8: Otoha Sound without hardware.

    Live-style pipeline (#45): generated 1 kHz -> MockAudioBackend ->
    SoundEngine (Bass/EQ/Limiter via the shared chain) -> captured output.
    Verifies processing happens, bypass restores passthrough, Enhance blends
    monotonically, nothing clips/NaNs — plus device-profile persistence.

    Windows WASAPI code paths are NOT exercised here (no hardware in CI);
    they carry their own documented status reporting instead of faking.
*/
#include "../Source/Dsp/Presets.h"
#include "../Source/Sound/SoundEngine.h"
#include "../Source/Sound/ProfileStorage.h"
#include "../Source/Platform/AudioBackend.h"
#include "../Source/Platform/MockAudioBackend.h"

#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace otoha;
using namespace otoha::dsp;
using namespace otoha::platform;

namespace
{
bool expect (bool condition, const char* message)
{
    if (! condition)
        std::printf ("FAIL: %s\n", message);
    return condition;
}

std::vector<float> sine1k (int frames, double rate, float amp)
{
    std::vector<float> v ((size_t) frames);
    for (int i = 0; i < frames; ++i)
        v[(size_t) i] = amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * i / rate);
    return v;
}

float peakOf (const std::vector<float>& v)
{
    float p = 0.0f;
    for (float x : v) p = std::max (p, std::abs (x));
    return p;
}
} // namespace

int main()
{
    bool ok = true;

    // ------------------------------------------------------------------
    // Live-style pipeline (#45): mock backend -> SoundEngine -> sink.
    // ------------------------------------------------------------------
    class Sink : public AudioOutputSink
    {
    public:
        void writeBlock (const AudioBlock& b) override
        {
            for (int i = 0; i < b.numFrames; ++i)
                samples.push_back (b.channelData[0][i]);
        }
        std::vector<float> samples;
    };

    auto runPipeline = [&] (bool engineOn, float enhance, int framesIn) -> std::vector<float>
    {
        MockAudioBackend backend;
        SoundEngine engine;
        Sink sink;

        ProcessingContext ctx { 48000.0, 2, 512 };
        engine.prepare (ctx);
        engine.setEnabled (engineOn);
        engine.setEnhanceAmount (enhance);

        auto preset = presetToState (DspPreset::bass);
        preset.enabled = true;
        engine.setParameters (preset);

        backend.initialize ({ 48000.0, 2, 512 });
        backend.setOutputSink (&sink);
        backend.setProcessStage ([&] (AudioBlock& b) { engine.process (b.channelData, b.numChannels, b.numFrames); });
        backend.start();

        auto l = sine1k (framesIn, 48000.0, 0.5f);
        std::vector<float> r ((size_t) framesIn, 0.0f);
        std::vector<float*> ptrs { l.data(), r.data() };

        // Deliver in 512-frame blocks like a real audio thread.
        for (int start = 0; start < framesIn; start += 512)
            backend.deliverBlock (ptrs.data(), juce::jmin (512, framesIn - start));

        backend.stop();
        return sink.samples;
    };

    // Warm-up + measurement window (smoothers need a moment to settle).
    constexpr int totalFrames = 48000;   // one second

    {
        const auto dry     = runPipeline (false, 1.0f, totalFrames);   // OFF: passthrough
        const auto fullWet = runPipeline (true,  1.0f, totalFrames);   // ON, Enhance 100%
        const auto zeroWet = runPipeline (true,  0.0f, totalFrames);   // ON, Enhance 0%

        ok &= expect (dry.size() == (size_t) totalFrames, "pipeline delivers every sample");
        ok &= expect (fullWet.size() == dry.size() && zeroWet.size() == dry.size(),
                      "all modes deliver identical sample counts");

        ok &= expect (std::abs (peakOf (dry) - 0.5f) < 0.02f,
                      "OFF mode is a clean passthrough");

        ok &= expect (std::abs (peakOf (zeroWet) - 0.5f) < 0.05f,
                      "Enhance at 0% is effectively neutral (wet fully faded)");

        const float bassPeak = peakOf (fullWet);
        ok &= expect (bassPeak > 0.5f && bassPeak <= 1.01f,
                      "Bass preset lifts level but limiter keeps it clean");
        ok &= expect (peakOf (fullWet) > peakOf (zeroWet),
                      "processing actually changes the signal when engaged");

        bool finite = true;
        for (float x : fullWet) if (! std::isfinite (x)) { finite = false; break; }
        ok &= expect (finite, "no NaN/Inf in processed output");
    }

    // ------------------------------------------------------------------
    // Enhance monotonicity: more enhance => further from passthrough.
    // ------------------------------------------------------------------
    {
        const float framesF = 12000;        auto drySignal = runPipeline (false, 1.0f, (int) framesF);

        auto distanceFromDry = [&] (float amount)
        {
            auto out = runPipeline (true, amount, (int) framesF);
            float d = 0.0f;
            for (size_t i = 0; i < out.size() && i < drySignal.size(); ++i)
                d += std::abs (out[i] - drySignal[i]);
            return d;
        };

        const float low = distanceFromDry (0.25f);
        const float high = distanceFromDry (1.0f);
        ok &= expect (high > low * 1.5f,
                      "higher Enhance moves further from passthrough");
    }

    // ------------------------------------------------------------------
    // Diagnostics counters exist and count something.
    // ------------------------------------------------------------------
    {
        MockAudioBackend backend;
        SoundEngine engine;
        ProcessingContext ctx { 48000.0, 2, 256 };
        engine.prepare (ctx);
        engine.setEnabled (true);

        std::vector<float> l (256), r (256);
        std::vector<float*> ptrs { l.data(), r.data() };
        for (int i = 0; i < 10; ++i)
            engine.process (ptrs.data(), 2, 256);

        const auto stats = engine.getStats();
        ok &= expect (stats.blocksProcessed == 10 && stats.blocksPassed == 0,
                      "processed blocks counted while enabled");

        engine.setEnabled (false);
        engine.process (ptrs.data(), 2, 256);
        ok &= expect (engine.getStats().blocksPassed == 1,
                      "bypassed blocks counted while disabled");
    }

    // ------------------------------------------------------------------
    // Device-profile persistence round-trip (#33-#35).
    // ------------------------------------------------------------------
    {
        ProfileManager manager;
        manager.upsert ({ "default", "Default", "", presetToState (DspPreset::natural), true });
        manager.upsert ({ "bt-1", "Headphones", "bthh-abc123",
                          presetToState (DspPreset::bass), true });

        const auto profilesDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                     .getChildFile ("otoha_profile_test");
        profilesDir.createDirectory();

        ok &= expect (otoha::sound::saveProfiles (manager, profilesDir), "profiles saved");

        ProfileManager reloaded;
        otoha::sound::loadProfiles (reloaded, profilesDir);

        const auto* bt = reloaded.resolveForDevice ("bthh-abc123");
        ok &= expect (bt != nullptr && bt->name == "Headphones",
                      "device-bound profile survives persistence");
        ok &= expect (bt != nullptr && std::abs (bt->dspState.bassAmount - 0.7f) < 0.001f,
                      "profile DSP state survives persistence");

        const auto* def = reloaded.resolveForDevice ("some-other-device");
        ok &= expect (def != nullptr && def->id == "default",
                      "default profile resolution works after reload");
    }

    // ------------------------------------------------------------------
    // New presets exist in the shared table (#6: no second preset engine).
    // ------------------------------------------------------------------
    {
        const auto presets = allDspPresets();
        bool hasBass = false, hasClarity = false;
        for (auto p : presets)
        {
            if (p == DspPreset::bass) hasBass = true;
            if (p == DspPreset::clarity) hasClarity = true;
            auto s = presetToState (p);   // every preset must produce finite params
            ok &= expect (std::isfinite (s.bassAmount) && std::isfinite (s.clarityAmount)
                              && std::isfinite (s.stereoWidth),
                          "preset parameters finite");
        }
        ok &= expect (hasBass && hasClarity, "Sound presets present in shared table");
        ok &= expect ((int) presets.size() == kNumDspPresets,
                      "kNumDspPresets matches the table");
    }

    if (! ok)
    {
        std::printf ("SoundEngine tests FAILED.\n");
        return 1;
    }
    std::printf ("PASS: sound engine\n");
    return 0;
}
