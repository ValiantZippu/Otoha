#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

/*
    SampleRateConverter — shared by the editor (cross-rate paste) and the
    exporter (preset sample-rate conversion). One implementation, no copies.

    Linear interpolation is the current quality level; the signature is the
    upgrade path to a windowed-sinc converter without touching callers.
*/
namespace otoha
{
juce::AudioBuffer<float> resampleLinear (const juce::AudioBuffer<float>& input,
                                         double srcRate, double destRate);

juce::AudioBuffer<float> adaptChannels (const juce::AudioBuffer<float>& input, int targetChannels);
} // namespace otoha
