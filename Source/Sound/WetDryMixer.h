#pragma once

#include "../Dsp/Core/OtohaDspCore.h"
#include "../Dsp/DspChain.h"

#include <vector>

/*
    WetDryMixer — Otoha Sound's "Enhance" amount, implemented as a smoothed
    dry/wet crossfade between the untouched input and the DSP chain output.

    Real-time safe after prepare(): all scratch memory is preallocated,
    no locks, one SmoothedFloat per call. 0% = pure passthrough (chain still
    runs so meters stay live, but its output is fully faded out), 100% =
    fully enhanced.

    The crossfade — rather than blind parameter interpolation — is what makes
    a continuous Enhance slider safe: every processor always runs at exactly
    its preset values, so no intermediate parameter combination can ever be
    unstable.
*/
class WetDryMixer
{
public:
    void prepare (const otoha::dsp::ProcessingContext& context)
    {
        channels = context.numChannels;
        capacity = context.maxBlockSize;
        dry.assign ((size_t) channels * (size_t) capacity, 0.0f);
        mix.reset (0.0f);
        mix.setTimeConstant (40.0f, context.sampleRate);   // ~40 ms: click-free
    }

    /** Real-time safe. `block` must be within the prepared bounds. */
    void process (otoha::dsp::AudioBlock& block, otoha::DspChain& chain)
    {
        const int frames = block.numFrames;
        if (frames > capacity) return;                      // defensive; never in practice

        // Snapshot the dry signal first (chain processes in place).
        for (int c = 0; c < block.numChannels; ++c)
            std::copy_n (block.channelData[c], (size_t) frames,
                         dry.data() + (size_t) c * (size_t) capacity);

        chain.process (block.channelData, frames);

        // Crossfade with a per-block-smoothed gain (one smoother step/block).
        float g = mix.next();
        if (g >= 0.999f)
            return;                                         // fully wet: nothing to do

        for (int c = 0; c < block.numChannels; ++c)
        {
            auto* out = block.channelData[c];
            const float* dryCh = dry.data() + (size_t) c * (size_t) capacity;
            for (int i = 0; i < frames; ++i)
                out[i] = out[i] * g + dryCh[i] * (1.0f - g);
        }
    }

    /** Message thread. */
    void setMix (float newMix) { mix.setTarget (juce::jlimit (0.0f, 1.0f, newMix)); }
    float getMix() const { return mix.getTarget(); }

private:
    std::vector<float> dry;
    otoha::dsp::SmoothedFloat mix;
    int channels = 2;
    int capacity = 512;
};
