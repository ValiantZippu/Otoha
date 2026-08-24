#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <vector>

#include "OtohaDspCore.h"

/*
    Otoha DSP Core processors.

    Migration note (M7): EQ / Compressor / Limiter / NoiseReduction are the
    Milestone 5 algorithms moved into processor classes with their math
    preserved (same coefficients, same envelope/smoothing constants, same
    linked-detector dynamics). Two deliberate correctness fixes are documented
    in docs/dsp.md: filter state is now PER CHANNEL instead of shared across
    channels (mono output is bit-identical; stereo differs only by removing
    inter-channel filter crosstalk).
*/

namespace otoha::dsp
{
// =============================================================================
// Gain — input/output trims with smoothed gains (#40). Useful everywhere.
// =============================================================================
class GainProcessor : public DspProcessor
{
public:
    void prepare (const ProcessingContext& context) override;
    void process (AudioBlock& block) override;
    void reset() override;
    void setParameters (const ProcessingState& state) override;

private:
    SmoothedFloat inputGain  { }, outputGain { };
    float inputGainDbTarget = 0.0f, outputGainDbTarget = 0.0f;
};

// =============================================================================
// Bass — low-shelf lift with gain staging (amount 0..1 -> up to +6 dB @ 90 Hz).
// Never crude boost: shelf shape keeps mids intact and the chain limiter owns
// peak safety.
// =============================================================================
class BassProcessor : public DspProcessor
{
public:
    void prepare (const ProcessingContext& context) override;
    void process (AudioBlock& block) override;
    void reset() override;
    void setParameters (const ProcessingState& state) override;

private:
    // juce::IIRFilter is non-copyable in JUCE 8 (SpinLock member), so the
    // per-channel filters live behind unique_ptrs.
    std::vector<std::unique_ptr<juce::IIRFilter>> shelf;      // per channel (M7 fix)
    SmoothedFloat gainDb;
    float amountTarget = 0.0f;
    double sampleRate = 0.0;
    bool dirtyShelf = true;
};

// =============================================================================
// Clarity — controlled presence: peaking lift at 3.5 kHz plus a gentle high
// shelf, scaled by amount (0..1). No massive HF gain, no harshness by design.
// =============================================================================
class ClarityProcessor : public DspProcessor
{
public:
    void prepare (const ProcessingContext& context) override;
    void process (AudioBlock& block) override;
    void reset() override;
    void setParameters (const ProcessingState& state) override;

private:
    std::vector<std::unique_ptr<juce::IIRFilter>> presence;   // per channel
    std::vector<std::unique_ptr<juce::IIRFilter>> air;        // per channel
    SmoothedFloat presenceGainDb, airGainDb;
    float amountTarget = 0.0f;
    double sampleRate = 0.0;
    bool dirty = true;
};

// =============================================================================
// StereoWidth — mid/side width. 0.5 = normal stereo, >0.5 wider, <0.5 narrower.
// Mono stays mono (passthrough); processing is sample-symmetric so no phase
// instability is introduced.
// =============================================================================
class StereoWidthProcessor : public DspProcessor
{
public:
    void prepare (const ProcessingContext& context) override;
    void process (AudioBlock& block) override;
    void reset() override;
    void setParameters (const ProcessingState& state) override;

private:
    SmoothedFloat widthFactor;   // 0..2 applied to the side signal
};

// =============================================================================
// EQ — five bands (low shelf / 3 x peaking / high shelf), per-channel filters.
// Neutral at 0 dB by contract.
// =============================================================================
class EqProcessor : public DspProcessor
{
public:
    void prepare (const ProcessingContext& context) override;
    void process (AudioBlock& block) override;
    void reset() override;
    void setParameters (const ProcessingState& state) override;

private:
    void rebuildCoefficients();
    std::vector<std::array<std::unique_ptr<juce::IIRFilter>, 5>> filters;   // [channel][band]
    ProcessingState pendingState;
    float appliedGains[5] = { 0, 0, 0, 0, 0 };
    bool dirty = true;
    double sampleRate = 0.0;
    int numChannelsPrepared = 0;
};

// =============================================================================
// Compressor — feed-forward peak compressor, hard knee, smoothed gain.
// Detector is LINKED across channels so the stereo image never pumps unevenly.
// ============================================================================
class CompressorProcessor : public DspProcessor
{
public:
    void prepare (const ProcessingContext& context) override;
    void process (AudioBlock& block) override;
    void reset() override;
    void setParameters (const ProcessingState& state) override;

private:
    float envelope = 0.0f;
    float gainSmoothed = 1.0f;
    double preparedSampleRate = 48000.0;
    ProcessingState params {};
};

// =============================================================================
// Limiter — final safety stage. Instant attack, exponential release.
// Publishes its gain reduction to the meter snapshot.
// =============================================================================
class LimiterProcessor : public DspProcessor
{
public:
    void prepare (const ProcessingContext& context) override;
    void process (AudioBlock& block) override;
    void reset() override;
    void setParameters (const ProcessingState& state) override;

    MeterSnapshot meters;   // limiterReductionDb is live here

private:
    float gain = 1.0f;
    double preparedSampleRate = 48000.0;
    ProcessingState params {};
};

// =============================================================================
// NoiseReduction — 85 Hz high-pass plus a downward expander tuned by mode
// (Off/Gentle/Strong). Per-channel envelope and gain state (M7 fix).
// =============================================================================
class NoiseReductionProcessor : public DspProcessor
{
public:
    void prepare (const ProcessingContext& context) override;
    void process (AudioBlock& block) override;
    void reset() override;
    void setParameters (const ProcessingState& state) override;

private:
    struct NrTuning { float thresholdDb, depthDb, attackMs, releaseMs; };
    static NrTuning tuningFor (NoiseReductionMode mode, float strength);

    std::vector<std::unique_ptr<juce::IIRFilter>> highPass;   // per channel
    bool highPassDirty = true;
    std::vector<float> envelope;
    std::vector<float> gain;
    double sampleRateStored = 48000.0;
    ProcessingState params {};
};

// =============================================================================
// Meter — read-only tap: peak/RMS atomics, audio untouched otherwise.
// Place it anywhere in the chain (Studio puts it after the limiter).
// =============================================================================
class MeterProcessor : public DspProcessor
{
public:
    void prepare (const ProcessingContext& context) override;
    void process (AudioBlock& block) override;
    void reset() override;
    void setParameters (const ProcessingState&) override {}

    MeterSnapshot meters;

private:
    double sampleRate = 0.0;
    int holdCounter = 0;
};

} // namespace otoha::dsp
