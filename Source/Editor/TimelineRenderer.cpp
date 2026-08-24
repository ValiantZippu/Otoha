#include "TimelineRenderer.h"

#include "../Dsp/DspChain.h"

namespace otoha
{
TimelineRenderer::TimelineRenderer (std::shared_ptr<const AudioDocument> document)
    : doc (std::move (document))
{
}

juce::int64 TimelineRenderer::getRenderedLengthSamples() const
{
    return doc != nullptr ? doc->totalSamples() : 0;
}

bool TimelineRenderer::renderToFile (juce::AudioFormat& format,
                                     const juce::File& destination,
                                     juce::String& errorOut,
                                     const ProcessingState* dsp,
                                     const std::atomic<bool>* cancelFlag,
                                     const std::function<bool (float)>& progress) const
{
    if (doc == nullptr || doc->getClips().empty())
    {
        errorOut = "There is nothing to save.";
        return false;
    }

    const auto parent = destination.getParentDirectory();
    if (! parent.createDirectory().wasOk())
    {
        errorOut = "Couldn't create the destination folder:\n" + parent.getFullPathName();
        return false;
    }

    // Render into a temp sibling so a failed write can never damage the target.
    const auto temp = parent.getNonexistentChildFile (
        destination.getFileNameWithoutExtension() + " saving", ".tmp");

    auto stream = std::make_unique<juce::FileOutputStream> (temp);
    if (stream == nullptr || ! stream->openedOk())
    {
        errorOut = "Couldn't start writing the file.\nCheck storage space and permissions.";
        return false;
    }

    const int bitsPerSample = format.getFileExtensions().contains ("flac") ? 16 : 24;

    // createWriterFor takes ownership of the stream on success (JUCE 6+ API).
    std::unique_ptr<juce::AudioFormatWriter> writer (
        format.createWriterFor (stream,
                                doc->getSampleRate(),
                                (unsigned int) juce::jmax (1, doc->getNumChannels()),
                                (unsigned int) bitsPerSample,
                                {}, 0));

    if (writer == nullptr)
    {
        errorOut = "Couldn't start writing the file.\nCheck storage space and permissions.";
        temp.deleteFile();
        return false;
    }

    // Offline DSP instance: same definition, same ProcessingState as preview.
    DspChain chain;
    if (dsp != nullptr && dsp->enabled)
    {
        chain.prepare (doc->getSampleRate(), doc->getNumChannels());
        chain.setParameters (*dsp);
    }

    constexpr int chunkFrames = 1 << 16;
    const int channels = juce::jmax (1, doc->getNumChannels());
    juce::AudioBuffer<float> chunk (channels, chunkFrames);
    const float total = (float) doc->totalSamples();
    juce::int64 rendered = 0;

    while (rendered < (juce::int64) total)
    {
        if (cancelFlag != nullptr && cancelFlag->load())
        {
            errorOut = "Export cancelled.";
            writer.reset();
            temp.deleteFile();          // never leave a partial file behind
            return false;
        }

        const int frames = (int) juce::jmin ((juce::int64) chunkFrames,
                                             (juce::int64) total - rendered);
        float* ptrs[2] = { chunk.getWritePointer (0),
                           channels > 1 ? chunk.getWritePointer (1) : nullptr };

        doc->readRange (rendered, frames, ptrs, channels);   // timeline

        if (dsp != nullptr && dsp->enabled)
            chain.process (ptrs, frames);                    // DSP chain

        if (! writer->writeFromFloatArrays (ptrs, channels, frames))
        {
            errorOut = "Otoha couldn't finish writing the file.\n"
                       "There may not be enough storage space.\n"
                       "Your original recording is still safe.";
            writer.reset();
            temp.deleteFile();
            return false;
        }

        rendered += frames;
        if (progress != nullptr && ! progress (rendered / total))
        {
            errorOut = "Export cancelled.";
            writer.reset();
            temp.deleteFile();
            return false;
        }
    }

    writer.reset();   // finalises headers

    // Verify before replacing.
    if (! temp.existsAsFile() || temp.getSize() <= 44)
    {
        errorOut = "The saved file came out empty — nothing was changed.\n"
                   "Your original recording is still safe.";
        temp.deleteFile();
        return false;
    }

    // Match the extension the user asked for.
    auto finalTemp = temp.withFileExtension (destination.getFileExtension());
    if (! temp.moveFileTo (finalTemp))
        finalTemp = temp;

    if (destination.existsAsFile())
        destination.deleteFile();

    if (! finalTemp.moveFileTo (destination))
    {
        errorOut = "Couldn't put the saved file in place:\n" + destination.getFullPathName();
        finalTemp.deleteFile();
        return false;
    }

    return true;
}

bool TimelineRenderer::renderToWav (const juce::File& destination, juce::String& errorOut,
                                    const ProcessingState* dsp,
                                    const std::atomic<bool>* cancelFlag) const
{
    juce::WavAudioFormat wav;
    return renderToFile (wav, destination, errorOut, dsp, cancelFlag, {});
}
} // namespace otoha
