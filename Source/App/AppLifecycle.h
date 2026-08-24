#pragma once

#include <functional>

/*
    AppLifecycle — the one place that knows what state Otoha is in (#23).

    Replaces scattered booleans (isRunning/isEnabled/isStarted/isProcessing)
    with an explicit, exhaustively-tested transition table:

        starting -> ready -> processing <-> bypassed
                       |         |             |
                       v         v             v
                  recovering -> processing | bypassed | ready
                       |
                       v
                 unavailable -> ready | recovering
        any (except starting) -> stopping

    Pure logic: no JUCE, no threads, no OS. The UI and backend report events;
    this type decides whether they make sense.
*/
namespace otoha
{
enum class AppState
{
    starting,
    ready,
    processing,
    bypassed,
    recovering,
    unavailable,
    stopping
};

inline const char* appStateToString (AppState s)
{
    switch (s)
    {
        case AppState::starting:    return "Starting";
        case AppState::ready:       return "Ready";
        case AppState::processing:  return "ON";           // user-facing wording (#18)
        case AppState::bypassed:    return "OFF";
        case AppState::recovering:  return "Recovering";
        case AppState::unavailable: return "Unavailable";
        case AppState::stopping:    return "Stopping";
    }
    return "?";
}

inline bool isValidAppStateTransition (AppState from, AppState to)
{
    if (from == to) return false;

    switch (from)
    {
        case AppState::starting:
            return to == AppState::ready || to == AppState::stopping;

        case AppState::ready:
            return to == AppState::processing || to == AppState::bypassed
                || to == AppState::recovering || to == AppState::unavailable
                || to == AppState::stopping;

        case AppState::processing:
        case AppState::bypassed:
            return to == AppState::processing || to == AppState::bypassed   // ON/OFF flips
                || to == AppState::recovering || to == AppState::unavailable
                || to == AppState::stopping;

        case AppState::recovering:
            return to == AppState::processing || to == AppState::bypassed
                || to == AppState::ready       || to == AppState::unavailable
                || to == AppState::stopping;

        case AppState::unavailable:
            return to == AppState::ready || to == AppState::recovering
                || to == AppState::stopping;

        case AppState::stopping:
            return false;   // terminal
    }
    return false;
}

class AppLifecycle
{
public:
    explicit AppLifecycle (AppState initial = AppState::starting) : current_ (initial) {}

    AppState current() const { return current_; }

    /** Requests a transition; illegal requests are refused, never applied. */
    bool request (AppState next)
    {
        if (! isValidAppStateTransition (current_, next))
            return false;
        current_ = next;
        if (onChanged) onChanged (current_);
        return true;
    }

    /** Convenience: ON/OFF flip from either active state. */
    bool setPower (bool on)
    {
        return request (on ? AppState::processing : AppState::bypassed);
    }

    std::function<void (AppState)> onChanged;   // UI hook (message thread)

private:
    AppState current_;
};
} // namespace otoha
