#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <memory>

#include "AudioDocument.h"

/*
    TimelineSource — plays an AudioDocument's clip list as one continuous stream.

    It is a plain juce::PositionableAudioSource, so it plugs straight into the
    existing AudioTransportSource inside Player — no second playback engine,
    no extra device callback. Reads come from the decoded source buffer
    following the clip map; edits bump the document version and the UI swaps
    in a fresh source.
*/
class TimelineSource : public juce::PositionableAudioSource
{
public:
    explicit TimelineSource (std::shared_ptr<const otoha::AudioDocument> document);

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;

    void getNextAudioBlock (const juce::AudioSourceChannelInfo&) override;
    void setNextReadPosition (juce::int64 newPosition) override;
    juce::int64 getNextReadPosition() const override;
    juce::int64 getTotalLength() const override;
    bool isLooping() const override;

private:
    std::shared_ptr<const otoha::AudioDocument> doc;
    juce::int64 position = 0;
};
