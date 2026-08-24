/*
    ExportTests — headless verification of the M6 export system.

    FFmpeg-dependent integration tests self-skip when no encoder is found
    (#51): everything else (mappings, naming, queue, isolation, per-recording
    DSP) is fully offline.
*/
#include "../Source/Export/AudioExporter.h"
#include "../Source/Export/ExportManager.h"
#include "../Source/Export/ExportPresets.h"
#include "../Source/Export/Naming.h"
#include "../Source/Dsp/Presets.h"

#include <cstdio>

using namespace otoha;

namespace
{
bool expect (bool condition, const char* message)
{
    if (! condition)
        std::printf ("FAIL: %s\n", message);
    return condition;
}

juce::File writeTestWav (const juce::File& dir, const juce::String& name, double rate = 48000.0)
{
    juce::WavAudioFormat wavFormat;

    const auto file = dir.getChildFile (name);
    auto stream = file.createOutputStream();
    if (stream == nullptr) return {};

    auto* wav = &wavFormat;
    std::unique_ptr<juce::AudioFormatWriter> writer (
        wav->createWriterFor (stream.release(), rate, 1, 16, {}, 0));
    if (writer == nullptr) return {};

    juce::AudioBuffer<float> silence (1, (int) rate);   // one second
    writer->writeFromAudioSampleBuffer (silence, 0, silence.getNumSamples());
    writer.reset();
    return file;
}

bool waitForCompletion (ExportManager& manager, int timeoutMs = 30000)
{
    for (int waited = 0; waited < timeoutMs; waited += 50)
    {
        bool busy = false;
        for (const auto& s : manager.getStatuses())
            if (s.state == JobStatus::State::waiting
                || s.state == JobStatus::State::rendering
                || s.state == JobStatus::State::encoding)
                { busy = true; break; }
        if (! busy) return true;
        juce::Thread::sleep (50);
    }
    return false;
}
} // namespace

int main()
{
    bool ok = true;

    // --- format/preset mappings -------------------------------------------------
    ok &= expect (bitrateKbpsFor (ExportFormat::m4a, ExportQuality::high) == 256,
                  "M4A High = AAC 256 kbps");
    ok &= expect (bitrateKbpsFor (ExportFormat::opus, ExportQuality::standard) == 128,
                  "Opus Standard = 128 kbps");
    ok &= expect (bitrateKbpsFor (ExportFormat::mp3, ExportQuality::small) == 96,
                  "MP3 Small = 96 kbps");
    ok &= expect (bitrateKbpsFor (ExportFormat::wav, ExportQuality::high) == 0
                      && bitrateKbpsFor (ExportFormat::flac, ExportQuality::standard) == 0,
                  "lossless formats carry no fixed bitrate");

    ok &= expect (! capabilitiesFor (ExportFormat::wav).requiresFfmpeg
                      && ! capabilitiesFor (ExportFormat::flac).requiresFfmpeg,
                  "WAV/FLAC never route through FFmpeg");
    ok &= expect (capabilitiesFor (ExportFormat::m4a).requiresFfmpeg
                      && capabilitiesFor (ExportFormat::opus).requiresFfmpeg
                      && capabilitiesFor (ExportFormat::mp3).requiresFfmpeg,
                  "compressed formats declare FFmpeg dependency");
    ok &= expect ((int) allExportPresets().size() >= 8, "all launch presets present");

    // --- filename generation -------------------------------------------------------
    const auto dir = juce::File::createTempFile ("otoha_export_test_dir");
    dir.createDirectory();

    const auto base = resolveDestination (dir, "My Recording", ExportFormat::m4a, CollisionPolicy::keepBoth);
    ok &= expect (base.getFileName() == "My Recording.m4a", "plain name maps to .m4a");
    base.create();

    const auto second = resolveDestination (dir, "My Recording", ExportFormat::m4a, CollisionPolicy::keepBoth);
    ok &= expect (second.getFileName() == "My Recording (1).m4a", "keepBoth appends (1)");
    second.create();

    ok &= expect (resolveDestination (dir, "My Recording", ExportFormat::m4a,
                                      CollisionPolicy::skip) == juce::File{},
                  "skip policy yields an empty destination");

    const auto replaced = resolveDestination (dir, "My Recording", ExportFormat::m4a,
                                              CollisionPolicy::replace);
    ok &= expect (replaced == base, "replace policy targets the existing file");

    const auto unicode = resolveDestination (dir, "Ünïcode \"take\" 🎙", ExportFormat::opus,
                                             CollisionPolicy::keepBoth);
    ok &= expect (! unicode.getFileName().containsAnyOf ("\\/:*?\"<>|"),
                  "sanitizer strips filesystem-illegal characters");
    ok &= expect (resolveDestination (dir, juce::String().paddedRight ('x', 400),
                                      ExportFormat::mp3, CollisionPolicy::keepBoth)
                      .getFileName().length() < 220,
                  "very long names are truncated defensively");

    // --- batch queue: ordering + failure isolation + per-recording DSP ---------------
    {
        const auto workDir = dir.getChildFile ("jobs");
        workDir.createDirectory();
        const auto outDir = workDir.getChildFile ("out");
        outDir.createDirectory();

        ExportManager manager (juce::File{});

        // A and C are real recordings; B points at a missing file -> isolated failure.
        juce::File a = writeTestWav (workDir, "A.wav");
        juce::File c = writeTestWav (workDir, "C.wav");

        auto submit = [&] (const juce::String& name, const juce::File& source)
        {
            ExportRequest r;
            r.sourceFile = source;
            r.baseName = name;
            r.destinationDirectory = outDir;
            r.format = ExportFormat::wav;
            r.quality = ExportQuality::standard;
            r.collision = CollisionPolicy::replace;
            return manager.submit (r);
        };

        submit ("A", a);
        submit ("B", workDir.getChildFile ("missing.wav"));
        submit ("C", c);

        ok &= expect (waitForCompletion (manager), "batch finishes");

        const auto summary = manager.getSummary();
        ok &= expect (summary.succeeded == 2, "two files succeed");
        ok &= expect (summary.failed == 1, "one file fails without killing the batch");

        ok &= expect (outDir.getChildFile ("A.wav").existsAsFile()
                          && outDir.getChildFile ("C.wav").existsAsFile(),
                      "successful outputs exist on disk");

        // Per-recording DSP: sidecar states differ per recording and survive.
        ProcessingState voiceState = presetToState (DspPreset::voice);
        voiceState.enabled = true;
        voiceState.noiseReduction.mode = NoiseReductionMode::gentle;
        voiceState.noiseReduction.strength = 0.6f;

        AudioDocument docA;
        juce::String err;
        docA.loadFromFile (a, err);
        docA.sourceFileForTest() = a;
        docA.processing = voiceState;
        ok &= expect (docA.autosaveState(), "sidecar with DSP written for A");

        AudioDocument docC;
        docC.loadFromFile (c, err);

        ok &= expect (docA.processing.enabled != docC.processing.enabled,
                      "recordings can hold different DSP states");

        // Override path: an explicit DSP state wins over the sidecar.
        ExportRequest overrideRequest;
        overrideRequest.sourceFile = a;
        overrideRequest.baseName = "A_overridden";
        overrideRequest.destinationDirectory = outDir;
        overrideRequest.format = ExportFormat::wav;
        overrideRequest.useDspOverride = true;
        overrideRequest.dspStateOverride = ProcessingState{};   // neutral/off
        manager.submit (overrideRequest);
        ok &= expect (waitForCompletion (manager), "override job finishes");
        ok &= expect (manager.getSummary().succeeded == 3, "override job succeeds");

        // --- retry of a failed job --------------------------------------------------
        const auto statusesBefore = manager.getStatuses();
        int failedId = -1;
        for (const auto& s : statusesBefore)
            if (s.state == JobStatus::State::failed)
                failedId = s.id;
        ok &= expect (failedId > 0, "a failed job exists");

        // Make B's source appear, then retry -> should now succeed.
        writeTestWav (workDir, "missing.wav");
        ok &= expect (manager.retryJob (failedId), "retry accepts a failed job");
        ok &= expect (waitForCompletion (manager), "retry completes");
        ok &= expect (manager.getSummary().succeeded == 4, "retried job succeeds");

        // --- cancellation between jobs -------------------------------------------------
        // Race note: a 1-second wav can legitimately FINISH inside the 20 ms
        // window before cancelAll() arrives — a completed job counts as
        // succeeded, and that is correct behaviour. The invariant under test:
        // a job hit by the cancel must end 'cancelled', never half-running,
        // failed-by-cancel, or "completed by cancellation".
        // Queue several and cancel immediately: jobs run one-at-a-time, so the
        // tail of the queue is deterministically still waiting (state waiting
        // -> cancelled inside cancelAll) no matter how fast each render is.
        submit ("X", a);
        submit ("Y", c);
        submit ("Z", a);
        submit ("W", c);
        manager.cancelAll();
        ok &= expect (waitForCompletion (manager), "cancellation settles");

        bool cancelledJobsAreClean = true;
        int sawCancelled = 0;
        for (const auto& s : manager.getStatuses())
        {
            if (s.displayName == "X" || s.displayName == "Y"
                    || s.displayName == "Z" || s.displayName == "W")
            {
                if (s.state == JobStatus::State::cancelled)
                    ++sawCancelled;
                else if (s.state != JobStatus::State::completed)
                    cancelledJobsAreClean = false;   // neither finished nor cleanly cancelled
            }
        }
        ok &= expect (sawCancelled >= 1, "at least one queued job was cancelled");
        ok &= expect (cancelledJobsAreClean,
                      "cancelled jobs do not count as succeeded");
    }

    // --- FFmpeg availability reporting (never fatal) -------------------------------------
    {
        FfmpegLocator locator;
        FfmpegInfo info;
        const auto status = locator.locate (info);
        ok &= expect (status == EncoderStatus::available
                          || status == EncoderStatus::unavailable
                          || status == EncoderStatus::unsupported,
                      "locator reports a sane status");

        if (status != EncoderStatus::available)
            std::printf ("NOTE: FFmpeg not available in this environment — "
                         "compressed-encode integration tests skipped.\n");

        // Lossless export works regardless of FFmpeg.
        const auto src = writeTestWav (dir, "lossless_src.wav");
        ExportRequest request;
        request.sourceFile = src;
        request.baseName = "lossless_out";
        request.destinationDirectory = dir.getChildFile ("ffx");
        request.format = ExportFormat::wav;
        request.collision = CollisionPolicy::replace;
        ExportManager losslessManager (juce::File{});
        losslessManager.submit (request);
        ok &= expect (waitForCompletion (losslessManager), "WAV export works without FFmpeg");
        ok &= expect (losslessManager.getSummary().succeeded == 1,
                      "lossless export succeeds even when compressed is unavailable");
    }

    dir.deleteRecursively();

    if (! ok) return 1;
    std::printf ("PASS: export system\n");
    return 0;
}
