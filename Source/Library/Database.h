#pragma once

#include <juce_core/juce_core.h>

#include <vector>

#include "LibraryModel.h"

struct sqlite3;

/*
    Database — the only class that talks to SQLite.

    One connection, guarded by a CriticalSection, so the UI thread and any
    background worker can share it safely. The API is deliberately tiny;
    higher-level logic lives in LibraryService. No UI code ever issues SQL.
*/
class Database
{
public:
    Database() = default;
    ~Database();

    Database (const Database&) = delete;
    Database& operator= (const Database&) = delete;

    /** Opens (creating if needed) and initialises/migrates the schema. */
    bool open (const juce::File& databaseFile, juce::String& errorOut);
    void close();

    int schemaVersion() const;                       // PRAGMA user_version
    bool executeRaw (const juce::String& sql);       // escape hatch (tests, maintenance)

    // --- media CRUD ----------------------------------------------------------
    /** Inserts and fills item->id with the new row id. Returns false on failure. */
    bool insertMedia (otoha::MediaItem& item);

    bool updateDisplayName (juce::int64 id, const juce::String& newName);
    bool updateFavorite    (juce::int64 id, bool favorite);

    /** Removes the row only — never touches files on disk. */
    bool removeMedia (juce::int64 id);

    otoha::MediaItem getMedia (juce::int64 id) const;   // invalid item (id 0) when missing
    std::vector<otoha::MediaItem> all() const;

    std::vector<otoha::MediaItem> query (const juce::String& searchText,
                                         otoha::LibraryFilter filter,
                                         otoha::LibrarySort sort) const;

private:
    bool ensureSchema (juce::String& errorOut);
    void runMigrations (int fromVersion);

    mutable juce::CriticalSection lock;
    sqlite3* db = nullptr;
};
