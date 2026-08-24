/*
    ReleaseTests — Milestone 10 release hardening, headless.

    Covers:
      #50  audio artifact suite: silence / impulse / sine / sweep / pink noise
           through DSP OFF + Natural + Music + Voice + a Custom state,
           checking NaN/Inf, clipping, DC offset, channel integrity, length
      #42  AppSettings migration + resilience (missing/corrupt/future files)
      #46/#47  the two distinct reset policies
      #13  UserPresetStore CRUD + persistence
      #23  AppLifecycle transition table (exhaustive)
      #39  UpdateChecker version comparison
      #44  DiagnosticsReport content sanity

    No audio device, no UI, no network.
*/
#include "../Source/App/AppLifecycle.h"
#include "../Source/App/UpdateChecker.h"
#include "../Source/Core/AppSettings.h"
#include "../Source/Dsp/DspChain.h"
#include "../Source/Dsp/Presets.h"
#include "../Source/Dsp/UserPresets.h"
#include "../Source/Sound/DiagnosticsReport.h"

#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace otoha;

namespace
{
bool expect (bool condition, const char* message)
{
    if (! condition)
        std::printf ("FAIL: %s\n", message);
    return condition;
}

constexpr double sr = 48000.0;
constexpr int frames = 4800;   // 100 ms

// --- signal generators -------------------------------------------------------

std::vector<float> silence() { return std::vector<float> ((size_t) frames, 0.0f); }

std::vector<float> impulse()
{
    std::vector<float> v ((size_t) frames, 0.0f);
    v[10] = 1.0f;
    return v;
}

std::vector<float> sine (float amp = 0.5f)
{
    std::vector<float> v ((size_t) frames);
    for (int i = 0; i < frames; ++i)
        v[(size_t) i] = amp * std::sin (2.0 * juce::MathConstants<double>::pi * 997.0 * i / sr);
    return v;
}

std::vector<float> sweep()
{
    std::vector<float> v ((size_t) frames);
    for (int i = 0; i < frames; ++i)
    {
        const double t = i / sr;
        const double f = 40.0 + (12000.0 - 40.0) * (i / (double) frames);   // log-ish ramp
        v[(size_t) i] = 0.4f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * f * t);
    }
    return v;
}

std::vector<float> pinkNoise()
{
    // Voss-ish pink filter over white noise, deterministic seed.
    std::vector<float> v ((size_t) frames);
    unsigned seed = 1234567u;
    auto rnd = [&seed]
    {
        seed = seed * 1664525u + 1013904223u;
        return ((seed >> 8) & 0xFFFF) / 32768.0f - 1.0f;
    };
    float b = 0.0f;
    for (auto& x : v)
    {
        b = 0.85f * b + 0.15f * rnd();   // crude -6 dB/oct tilt; deterministic
        x = juce::jlimit (-1.0f, 1.0f, 2.0f * b);
    }
    return v;
}

// --- chain runner --------------------------------------------------------------

struct StereoResult
{
    std::vector<float> left, right;
};

StereoResult runThroughChain (const ProcessingState& state, const std::vector<float>& inputL,
                              const std::vector<float>& inputR)
{
    DspChain chain;
    chain.prepare (sr, 2);
    chain.setParameters (state);

    std::vector<float> l = inputL, r = inputR;
    std::vector<float*> ptrs { l.data(), r.data() };

    // Process in 480-frame blocks like a real stream.
    for (int start = 0; start < frames; start += 480)
    {
        chain.process (ptrs.data(), 480);
        ptrs[0] += 480;
        ptrs[1] += 480;
    }

    return { std::move (l), std::move (r) };
}

bool allFinite (const std::vector<float>& v)
{
    for (float x : v) if (! std::isfinite (x)) return false;
    return true;
}

float peakOf (const std::vector<float>& v)
{
    float p = 0.0f;
    for (float x : v) p = std::max (p, std::abs (x));
    return p;
}

float dcOffset (const std::vector<float>& v)
{
    double sum = 0.0;
    for (float x : v) sum += x;
    return (float) (sum / (double) v.size());
}

juce::File tempDir()
{
    static juce::File dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                .getChildFile ("otoha-release-tests")
                                .getNonexistentSibling (true);
    dir.createDirectory();
    return dir;
}

} // namespace

int main()
{
    bool ok = true;

    // ==========================================================================
    // 1. Audio artifact suite (#50)
    // ==========================================================================
    {
        struct NamedSignal { const char* name; std::vector<float> (*fn)(); };
        const NamedSignal signals[] = {
            { "silence",   &silence }, { "impulse", &impulse }, { "sine", &sine },
            { "sweep",     &sweep },   { "pink noise", &pinkNoise },
        };

        const ProcessingState offState;                       // everything neutral/off
        auto customState = presetToState (DspPreset::music);
        customState.eq.gainsDb[2] = 2.5f;                     // user tweak
        customState.bassAmount = 0.4f;
        customState.compressor.enabled = true;

        const ProcessingState statesToTest[] = {
            offState,
            presetToState (DspPreset::natural),
            presetToState (DspPreset::music),
            presetToState (DspPreset::voice),
            customState,
        };

        for (const auto& sig : signals)
        {
            for (const auto& st : statesToTest)
            {
                const auto res = runThroughChain (st, sig.fn(), sig.fn());

                ok &= expect (res.left.size() == (size_t) frames && res.right.size() == (size_t) frames,
                              "output length preserved");
                ok &= expect (allFinite (res.left) && allFinite (res.right),
                              "no NaN/Inf in output");
                ok &= expect (peakOf (res.left) <= 1.001f && peakOf (res.right) <= 1.001f,
                              "no clipping past full scale");
                ok &= expect (std::abs (dcOffset (res.left)) < 0.05f
                                  && std::abs (dcOffset (res.right)) < 0.05f,
                              "no unexpected DC offset");
            }
        }

        // Channel integrity: distinct L/R content must not collapse or swap.
        {
            const auto res = runThroughChain (presetToState (DspPreset::natural), sine(), silence());
            ok &= expect (peakOf (res.left) > 0.01f && peakOf (res.right) < 0.01f,
                          "channels stay independent through the chain");
        }
    }

    // ==========================================================================
    // 2. AppSettings: resilience, migration, round-trip, resets (#35/#42/#46/#47)
    // ==========================================================================
    {
        const auto dir = tempDir();

        AppSettings defaults;
        ok &= expect (! defaults.firstLaunchComplete && ! defaults.sound.enabled,
                      "fresh settings are conservative");

        // Corrupt file -> defaults.
        dir.getChildFile ("settings.json").replaceWithText ("{ this is not json");
        AppSettings loaded;
        loadAppSettings (loaded, dir);
        ok &= expect (! loaded.firstLaunchComplete, "corrupt settings file falls back to defaults");

        // Round trip.
        loaded.firstLaunchComplete      = true;
        loaded.sound.enabled            = true;
        loaded.sound.enhanceAmount      = 0.75f;
        loaded.sound.presetName         = "My Sound";
        ok &= expect (saveAppSettings (loaded, dir), "settings save succeeds");
        AppSettings reloaded;
        loadAppSettings (reloaded, dir);
        ok &= expect (reloaded.firstLaunchComplete && reloaded.sound.enabled
                          && std::abs (reloaded.sound.enhanceAmount - 0.75f) < 1e-3f
                          && reloaded.sound.presetName == "My Sound",
                      "settings survive a save/load round trip");

        // Migration: a v0 file (missing fields) upgrades cleanly.
        auto* v0 = new juce::DynamicObject();
        v0->setProperty ("configVersion", 0);
        const auto migrated = settingsFromVar (juce::var (v0));
        ok &= expect (migrated.configVersion == kCurrentConfigVersion,
                      "v0 config migrates to the current version");
        ok &= expect (migrated.sound.presetName == "Natural",
                      "migrated config keeps conservative defaults");

        // Future-version file -> safe defaults, no crash.
        auto* future = new juce::DynamicObject();
        future->setProperty ("configVersion", 999);
        const auto guarded = settingsFromVar (juce::var (future));
        ok &= expect (guarded.configVersion == kCurrentConfigVersion && ! guarded.firstLaunchComplete,
                      "future config versions fall back to defaults");

        // Two reset policies.
        AppSettings s = reloaded;
        resetAudioPrefs (s);
        ok &= expect (! s.sound.enabled && s.firstLaunchComplete,
                      "audio-only reset keeps general state (#47)");
        resetAllSettings (s);
        ok &= expect (! s.firstLaunchComplete && ! s.sound.enabled,
                      "full reset clears everything (#46)");
    }

    // ==========================================================================
    // 3. UserPresetStore CRUD + persistence (#13)
    // ==========================================================================
    {
        const auto dir = tempDir().getChildFile ("presets");
        UserPresetStore store (dir);
        store.load();
        ok &= expect (store.size() == 0, "empty store starts empty");

        const auto idA = store.create ("Headphones", presetToState (DspPreset::voice), "Voice");
        ok &= expect (idA.isNotEmpty(), "create returns an id");
        ok &= expect (store.nameExists ("Headphones"), "created preset is findable by name");

        const auto idB = store.create ("Headphones", presetToState (DspPreset::bass), "Bass");
        ok &= expect (store.get (idB) != nullptr && store.get (idB)->name == "Headphones (2)",
                      "duplicate names are disambiguated automatically");

        ok &= expect (store.rename (idA, "Renamed Preset"), "rename works");
        ok &= expect (store.get (idA) != nullptr && store.get (idA)->name == "Renamed Preset",
                      "renamed preset keeps its state under the new name");

        ok &= expect (store.duplicate (idA), "duplicate works");
        ok &= expect (store.remove (idB), "remove works");
        ok &= expect (store.get (idB) == nullptr, "removed preset is gone");
        ok &= expect (store.save(), "store saves");

        UserPresetStore reloaded (dir);
        reloaded.load();
        ok &= expect (reloaded.size() == store.size(),
                      "custom presets survive persistence");
        ok &= expect (std::abs (reloaded.all()[0].state.clarityAmount
                                    - store.all()[0].state.clarityAmount) < 1e-4f,
                      "preset DSP state survives persistence");
    }

    // ==========================================================================
    // 4. AppLifecycle transition table (#23) — exhaustive
    // ==========================================================================
    {
        constexpr AppState all[] = { AppState::starting, AppState::ready,
                                     AppState::processing, AppState::bypassed,
                                     AppState::recovering, AppState::unavailable,
                                     AppState::stopping };

        auto isAllowedPair = [] (AppState from, AppState to)
        {
            if (from == to) return false;
            switch (from)
            {
                case AppState::starting:    return to == AppState::ready || to == AppState::stopping;
                case AppState::ready:       return to == AppState::processing || to == AppState::bypassed
                                                 || to == AppState::recovering || to == AppState::unavailable
                                                 || to == AppState::stopping;
                case AppState::processing:
                case AppState::bypassed:    return to == AppState::processing || to == AppState::bypassed
                                                 || to == AppState::recovering || to == AppState::unavailable
                                                 || to == AppState::stopping;
                case AppState::recovering:  return to == AppState::processing || to == AppState::bypassed
                                                 || to == AppState::ready || to == AppState::unavailable
                                                 || to == AppState::stopping;
                case AppState::unavailable: return to == AppState::ready || to == AppState::recovering
                                                 || to == AppState::stopping;
                case AppState::stopping:    return false;
            }
            return false;
        };

        for (auto from : all)
        {
            AppLifecycle lc (from);
            int legalCount = 0;
            for (auto to : all)
            {
                const bool expectedLegal = isAllowedPair (from, to);
                AppLifecycle probe (from);
                ok &= expect (probe.request (to) == expectedLegal,
                              "lifecycle transition table matches the design");
                if (expectedLegal) ++legalCount;
            }
            ok &= expect (legalCount > 0 || from == AppState::stopping,
                          "every non-terminal state has an exit");
        }

        // The user-facing wording contract (#18).
        AppLifecycle lc;
        lc.request (AppState::ready);
        lc.request (AppState::processing);
        ok &= expect (lc.current() == AppState::processing, "power ON reaches processing");
        ok &= expect (lc.setPower (false) && lc.current() == AppState::bypassed,
                      "ON/OFF flips between processing and bypassed");
        ok &= expect (lc.request (AppState::starting) == false,
                      "cannot go backwards to starting");
    }

    // ==========================================================================
    // 5. UpdateChecker version compare (#39)
    // ==========================================================================
    {
        using UC = UpdateChecker;
        ok &= expect (UC::compareVersions ("1.0.0", "1.0.0") == 0, "equal versions compare equal");
        ok &= expect (UC::compareVersions ("1.0.0", "1.0.1") < 0, "patch ordering");
        ok &= expect (UC::compareVersions ("1.2.0", "1.10.0") < 0, "numeric (not lexical) ordering");
        ok &= expect (UC::compareVersions ("2.0.0", "1.9.9") > 0, "major ordering");
        ok &= expect (UC::compareVersions ("1.0", "1.0.0") == 0, "short versions pad with zeros");

        DisabledUpdateSource disabled;
        UpdateChecker checker { &disabled };
        ok &= expect (! checker.checkNow().checked,
                      "disabled source honestly reports 'unknown' — never invents a version");
    }

    // ==========================================================================
    // 6. Diagnostics report sanity (#44)
    // ==========================================================================
    {
        sound::DiagnosticsInput in;
        in.version = OTOHA_VERSION;
        in.backendName = "WASAPI shared loopback";
        in.outputName = "Headphones";
        in.sampleRate = 48000.0;
        in.channels = 2;
        in.bufferSizeFrames = 512;
        in.hasLatency = true;
        in.latencyMs = 11.6;
        in.underruns = 3;

        const auto report = sound::buildDiagnosticsReport (in);
        ok &= expect (report.contains ("Otoha Diagnostics"), "report has its header");
        ok &= expect (report.contains ("48000"), "report includes sample rate");
        ok &= expect (report.contains ("Headphones"), "report includes output name");
        ok &= expect (report.contains ("No audio content"), "report documents privacy policy");
    }

    if (! ok)
    {
        std::printf ("Release tests FAILED.\n");
        return 1;
    }
    std::printf ("PASS: release hardening\n");
    return 0;
}
