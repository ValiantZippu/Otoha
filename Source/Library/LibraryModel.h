#pragma once

#include <juce_core/juce_core.h>

#include <cmath>

/*
    LibraryModel — the value types of the media library.

    Deliberately small: only metadata that is actually shown or queried.
    Files are referenced by path; nothing is ever stored as a BLOB.
*/

namespace otoha
{
enum class MediaType { audio, video };

inline const char* mediaTypeToString (MediaType t)
{
    return t == MediaType::audio ? "audio" : "video";
}

inline MediaType mediaTypeFromString (const juce::String& s)
{
    return s.equalsIgnoreCase ("video") ? MediaType::video : MediaType::audio;
}

enum class LibraryFilter { all, audio, video, favorites };

enum class LibrarySort
{
    newestFirst,     // default
    oldestFirst,
    nameAscending,
    nameDescending,
    longestFirst,
    shortestFirst
};

struct MediaItem
{
    juce::int64 id = 0;
    MediaType type = MediaType::audio;
    juce::File file;
    juce::String displayName;          // user-facing; independent of the physical filename
    juce::Time createdAt;
    double durationSeconds = 0.0;
    int sampleRate = 0;
    int channels = 0;
    int bitDepth = 0;
    juce::String format { "WAV" };
    bool favorite = false;
    juce::int64 fileSizeBytes = 0;
    juce::String waveformCachePath;    // Cache/Waveforms/… (audio)
    juce::String thumbnailPath;        // Cache/Thumbnails/… (video, reserved)

    bool isValid() const  { return id != 0 && file.existsAsFile(); }
};

/** Compact friendly date for list rows: "Today", "Yesterday", otherwise a short date. */
inline juce::String friendlyRelativeDate (const juce::Time& t, const juce::Time& now = juce::Time::getCurrentTime())
{
    const int daysDelta = (int) juce::roundToInt (std::floor ((now.getStartOfDay().toMilliseconds()
                                                              - t.getStartOfDay().toMilliseconds()) / 86400000.0));

    if (daysDelta <= 0)  return "Today";
    if (daysDelta == 1)  return "Yesterday";
    if (daysDelta < 7)   return juce::String (daysDelta) + " days ago";

    return t.toString ("%d %b %Y");
}
} // namespace otoha
