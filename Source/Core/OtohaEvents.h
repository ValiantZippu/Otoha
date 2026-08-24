#pragma once

#include <juce_core/juce_core.h>

#include <functional>
#include <vector>

/*
    OtohaEvents — the minimal notification mechanism (M13 #7).

    The audio engine and services NEVER call UI widgets; they broadcast typed
    events here and interested UI observes. Deliberately tiny:

      * listeners live on the message thread only (register from the message
        thread; broadcast is documented as message-thread or a thread-safe
        async wrapper at the call site)
      * no dynamic priority/queue machinery — if a future need appears, it
        gets added then, not speculatively

    Event list mirrors the milestone: recording/playback lifecycle, selection,
    project, device, DSP and export progress events.
*/

namespace otoha
{
enum class AppEvent
{
    recordingStarted,
    recordingStopped,
    playbackStarted,
    playbackStopped,
    selectionChanged,
    projectChanged,
    deviceChanged,
    dspChanged,
    exportStarted,
    exportProgress,     // payload via broadcast context below
    exportFinished,
    libraryChanged
};

class EventBroadcaster
{
public:
    using Listener = std::function<void (AppEvent)>;

    void addListener (Listener listener) { listeners.push_back (std::move (listener)); }

    /** Fire-and-forget notify. Message thread only. */
    void broadcast (AppEvent event)
    {
        for (auto& listener : listeners)
            listener (event);
    }

private:
    std::vector<Listener> listeners;
};
} // namespace otoha
