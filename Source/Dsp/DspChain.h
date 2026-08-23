#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <vector>

#include "ProcessingState.h"

/*
    DspChain — THE audio processing implementation. There is exactly one.

    Processing order (explicit and deterministic, never UI-driven):

        Noise Reduction -> EQ -> Compressor -> Limiter

    Usage contract:
      * prepare(sampleRate, numChannels)  — message thread / before rendering
      * setParameters(ProcessingState)    — message thread; POD snapshot swap
      * process(float**, int frames)      — real-time safe: no allocation,
                                            no locks held beyond a tiny POD
                                            copy, no I/O
    The SAME object definition powers:
        preview   : DspPreviewSource wraps the timeline source with one chain
        offline   : TimelineRenderer streams chunks through another instance
    Both consume identical ProcessingState data, so outputs match within
    normal floating-point tolerance.
*/
namespace otoha
{
class DspChain
{
public:
    DspChain() = default;

    void prepare (double sampleRate, int numChannels);
    void reset();

    /** Message-thread only. Copies the state into the audio-side snapshot. */
    void setParameters (const ProcessingState& p);

    /** Real-time safe. Channels must match the prepared channel count. */
    void process (float* const* channels, int numFrames);

    bool isPrepared() const { return sampleRate > 0.0; }

private:
    // --- internal processors (kept here; each small and self-contained) ------
    void rebuildEqCoefficients();
    float processNoiseReduction (int ch, float sample);
    float eqStage (float sample);
    void updateCompressorGain (float linkedPeak);
    void updateLimiterGain (float linkedPeak);

    double sampleRate = 0.0;
    int numChannels = 0;

    // Parameters: `incoming` is published by the message thread under a spin
    // lock (POD copy); `working` is owned exclusively by the audio side and
    // pulled once per block. No tearing, no blocking.
    juce::SpinLock paramLock;
    ProcessingState incoming;
    ProcessingState working;

    // EQ: five biquad bands (low shelf / 3 x peaking / high shelf).
    juce::IIRFilter eqFilters[5];
    bool eqDirty = true;
    float appliedEqGains[5] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

    // Noise reduction: 85 Hz high-pass + downward expander per channel.
    juce::IIRFilter nrHighPass;
    bool nrHighPassDirty = true;
    std::vector<float> nrEnvelope;       // smoothed |x| follower per channel
    std::vector<float> nrGain;           // current expansion gain per channel

    // Compressor: linked peak detector + smoothed gain computer.
    float compEnvelope = 0.0f;
    float compGainSmoothed = 1.0f;

    // Limiter: linked peak, instant attack / exponential release.
    float limGain = 1.0f;
};

} // namespace otoha
