#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include <memory>

#include "AudioDocument.h"

/*
    TimelineRenderer — renders the edited timeline to a WAV file safely:
        temp file → chunked write → verify size → atomic-ish move into place.

    The pipeline (source → edit timeline → [DSP later] → renderer → output)
    is deliberately the shape the Enhance milestone will plug into.
*/
namespace otoha
{
class TimelineRenderer
{
public:
    explicit TimelineRenderer (std::shared_ptr<const AudioDocument> document);

    juce::int64 getRenderedLengthSamples() const;

    /** Never touches `destination` unless the whole render succeeded. */
    bool renderToWav (const juce::File& destination, juce::String& errorOut) const;

private:
    std::shared_ptr<const AudioDocument> doc;
};
} // namespace otoha
