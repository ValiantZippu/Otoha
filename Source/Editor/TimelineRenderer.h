#pragma once

#include <atomic>
#include <functional>
#include <memory>

#include <juce_audio_formats/juce_audio_formats.h>

#include "../Dsp/ProcessingState.h"
#include "AudioDocument.h"

/*
    TimelineRenderer — the offline half of the shared DSP pipeline:

        source -> edit timeline -> [DSP chain] -> renderer -> output

    The same DspChain definition used by real-time preview processes each
    chunk here; there is no separate export algorithm. Supports cancellation
    (checked between chunks) and never touches `destination` unless the whole
    render succeeded.
*/
namespace otoha
{
class TimelineRenderer
{
public:
    explicit TimelineRenderer (std::shared_ptr<const AudioDocument> document);

    juce::int64 getRenderedLengthSamples() const;

    /** Renders through `format` (WAV, FLAC, ...). Pass a ProcessingState to
        include processing (only meaningful when state.enabled). */
    bool renderToFile (juce::AudioFormat& format,
                       const juce::File& destination,
                       juce::String& errorOut,
                       const ProcessingState* dsp = nullptr,
                       const std::atomic<bool>* cancelFlag = nullptr,
                       const std::function<bool (float progress01)>& progress = {}) const;

    /** WAV convenience kept for existing callers. */
    bool renderToWav (const juce::File& destination,
                      juce::String& errorOut,
                      const ProcessingState* dsp = nullptr,
                      const std::atomic<bool>* cancelFlag = nullptr) const;

private:
    std::shared_ptr<const AudioDocument> doc;
};
} // namespace otoha
