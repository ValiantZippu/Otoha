#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <memory>
#include <optional>
#include <utility>

#include "DspChain.h"
#include "ProcessingState.h"

/*
    DspPreviewSource — real-time preview path.

    Wraps any PositionableAudioSource (the editor's TimelineSource) with a
    DspChain. It uses the SAME DspChain definition and ProcessingState data as
    offline rendering; there is no separate "preview DSP".

    A/B and parameter tweaks call setParameters() from the UI thread — no
    source rebuild, no file reload, smoothing inside the chain prevents clicks.
*/
class DspPreviewSource : public juce::PositionableAudioSource
{
public:
    DspPreviewSource (std::unique_ptr<juce::PositionableAudioSource> upstream, double sampleRate)
        : upstreamSource (std::move (upstream))
    {
        chain.prepare (sampleRate, 2);
    }

    /** UI thread: publish new processing parameters (bypass flips included). */
    void setParameters (const otoha::ProcessingState& p)
    {
        currentParams = p;
        chain.setParameters (p);
    }

    void prepareToPlay (int /*samplesPerBlock*/, double newSampleRate) override
    {
        // Re-prepare keeps filter state valid across device changes.
        const auto params = currentParams;
        chain.prepare (newSampleRate, 2);
        if (currentParams.has_value())
            chain.setParameters (*params);
        if (upstreamSource != nullptr)
            upstreamSource->prepareToPlay (0, newSampleRate);
    }

    void releaseResources() override {}

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override
    {
        if (upstreamSource == nullptr)
        {
            info.clearActiveBufferRegion();
            return;
        }

        upstreamSource->getNextAudioBlock (info);       // timeline renders first

        float* channels[2] = { info.buffer->getWritePointer (0, info.startSample),
                               info.buffer->getNumChannels() > 1
                                   ? info.buffer->getWritePointer (1, info.startSample)
                                   : info.buffer->getWritePointer (0, info.startSample) };

        chain.process (channels, info.numSamples);      // then DSP shapes it

        // Mono upstream duplicated into stereo output stays phase-identical.
        if (info.buffer->getNumChannels() > 1 && upstreamIsMono)
            juce::FloatVectorOperations::copy (info.buffer->getWritePointer (1, info.startSample),
                                               channels[0], info.numSamples);
    }

    void setUpstreamMono (bool mono)  { upstreamIsMono = mono; }

    void setNextReadPosition (juce::int64 pos) override
    {
        if (upstreamSource != nullptr) upstreamSource->setNextReadPosition (pos);
    }
    juce::int64 getNextReadPosition() const override
    {
        return upstreamSource != nullptr ? upstreamSource->getNextReadPosition() : 0;
    }
    juce::int64 getTotalLength() const override
    {
        return upstreamSource != nullptr ? upstreamSource->getTotalLength() : 0;
    }
    bool isLooping() const override { return false; }

private:
    std::unique_ptr<juce::PositionableAudioSource> upstreamSource;
    otoha::DspChain chain;
    std::optional<otoha::ProcessingState> currentParams;
    bool upstreamIsMono = false;
};
