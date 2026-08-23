#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include "Database.h"
#include "WaveformCache.h"

/*
    LibraryService — the single door between the UI and the library.

    UI -> LibraryService -> Database / WaveformCache.
    Keeps the database API small and keeps file operations (scan recovery,
    delete-with-cache-cleanup) in one testable place.

    Layout under the Otoha base directory:
        Library/Audio   Library/Video   Cache/Waveforms   Cache/Thumbnails
        Database/library.sqlite
*/
class LibraryService
{
public:
    struct ScanResult
    {
        int recovered = 0;   // files on disk that were missing from the database
        int staleRemoved = 0; // entries whose files no longer exist
        int unreadable = 0;  // files found but not readable as media (left alone)
    };

    explicit LibraryService (const juce::File& baseDirectory);
    ~LibraryService();

    bool initialise (juce::String& errorOut);

    /** Startup reconciliation: recover unregistered files, drop stale rows.
        Never deletes media; a broken file only skips itself. */
    ScanResult performStartupScan();

    /** Extracts metadata from a finished audio recording and inserts it.
        Returns 0 on failure — the caller must NOT delete the file in that case;
        the next startup scan will recover it. */
    juce::int64 registerAudioFile (const juce::File& file);

    /** Display name is stored separately from the physical filename, so rename
        can never lose the file. */
    bool rename (juce::int64 id, const juce::String& newDisplayName);
    bool setFavorite (juce::int64 id, bool favorite);

    /** Removes the row, moves the media to trash where possible (falls back to
        deletion), and cleans its waveform cache. Returns false if the row was
        removed but the file could not be fully cleaned up. */
    bool deleteMedia (juce::int64 id);

    std::vector<otoha::MediaItem> query (const juce::String& searchText,
                                         otoha::LibraryFilter filter,
                                         otoha::LibrarySort sort) const;

    otoha::MediaItem get (juce::int64 id) const { return db.getMedia (id); }

    WaveformCache& getWaveformCache()             { return waveformCache; }
    const juce::File& getBaseDirectory() const    { return baseDir; }

private:
    juce::File baseDir;

    Database db;
    WaveformCache waveformCache;
    juce::AudioFormatManager formatManager;
};

/** Platform-appropriate Otoha root: <Music>/Otoha (fallback <Home>/Otoha). */
juce::File otohaBaseDirectory();
