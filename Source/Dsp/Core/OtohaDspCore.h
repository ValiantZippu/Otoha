#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>

#include "../ProcessingState.h"

/*
    Otoha DSP Core — the portable, UI-free processing engine shared by
    Otoha Studio and the future Otoha Sound.

    Rules (enforced by convention and reviewed in docs/dsp.md):
      * process() is REAL-TIME SAFE: no allocation, no locks (beyond tiny POD
        snapshot copies), no file/network/database/UI access.
      * prepare() runs off the audio thread and does all resource setup.
      * Processors receive everything they need via ProcessingContext — they
        never query the operating system.
      * Parameters cross threads as small POD snapshots; audible parameters
        are smoothed inside process() so slider moves never click.

    The core depends only on juce_audio_basics (portable DSP containers) and
    juce_core. It knows nothing about Library, Editor, FFmpeg, filesystems,
    or platform audio APIs.
*/

namespace otoha::dsp
{
/** Immutable configuration handed to processors at prepare() time. */
struct ProcessingContext
{
    double sampleRate  = 48000.0;
    int    numChannels = 2;
    int    maxBlockSize = 512;
};

/** Non-owning view over interleaved-planar audio: `channels[c]` -> frames. */
class AudioBlock
{
public:
    AudioBlock (float* const* channelPointers, int channels, int frames)
        : channelData (channelPointers), numChannels (channels), numFrames (frames) {}

    float* const* channelData;
    int numChannels;
    int numFrames;
};

/**
    Common interface so ANY processor can sit in the SAME chain.

    Implementations in Core/Processors.h:
        GainProcessor, BassProcessor, ClarityProcessor, StereoWidthProcessor,
        EqProcessor, CompressorProcessor, LimiterProcessor,
        NoiseReductionProcessor, MeterProcessor
*/
class DspProcessor
{
public:
    virtual ~DspProcessor() = default;

    virtual void prepare (const ProcessingContext& context) = 0;
    virtual void process (AudioBlock& block) = 0;              // real-time safe
    virtual void reset() = 0;

    /** Message thread. Publishes the processor's slice of the DSP state. */
    virtual void setParameters (const ProcessingState& state) = 0;
};

/** One-pole parameter smoother: slider moves become inaudible ramps. */
class SmoothedFloat
{
public:
    void reset (float value)                    { current = target = value; }
    void setTarget (float newTarget)            { target = newTarget; }
    void setTimeConstant (float ms, double sampleRate)
    {
        const float seconds = juce::jmax (0.5f, ms) * 0.001f;
        coefficient = std::exp (-1.0f / (seconds * (float) sampleRate));
    }

    float next()
    {
        current += (target - current) * (1.0f - coefficient);
        return current;
    }
    float getCurrent() const  { return current; }
    float getTarget() const   { return target; }

private:
    float current = 0.0f, target = 0.0f, coefficient = 0.0f;
};

/** Read-only metering snapshot for the UI (atomics; never blocks audio). */
struct MeterSnapshot
{
    std::atomic<float> peak              { 0.0f };
    std::atomic<float> rms               { 0.0f };
    std::atomic<float> limiterReductionDb { 0.0f };   // how hard the limiter works
};
} // namespace otoha::dsp
