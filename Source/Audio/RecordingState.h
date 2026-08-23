#pragma once

/*
    RecordingState — the single source of truth for recording transport state.

    This lives outside any UI or hardware class so the transition rules can be
    tested headlessly. The Recorder (audio engine) OWNS its state here; the UI
    only observes it and requests transitions.
*/

namespace otoha
{
enum class TransportState
{
    idle,       // no take open
    recording,  // capturing samples to the open file
    paused      // take open, capture suspended (one continuous logical recording)
};

/** Valid transitions:
        idle -> recording        start a new take
        recording -> paused      pause
        paused -> recording      resume
        recording -> idle        stop / finish
        paused -> idle           stop while paused
*/
inline bool isValidTransition (TransportState from, TransportState to)
{
    switch (from)
    {
        case TransportState::idle:      return to == TransportState::recording;
        case TransportState::recording: return to == TransportState::paused || to == TransportState::idle;
        case TransportState::paused:    return to == TransportState::recording || to == TransportState::idle;
    }
    return false;
}

/** Why an active take had to end abnormally (reported to the UI, never thrown). */
enum class FailureReason
{
    none,
    diskFull,    // the writer could not persist samples
    deviceLost   // sample rate/channel change or device closed mid-take
};

inline const char* toString (TransportState s)
{
    switch (s)
    {
        case TransportState::idle:      return "idle";
        case TransportState::recording: return "recording";
        case TransportState::paused:    return "paused";
    }
    return "unknown";
}
} // namespace otoha
