#pragma once

#include "../Dsp/Core/OtohaDspCore.h"
#include "../Dsp/DspChain.h"
#include "../Dsp/ProcessingState.h"
#include "WetDryMixer.h"

#include <atomic>

/*
    SoundEngine — Otoha Sound's real-time processing core.

    Sits between a live AudioBackend and the physical output:

        backend capture -> SoundEngine::process -> backend render

    It owns one otoha::DspChain (the SAME chain Studio uses — no duplicate
    processors) plus the WetDryMixer that implements the Enhance amount, and
    exposes read-only meters + diagnostics for the UI.

    Real-time rules (see docs/dsp.md): process() allocates nothing, takes no
    locks, touches no disk/UI. All state the UI changes crosses via atomics
    or message-thread setters that processors consume safely.
*/
class SoundEngine
{
public:
    /** Control thread. Allocates all buffers; call again if config changes. */
    void prepare (const otoha::dsp::ProcessingContext& context)
    {
        chain.prepare (context.sampleRate, context.numChannels);
        mixer.prepare (context);
        mixer.setMix (enhanceAmount.load (std::memory_order_relaxed));
        prepared = true;
    }

    /** Real-time safe. In-place: process `block` (already format-matched by
        the backend to the negotiated stream). Disabled = pure passthrough. */
    void process (float* const* channels, int numChannels, int numFrames)
    {
        if (! prepared || numFrames <= 0 || numChannels <= 0)
            return;

        otoha::dsp::AudioBlock block (channels, numChannels, numFrames);

        if (! enabled.load (std::memory_order_relaxed))
        {
            ++blocksPassed;
            return;                                     // clean bypass: no DSP at all
        }

        mixer.process (block, chain);
        ++blocksProcessed;
    }

    // --- control thread ------------------------------------------------------

    /** Master ON/OFF. Bypass is a flag flip — never restarts audio devices. */
    void setEnabled (bool on) { enabled.store (on, std::memory_order_relaxed); }
    bool isEnabled() const { return enabled.load (std::memory_order_relaxed); }

    /** Enhance amount 0..1 → wet/dry crossfade toward the active preset. */
    void setEnhanceAmount (float amount)
    {
        enhanceAmount.store (juce::jlimit (0.0f, 1.0f, amount), std::memory_order_relaxed);
        mixer.setMix (enhanceAmount.load (std::memory_order_relaxed));
    }
    float getEnhanceAmount() const { return enhanceAmount.load (std::memory_order_relaxed); }

    /** Full DSP parameter publish (preset apply, slider edits). Message thread. */
    void setParameters (const otoha::ProcessingState& p) { chain.setParameters (p); }

    struct Meters { float peak = 0.0f, rms = 0.0f, limiterReductionDb = 0.0f; };

    /** Safe snapshot for UI polling (atomics underneath). */
    Meters getMeters() const
    {
        const auto m = chain.getMeters();
        return { m.peak, m.rms, m.limiterReductionDb };
    }

    /** Lightweight diagnostics (#30) — counters, not logs. */
    struct Stats
    {
        unsigned long long blocksPassed = 0, blocksProcessed = 0;
    };
    Stats getStats() const
    {
        return { blocksPassed.load (std::memory_order_relaxed),
                 blocksProcessed.load (std::memory_order_relaxed) };
    }

private:
    otoha::DspChain chain;
    WetDryMixer mixer;

    std::atomic<bool> enabled { false };
    std::atomic<float> enhanceAmount { 1.0f };
    std::atomic<unsigned long long> blocksPassed { 0 }, blocksProcessed { 0 };

    bool prepared = false;
    int chainChannels = 2;
};
