#pragma once

/*
    RecorderPhase — the extended recording lifecycle (M12 #15).

    TransportState (idle/recording/paused) remains the AUDIO truth used by the
    working Windows recorder and its tests — untouched. RecorderPhase is the
    USER-VISIBLE lifecycle shared by all platforms, wrapping transport plus the
    phases mobile needs: permission, countdown, stopping/saving, completion,
    interruption.

    One enum, one transition table — no scattered booleans. UI observes; only
    the recorder drives transitions.
*/

namespace otoha
{
enum class RecorderPhase
{
    idle,        // nothing open
    preparing,   // opening device / writer (permission already granted)
    countdown,   // counting down BEFORE capture — never part of the take (#8/#20)
    recording,   // capturing
    paused,      // capture suspended, take still open
    stopping,    // user asked stop; draining/finalizing buffers
    saving,      // flushing to disk, generating waveform metadata
    complete,    // saved and registered in the Library (#10)
    error        // terminal for this attempt; reason in the accompanying error
};

inline const char* toString (RecorderPhase p)
{
    switch (p)
    {
        case RecorderPhase::idle:      return "idle";
        case RecorderPhase::preparing: return "preparing";
        case RecorderPhase::countdown: return "countdown";
        case RecorderPhase::recording: return "recording";
        case RecorderPhase::paused:    return "paused";
        case RecorderPhase::stopping:  return "stopping";
        case RecorderPhase::saving:    return "saving";
        case RecorderPhase::complete:  return "complete";
        case RecorderPhase::error:     return "error";
    }
    return "unknown";
}

/** Valid transitions. Anything not listed is a programming error and must be
    refused (the phase stays unchanged) rather than half-applied. */
inline bool isValidRecorderTransition (RecorderPhase from, RecorderPhase to)
{
    switch (from)
    {
        case RecorderPhase::idle:
            return to == RecorderPhase::preparing;
        case RecorderPhase::preparing:
            return to == RecorderPhase::countdown || to == RecorderPhase::recording
                || to == RecorderPhase::error     || to == RecorderPhase::idle;
        case RecorderPhase::countdown:
            return to == RecorderPhase::recording || to == RecorderPhase::idle   // cancel during count
                || to == RecorderPhase::error;
        case RecorderPhase::recording:
            return to == RecorderPhase::paused  || to == RecorderPhase::stopping
                || to == RecorderPhase::saving  // immediate stop on short takes
                || to == RecorderPhase::error   || to == RecorderPhase::idle;   // hard abort
        case RecorderPhase::paused:
            return to == RecorderPhase::recording || to == RecorderPhase::stopping
                || to == RecorderPhase::error;
        case RecorderPhase::stopping:
            return to == RecorderPhase::saving || to == RecorderPhase::error;
        case RecorderPhase::saving:
            return to == RecorderPhase::complete || to == RecorderPhase::error;
        case RecorderPhase::complete:
            return to == RecorderPhase::idle;       // ready for the next take
        case RecorderPhase::error:
            return to == RecorderPhase::idle;       // acknowledge/reset after failure
    }
    return false;
}

/** Interruptions (#23) always land somewhere defined: an active take is either
    paused cleanly or moved through error with ErrorCategory::audioInterrupted —
    never silently corrupted audio. */
inline bool isInterruptTerminal (RecorderPhase p)
{
    return p == RecorderPhase::paused || p == RecorderPhase::error || p == RecorderPhase::idle;
}
} // namespace otoha
