#include "TimelineSource.h"

TimelineSource::TimelineSource (std::shared_ptr<const otoha::AudioDocument> document)
    : doc (std::move (document))
{
}

// Audio data is read directly from the in-memory document, so there is
// nothing to prepare or release.
void TimelineSource::prepareToPlay (int /*samplesPerBlockExpected*/, double /*sampleRate*/) {}
void TimelineSource::releaseResources() {}

void TimelineSource::setNextReadPosition (juce::int64 newPosition)
{
    position = doc != nullptr ? juce::jlimit ((juce::int64) 0, doc->totalSamples(), newPosition) : 0;
}

juce::int64 TimelineSource::getNextReadPosition() const { return position; }

juce::int64 TimelineSource::getTotalLength() const
{
    return doc != nullptr ? doc->totalSamples() : 0;
}

bool TimelineSource::isLooping() const { return false; }

void TimelineSource::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    if (doc == nullptr || doc->getClips().empty())
    {
        info.clearActiveBufferRegion();
        return;
    }

    const int frames = info.numSamples;
    if (frames <= 0)
        return;

    float* dest[2] = { info.buffer->getWritePointer (0, info.startSample),
                       info.buffer->getNumChannels() > 1
                           ? info.buffer->getWritePointer (1, info.startSample) : nullptr };

    doc->readRange (position, frames, dest, info.buffer->getNumChannels());

    // If the document has fewer channels than the output, clear the extras.
    if (info.buffer->getNumChannels() > doc->getNumChannels())
        for (int ch = doc->getNumChannels(); ch < info.buffer->getNumChannels(); ++ch)
            juce::FloatVectorOperations::clear (dest[ch], frames);

    position += frames;
}
