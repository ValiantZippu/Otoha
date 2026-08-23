/*
    StateMachineTest — headless verification of the recording transition rules
    (no audio device required).
*/
#include "../../Source/Audio/RecordingState.h"

#include <cstdio>

using otoha::TransportState;

namespace
{
bool expect (bool condition, const char* message)
{
    if (! condition)
        std::printf ("FAIL: %s\n", message);
    return condition;
}

constexpr TransportState allStates[] = { TransportState::idle,
                                         TransportState::recording,
                                         TransportState::paused };

bool isAllowedPair (TransportState from, TransportState to)
{
    // The exact table from the design: idle->rec, rec->paused|idle, paused->rec|idle.
    if (from == to)
        return false;
    switch (from)
    {
        case TransportState::idle:      return to == TransportState::recording;
        case TransportState::recording: return to == TransportState::paused || to == TransportState::idle;
        case TransportState::paused:    return to == TransportState::recording || to == TransportState::idle;
    }
    return false;
}
} // namespace

int main()
{
    bool ok = true;

    for (auto from : allStates)
        for (auto to : allStates)
        {
            const bool allowed = otoha::isValidTransition (from, to);
            const char* names[] = { "idle", "recording", "paused" };
            auto name = [&] (TransportState s) { return names[(int) s]; };

            ok &= expect (allowed == isAllowedPair (from, to),
                          "transition table mismatch");

            if (allowed)
            {
                char msg[64];
                std::snprintf (msg, sizeof (msg), "%s -> %s should be valid",
                               name (from), name (to));
                ok &= expect (isAllowedPair (from, to), msg);
            }
        }

    // Spot-check the invalid operations the spec calls out:
    ok &= expect (! otoha::isValidTransition (TransportState::idle, TransportState::paused),
                  "cannot pause while idle");
    ok &= expect (! otoha::isValidTransition (TransportState::idle, TransportState::idle),
                  "cannot start a take that is already open / no-op transitions are invalid");
    ok &= expect (otoha::isValidTransition (TransportState::paused, TransportState::idle),
                  "must be able to stop while paused");

    if (! ok) return 1;
    std::printf ("PASS: state machine transitions\n");
    return 0;
}
