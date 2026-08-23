#pragma once

#include "ProcessingState.h"

/*
    Presets — the single place where Enhance preset values live.

    Tuning a preset must never require touching the audio engine or UI:
    edit the table in Presets.cpp only. Values start deliberately conservative
    and should be refined by listening tests, never by theory alone.
*/
namespace otoha
{
enum class DspPreset
{
    off,        // DSP disabled / neutral
    natural,
    voice,
    vocal,
    music,
    acoustic,
    live,
    podcast,
    // --- M8: Otoha Sound additions (same engine, same table) -----------------
    bass,       // low-frequency lift for speakers that need help
    clarity     // presence/intelligibility lift
};

/** Number of enumerable presets (including off) — keeps UI loops honest. */
inline constexpr int kNumDspPresets = 10;

/** Ordered list of every preset — UI combos must iterate this, not hardcode. */
juce::Array<DspPreset> allDspPresets();

juce::String presetToString (DspPreset p);
DspPreset    presetFromString (const juce::String& s);

/** Returns the parameter set for a preset (off => neutral/bypassed). */
ProcessingState presetToState (DspPreset p);

/** True when the state differs from its preset's canonical values. */
bool stateDiffersFromPreset (const ProcessingState& state, DspPreset p);
} // namespace otoha
