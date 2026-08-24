/*
    CrossPlatformTests — Milestone 12 shared-core verification, headless.

    Covers:
      #15  RecorderPhase transition table (valid chain + refusals + interruption)
      #50/#51  capability declarations per platform (no faked capabilities)
      #64/#65  error model: categories translate to distinct, jargon-free
               user messages on desktop and mobile surfaces
      #6-#8/#59  .otoha project format: save/load roundtrip, timeline + DSP
               payload preserved, newer-format refused gracefully, corrupt file
               reported with a user-facing message, atomic layout present
*/
#include "../Source/Audio/RecorderPhase.h"
#include "../Source/Core/OtohaError.h"
#include "../Source/Core/OtohaLog.h"
#include "../Source/Core/PlatformCapabilities.h"
#include "../Source/Dsp/DspChain.h"
#include "../Source/Editor/AudioDocument.h"
#include "../Source/Editor/ProjectFormat.h"

#include <cmath>
#include <cstdio>
#include <limits>

namespace
{
bool expect (bool condition, const char* message)
{
    if (! condition)
        std::printf ("FAIL: %s\n", message);
    return condition;
}
} // namespace

int main()
{
    using namespace otoha;
    bool ok = true;

    // --- #15 RecorderPhase ---------------------------------------------------
    {
        const RecorderPhase happyPath[] = {
            RecorderPhase::idle, RecorderPhase::preparing, RecorderPhase::countdown,
            RecorderPhase::recording, RecorderPhase::paused, RecorderPhase::recording,
            RecorderPhase::stopping, RecorderPhase::saving, RecorderPhase::complete,
            RecorderPhase::idle
        };
        for (int i = 0; i + 1 < (int) (sizeof (happyPath) / sizeof (happyPath[0])); ++i)
            ok = expect (isValidRecorderTransition (happyPath[i], happyPath[i + 1]),
                         "happy-path recorder transition should be valid") && ok;

        ok = expect (! isValidRecorderTransition (RecorderPhase::idle, RecorderPhase::recording),
                     "must not record without preparing") && ok;
        ok = expect (! isValidRecorderTransition (RecorderPhase::countdown, RecorderPhase::complete),
                     "cannot jump from countdown to complete") && ok;
        ok = expect (! isValidRecorderTransition (RecorderPhase::saving, RecorderPhase::recording),
                     "cannot re-enter recording while saving") && ok;
        ok = expect (! isValidRecorderTransition (RecorderPhase::error, RecorderPhase::saving),
                     "cannot save a failed take") && ok;
        ok = expect (isValidRecorderTransition (RecorderPhase::recording, RecorderPhase::error)
                     && isInterruptTerminal (RecorderPhase::error),
                     "interruptions land in a defined terminal state") && ok;
    }

    // --- #50/#51 Capabilities -------------------------------------------------
    {
        const auto win = PlatformCapabilities::forPlatform (PlatformKind::windows);
        ok = expect (win.systemWideOutputProcessing && win.systemTray && win.startupWithOS,
                     "Windows declares its real Sound capabilities") && ok;

        const auto android = PlatformCapabilities::forPlatform (PlatformKind::android);
        ok = expect (android.microphoneRecording && ! android.systemWideOutputProcessing,
                     "Android: mic yes, system-wide enhancement honestly no") && ok;
        ok = expect (android.nativeShareSheet && android.systemDocumentPicker
                     && ! android.backgroundRecording,
                     "Android: share/picker declared; background recording not claimed") && ok;

        const auto ios = PlatformCapabilities::forPlatform (PlatformKind::ios);
        ok = expect (ios.microphoneRecording && ! ios.backgroundRecording,
                     "iOS: recording yes, invisible background recording never") && ok;

        // Every platform's Studio basics stay true — that is the point of M12.
        for (auto k : { PlatformKind::windows, PlatformKind::macos, PlatformKind::linux,
                        PlatformKind::android, PlatformKind::ios })
        {
            const auto c = PlatformCapabilities::forPlatform (k);
            ok = expect (c.microphoneRecording && c.batchExport,
                         "Studio core capability holds on every platform") && ok;
        }
    }

    // --- #64/#65 Error model --------------------------------------------------
    {
        for (int i = 1; i < (int) ErrorCategory::audioInterrupted; ++i)
        {
            const auto cat = (ErrorCategory) i;
            const auto desktop = userMessage (cat, ErrorSurface::desktop);
            const auto mobile  = userMessage (cat, ErrorSurface::mobile);
            ok = expect (! desktop.isEmpty() && ! mobile.isEmpty(),
                         "every error category has user text on both surfaces") && ok;
            ok = expect (desktop != juce::String (toString (cat)),
                         "user message must not be the raw enum name") && ok;
        }
        ok = expect (userMessage (ErrorCategory::deviceUnavailable, ErrorSurface::mobile)
                     != userMessage (ErrorCategory::deviceUnavailable, ErrorSurface::desktop),
                     "surface-specific wording exists where it matters") && ok;

        OtohaError e { ErrorCategory::exportFailed, "ffmpeg exit code 1" };
        ok = expect (! e.ok() && ! e.detail.isEmpty(),
                     "errors carry category + diagnostics-only detail") && ok;
    }

    // --- #6-#8/#59 .otoha project format --------------------------------------
    {
        // Build a small document programmatically (no file needed).
        juce::AudioBuffer<float> buffer (2, 48000);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 48000; ++i)
                buffer.setSample (ch, i, std::sin (i * 0.01f));

        AudioDocument original (buffer, 48000.0);
        original.setSelection (100, 500);
        original.processing.enabled    = true;
        original.processing.bassAmount = 0.5f;
        original.rippleDelete (1000, 200);           // give it a non-trivial timeline

        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile ("otoha_xplat_test.otoha");
        dir.deleteRecursively();

        juce::String err;
        ok = expect (project::saveProject (original, dir, "Cross-platform test", err),
                     "saveProject succeeds") && ok;
        ok = expect (dir.getChildFile ("project.json").existsAsFile()
                     && dir.getChildFile ("audio").isDirectory()
                     && dir.getChildFile ("waveform").isDirectory(),
                     ".otoha container has project.json + audio/ + waveform/") && ok;
        ok = expect (! dir.getChildFile ("project.json.tmp").existsAsFile(),
                     "atomic write leaves no temp file behind") && ok;

        // Roundtrip into a fresh document.
        AudioDocument loaded (juce::AudioBuffer<float> (2, 48000), 48000.0);
        ok = expect (project::loadProject (dir, loaded, err), "loadProject succeeds") && ok;
        ok = expect ((juce::int64) loaded.getClips().size() == (juce::int64) original.getClips().size()
                     && loaded.totalSamples() == original.totalSamples(),
                     "timeline survives the roundtrip") && ok;
        ok = expect (loaded.processing.enabled && loaded.processing.bassAmount > 0.4f,
                     "DSP state survives the roundtrip") && ok;

        // A newer format version must be refused gracefully (#59).
        {
            auto futureJson = juce::JSON::parse (dir.getChildFile ("project.json"));
            auto* obj = futureJson.getDynamicObject();
            if (obj != nullptr)
                obj->setProperty ("formatVersion", project::currentFormatVersion + 1);

            AudioDocument victim (juce::AudioBuffer<float> (2, 48000), 48000.0);
            juce::String refuseErr;
            ok = expect (! project::applyProjectJSON (victim, futureJson, refuseErr)
                         && ! refuseErr.isEmpty(),
                         "newer project format refused with an explanation") && ok;
        }

        // Corrupt file -> user-facing message, no crash.
        {
            const auto badDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                    .getChildFile ("otoha_xplat_bad.otoha");
            badDir.deleteRecursively();
            badDir.createDirectory();
            badDir.getChildFile ("project.json").replaceWithText ("{ not json ");

            AudioDocument victim (juce::AudioBuffer<float> (2, 48000), 48000.0);
            juce::String badErr;
            ok = expect (! project::loadProject (badDir, victim, badErr)
                         && ! badErr.isEmpty(),
                         "corrupt project reports a user-facing error") && ok;

            badDir.deleteRecursively();
        }

        dir.deleteRecursively();
    }

    // --- #12 DSP NaN/Inf guard -------------------------------------------------
    {
        DspChain chain;
        chain.prepare (48000.0, 2);
        chain.setParameters (ProcessingState {});   // neutral defaults

        float left[8]  = { 0.1f, 0.2f, std::numeric_limits<float>::quiet_NaN(), 0.4f,
                           std::numeric_limits<float>::infinity(), 0.5f, 0.6f, 0.7f };
        float right[8] = { 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f };
        float* chans[2] = { left, right };

        chain.process (chans, 8);

        bool allFinite = true;
        for (float s : left)  allFinite &= std::isfinite (s);
        for (float s : right) allFinite &= std::isfinite (s);
        ok = expect (allFinite, "chain output contains no NaN/Inf after the guard");
        ok = expect (chain.invalidSampleCount() >= 2,
                     "guard counted the invalid input samples");
    }

    otoha::log::info ("cross-platform suite finished");   // smoke-test the logger

    if (ok) std::printf ("All cross-platform tests passed.\n");
    return ok ? 0 : 1;
}
