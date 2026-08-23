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
    podcast
};

juce::String presetToString (DspPreset p);
DspPreset    presetFromString (const juce::String& s);

/** Returns the parameter set for a preset (off => neutral/bypassed). */
ProcessingState presetToState (DspPreset p);

/** True when the state differs from its preset's canonical values. */
bool stateDiffersFromPreset (const ProcessingState& state, DspPreset p);
} // namespace otoha
