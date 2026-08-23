#include "SampleRateConverter.h"

#include <algorithm>
#include <cmath>

namespace otoha
{
juce::AudioBuffer<float> resampleLinear (const juce::AudioBuffer<float>& input,
                                         double srcRate, double destRate)
{
    if (srcRate == destRate || srcRate <= 0.0 || destRate <= 0.0 || input.getNumSamples() == 0)
        return input;

    const double ratio = srcRate / destRate;                 // >1 when reducing samples
    const int outLength = (int) std::ceil ((double) input.getNumSamples() / ratio);
    const int channels = input.getNumChannels();

    juce::AudioBuffer<float> output (channels, outLength);
    const int lastIndex = input.getNumSamples() - 1;

    for (int ch = 0; ch < channels; ++ch)
    {
        const auto* in = input.getReadPointer (ch);
        auto* out = output.getWritePointer (ch);

        for (int i = 0; i < outLength; ++i)
        {
            const double pos = (double) i * ratio;
            const int i0 = juce::jlimit (0, lastIndex, (int) pos);
            const int i1 = juce::jmin (i0 + 1, lastIndex);
            const double frac = pos - (double) i0;

            out[i] = (float) ((1.0 - frac) * (double) in[i0] + frac * (double) in[i1]);
        }
    }
    return output;
}

juce::AudioBuffer<float> adaptChannels (const juce::AudioBuffer<float>& input, int targetChannels)
{
    const int channels = input.getNumChannels();
    if (channels == targetChannels || channels <= 0 || targetChannels <= 0 || input.getNumSamples() == 0)
        return input;

    juce::AudioBuffer<float> output (targetChannels, input.getNumSamples());

    if (channels == 1 && targetChannels == 2)      // mono -> stereo: duplicate
    {
        output.copyFrom (0, 0, input, 0, 0, input.getNumSamples());
        output.copyFrom (1, 0, input, 0, 0, input.getNumSamples());
    }
    else if (channels == 2 && targetChannels == 1) // stereo -> mono: equal-power-ish average
    {
        for (int i = 0; i < input.getNumSamples(); ++i)
            output.setSample (0, i, 0.5f * (input.getSample (0, i) + input.getSample (1, i)));
    }
    else
    {
        for (int ch = 0; ch < targetChannels; ++ch)
            output.copyFrom (ch, 0, input, juce::jmin (ch, channels - 1), 0, input.getNumSamples());
    }
    return output;
}
} // namespace otoha
