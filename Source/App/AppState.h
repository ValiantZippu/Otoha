#pragma once

#include "../Audio/RecorderPhase.h"
#include "../Core/OtohaError.h"

#include <juce_core/juce_core.h>

/*
    AppState — the centralized observation point for application state
    (M13 #6). It does NOT own the subsystems (those live where they are:
    AudioDocument, DspChain, ExportManager, backends); it mirrors their
    user-relevant state in one place so the UI has a single coherent view
    instead of dozens of scattered booleans.

    Rule: subsystems update AppState; the UI only READS it. Contradictory
    flags become impossible because each fact has exactly one field.
*/

namespace otoha
{
enum class PlaybackState { stopped, playing, paused };
enum class ExportState   { idle, rendering, encoding, cancelling, failed, done };

struct DeviceState
{
    juce::String inputName;      // friendly name or {} when none
    juce::String outputName;
    double sampleRate = 0.0;     // 0 == unknown/none
    bool available = false;
};

struct AppState
{
    // --- document / project -------------------------------------------------
    juce::String projectTitle;
    bool projectModified  = false;

    // --- recording ----------------------------------------------------------
    RecorderPhase recorderPhase = RecorderPhase::idle;

    // --- playback -------------------------------------------------------------
    PlaybackState playback = PlaybackState::stopped;
    juce::int64 playheadSamples = 0;

    // --- editing ---------------------------------------------------------------
    juce::int64 selectionStart = 0;
    juce::int64 selectionEnd   = 0;
    bool canUndo = false;
    bool canRedo = false;

    // --- dsp / export / devices --------------------------------------------------
    bool enhanceEnabled = false;
    juce::String activePreset;             // portable preset name (#40/#42)
    ExportState exportState = ExportState::idle;
    int exportsCompleted = 0;
    int exportsTotal = 0;
    DeviceState device;

    OtohaError lastError;                  // category + diagnostics-only detail

    void invalidate (OtohaError e)         // one place errors funnel through (#51)
    {
        lastError = std::move (e);
        if (lastError.category == ErrorCategory::audioInterrupted)
            recorderPhase = isInterruptTerminal (recorderPhase) ? recorderPhase : RecorderPhase::error;
    }
};
} // namespace otoha
