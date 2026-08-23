#include "TimelineRenderer.h"

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

bool TimelineRenderer::renderToWav (const juce::File& destination, juce::String& errorOut) const
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
        destination.getFileNameWithoutExtension() + " saving", ".wav");

    juce::FileOutputStream stream (temp);
    if (! stream.openedOk())
    {
        errorOut = "Couldn't start writing the file.\nCheck storage space and permissions.";
        return false;
    }

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatWriter> writer (
        wavFormat.createWriterFor (&stream,
                                   doc->getSampleRate(),
                                   (unsigned int) juce::jmax (1, doc->getNumChannels()),
                                   24u /* bit depth: preserve studio-grade headroom */,
                                   {}, 0));

    if (writer == nullptr)
    {
        errorOut = "Couldn't start writing the file.\nCheck storage space and permissions.";
        temp.deleteFile();
        return false;
    }

    constexpr int chunkFrames = 1 << 16;
    const int channels = juce::jmax (1, doc->getNumChannels());
    juce::AudioBuffer<float> chunk (channels, chunkFrames);

    juce::int64 rendered = 0;
    const auto total = doc->totalSamples();

    while (rendered < total)
    {
        const int frames = (int) juce::jmin ((juce::int64) chunkFrames, total - rendered);
        float* ptrs[2] = { chunk.getWritePointer (0),
                           channels > 1 ? chunk.getWritePointer (1) : nullptr };
        doc->readRange (rendered, frames, ptrs, channels);

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
    }

    writer.reset();          // finalises the header

    // Verify before replacing: size must exceed an empty RIFF header.
    if (! temp.existsAsFile() || temp.getSize() <= 44)
    {
        errorOut = "The saved file came out empty — nothing was changed.\n"
                   "Your original recording is still safe.";
        temp.deleteFile();
        return false;
    }

    if (destination.existsAsFile())
        destination.deleteFile();

    if (! temp.moveFileTo (destination))
    {
        errorOut = "Couldn't put the saved file in place:\n" + destination.getFullPathName();
        temp.deleteFile();
        return false;
    }

    return true;
}
} // namespace otoha
