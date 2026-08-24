#include "AudioExporter.h"

#include "../Dsp/SampleRateConverter.h"
#include "FfmpegSupport.h"

namespace otoha
{
namespace
{
/** Streams the document through DSP into an AudioFormatWriter.
    Returns false on failure/cancellation (with errorOut set). */
bool streamIntoWriter (const AudioExportRequest& request,
                       juce::AudioFormatWriter* writer,
                       const std::atomic<bool>& cancelFlag,
                       const std::function<void (float)>& progress,
                       juce::String& errorOut)
{
    auto doc = request.document;

    DspChain chain;
    const bool useDsp = request.dsp != nullptr && request.dsp->enabled;
    if (useDsp)
    {
        chain.prepare (doc->getSampleRate(), doc->getNumChannels());
        chain.setParameters (*request.dsp);
    }

    constexpr int chunkFrames = 1 << 16;
    const int channels = juce::jmax (1, doc->getNumChannels());
    juce::AudioBuffer<float> chunk (channels, chunkFrames);
    const float total = (float) doc->totalSamples();
    juce::int64 rendered = 0;

    while (rendered < (juce::int64) total)
    {
        if (cancelFlag.load())
        {
            errorOut = "Export cancelled.";
            return false;
        }

        const int frames = (int) juce::jmin ((juce::int64) chunkFrames, (juce::int64) total - rendered);
        float* ptrs[2] = { chunk.getWritePointer (0),
                           channels > 1 ? chunk.getWritePointer (1) : nullptr };

        doc->readRange (rendered, frames, ptrs, channels);   // edit timeline

        if (useDsp)
            chain.process (ptrs, frames);                    // DSP chain

        if (! writer->writeFromFloatArrays (ptrs, channels, frames))
        {
            errorOut = "Otoha couldn't finish the export.\n"
                       "There may not be enough storage space.\n"
                       "Your original recording is still safe.";
            return false;
        }

        rendered += frames;
        if (progress != nullptr)
            progress (juce::jlimit (0.0f, 1.0f, 0.7f * (rendered / total)));
    }
    return true;
}

/** Whole-buffer conversion when the preset demands a different rate/channels. */
juce::AudioBuffer<float> convertIfNeeded (std::shared_ptr<const AudioDocument> doc,
                                          const ProcessingState* dsp,
                                          int sampleRateOverride, int channelOverride)
{
    // Memory note: only taken when the user actually overrides rate/channels —
    // the normal path streams chunk-by-chunk and stays flat in memory (#47).
    auto full = doc->readRangeToBuffer (0, (int) doc->totalSamples());

    if (dsp != nullptr && dsp->enabled)
    {
        DspChain chain;
        chain.prepare (doc->getSampleRate(), full.getNumChannels());
        chain.setParameters (*dsp);

        for (int pos = 0; pos < full.getNumSamples(); pos += 65536)
        {
            const int frames = juce::jmin (65536, full.getNumSamples() - pos);
            float* ptrs[2] = { full.getWritePointer (0, pos),
                               full.getNumChannels() > 1 ? full.getWritePointer (1, pos)
                                                         : full.getWritePointer (0, pos) };
            chain.process (ptrs, frames);
        }
    }

    auto converted = full;
    if (channelOverride > 0)
        converted = adaptChannels (converted, channelOverride);
    if (sampleRateOverride > 0)
        converted = resampleLinear (converted, doc->getSampleRate(), (double) sampleRateOverride);
    return converted;
}

bool streamIntoWriterBuffered (juce::AudioFormatWriter* writer,
                               const juce::AudioBuffer<float>& buffer,
                               const std::atomic<bool>& cancelFlag,
               const std::function<void (float)>& progress,
               juce::String& errorOut)
{
    constexpr int chunkFrames = 1 << 16;
    const int channels = buffer.getNumChannels();

    for (int pos = 0; pos < buffer.getNumSamples(); pos += chunkFrames)
    {
        if (cancelFlag.load()) { errorOut = "Export cancelled."; return false; }

        const int frames = juce::jmin (chunkFrames, buffer.getNumSamples() - pos);
        float* ptrs[2] = { buffer.getWritePointer (0, pos),
                           channels > 1 ? buffer.getWritePointer (1, pos)
                                        : buffer.getWritePointer (0, pos) };
        if (! writer->writeFromFloatArrays (ptrs, channels, frames))
        {
            errorOut = "Otoha couldn't finish the export.\nThere may not be enough storage space.";
            return false;
        }

        if (progress != nullptr)
            progress (juce::jlimit (0.0f, 1.0f,
                       0.7f * (float) (pos + frames) / (float) buffer.getNumSamples()));
    }
    return true;
}

/** Verify the written file is non-trivial before it may replace anything. */
bool writerFinaliseAndMove (const juce::File& candidate, const juce::File& destination,
                            juce::String& errorOut)
{
    if (! candidate.existsAsFile() || candidate.getSize() <= 44)
    {
        errorOut = "The exported file came out empty — nothing was changed.\n"
                   "Your original recording is still safe.";
        candidate.deleteFile();
        return false;
    }

    if (destination.existsAsFile())
        destination.deleteFile();

    if (! candidate.moveFileTo (destination))
    {
        errorOut = "Couldn't put the exported file in place:\n" + destination.getFullPathName();
        candidate.deleteFile();
        return false;
    }
    return true;
}
} // namespace

bool AudioExporter::exportAudio (const AudioExportRequest& request,
                                 const juce::File& destination,
                                 const juce::File& ffmpegExecutable,
                                 const std::atomic<bool>& cancelFlag,
                                 const std::function<void (float)>& progress,
                                 juce::String& errorOut)
{
    auto doc = request.document;
    if (doc == nullptr || destination == juce::File{} || doc->getClips().empty())
    {
        errorOut = "There is nothing to export.";
        return false;
    }

    const auto caps = capabilitiesFor (request.format);
    const auto parent = destination.getParentDirectory();
    parent.createDirectory();

    const auto tmpBase = parent.getNonexistentChildFile (
        destination.getFileNameWithoutExtension() + " exporting", ".tmp");

    struct TempGuard
    {
        juce::File a, b;
        bool active = true;
        ~TempGuard() { if (active) { a.deleteFile(); b.deleteFile(); } }
    } temps;
    temps.a = tmpBase.withFileExtension ("wav");       // intermediate for FFmpeg
    temps.b = tmpBase.withFileExtension (caps.extension); // final candidate

    bool ok = false;

    if (! caps.requiresFfmpeg)
    {
        // --- lossless: JUCE's own writers, no FFmpeg -------------------------
        juce::WavAudioFormat wavFormat;
        juce::FlacAudioFormat flacFormat;
        juce::AudioFormat& format = request.format == ExportFormat::wav
            ? static_cast<juce::AudioFormat&> (wavFormat) 
            : static_cast<juce::AudioFormat&> (flacFormat);

        const int bitsPerSample = request.format == ExportFormat::flac ? 16 : 24;

        const bool needsConversion = request.sampleRateOverride > 0
                                  || request.channelOverride > 0;

        if (needsConversion)
        {            auto converted = convertIfNeeded (doc, request.dsp,
                                              request.sampleRateOverride, request.channelOverride);

            auto stream = temps.b.createOutputStream();
            std::unique_ptr<juce::AudioFormatWriter> writer (
                stream != nullptr
                    ? format.createWriterFor (stream,
                                                  request.sampleRateOverride > 0
                                                  ? (double) request.sampleRateOverride : doc->getSampleRate(),
                                              (unsigned int) juce::jmax (1, converted.getNumChannels()),
                                              (unsigned int) bitsPerSample, {}, 0)
                    : nullptr);

            if (writer == nullptr)
                errorOut = "Couldn't start writing the export.\nCheck storage space and permissions.";
            else
                ok = streamIntoWriterBuffered (writer.get(), converted, cancelFlag, progress, errorOut);
        }
        else
        {
            auto stream = temps.b.createOutputStream();
            std::unique_ptr<juce::AudioFormatWriter> writer (
                stream != nullptr
                    ? format.createWriterFor (stream, doc->getSampleRate(),
                                              (unsigned int) juce::jmax (1, doc->getNumChannels()),
                                              (unsigned int) bitsPerSample, {}, 0)
                    : nullptr);

            if (writer == nullptr)
                errorOut = "Couldn't start writing the export.\nCheck storage space and permissions.";
            else
                ok = streamIntoWriter (request, writer.get(), cancelFlag, progress, errorOut);
        }

        if (ok)
            ok = writerFinaliseAndMove (temps.b, destination, errorOut);
    }
    else
    {
        // --- compressed: intermediate WAV -> FFmpeg -> destination --------------
        double sourceRate = 0.0;
        int sourceChans = 0;

        // Intermediate WAV always at source rate/channels (no double conversion).
        auto stream = temps.a.createOutputStream();
        std::unique_ptr<juce::AudioFormatWriter> wavWriter;
        if (stream != nullptr)
        {
            juce::WavAudioFormat wav;
            wavWriter.reset (wav.createWriterFor (stream, doc->getSampleRate(),
                                                  (unsigned int) juce::jmax (1, doc->getNumChannels()),
                                                  24u, {}, 0));
        }
        if (wavWriter == nullptr)
        {
            errorOut = "Couldn't start writing the export.\nCheck storage space and permissions.";
            return false;
        }

        if (! streamIntoWriter (request, wavWriter.get(), cancelFlag,
                                [&progress] (float p) { if (progress) progress (p); }, errorOut))
            return false;

        wavWriter.reset();

        FfmpegEncoder::Request enc;
        enc.format = request.format;
        enc.bitrateKbps = bitrateKbpsFor (request.format, request.quality);
        enc.sourceSampleRate = doc->getSampleRate();
        enc.channels = doc->getNumChannels();
        enc.durationSeconds = (double) doc->totalSamples() / doc->getSampleRate();
        enc.titleMetadata = request.titleMetadata;

        ok = FfmpegEncoder::encode (ffmpegExecutable, temps.a, temps.b, enc, cancelFlag,
                                    [&progress] (float p)
                                    { if (progress) progress (0.7f + 0.3f * p); },
                                    errorOut);

        if (ok)
        {
            if (destination.existsAsFile())
                destination.deleteFile();

            ok = temps.b.moveFileTo (destination);
            if (! ok)
                errorOut = "Couldn't put the exported file in place:\n" + destination.getFullPathName();
        }
    }

    if (ok)
        temps.active = false;   // output moved out; nothing temp left to guard

    return ok;
}
} // namespace otoha
