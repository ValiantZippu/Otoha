#include "Database.h"

#include <sqlite3.h>

namespace
{
constexpr int currentSchemaVersion = 1;

juce::String columnText (sqlite3_stmt* stmt, int col)
{
    const auto* text = sqlite3_column_text (stmt, col);
    return juce::String::fromUTF8 (text == nullptr ? "" : (const char*) text);
}

otoha::MediaItem rowToItem (sqlite3_stmt* stmt)
{
    otoha::MediaItem item;
    item.id                = sqlite3_column_int64 (stmt, 0);
    item.type              = otoha::mediaTypeFromString (columnText (stmt, 1));
    item.file              = juce::File (columnText (stmt, 2));
    item.displayName       = columnText (stmt, 3);
    item.createdAt         = juce::Time (sqlite3_column_int64 (stmt, 4));
    item.durationSeconds   = sqlite3_column_double (stmt, 5);
    item.sampleRate        = sqlite3_column_int (stmt, 6);
    item.channels          = sqlite3_column_int (stmt, 7);
    item.bitDepth          = sqlite3_column_int (stmt, 8);
    item.format            = columnText (stmt, 9);
    item.favorite          = sqlite3_column_int (stmt, 10) != 0;
    item.fileSizeBytes     = sqlite3_column_int64 (stmt, 11);
    item.thumbnailPath     = columnText (stmt, 12);
    item.waveformCachePath = columnText (stmt, 13);
    return item;
}

/** Escapes SQL LIKE wildcards so user search text matches literally. */
juce::String escapeLike (juce::String text)
{
    return text.replace ("\\", "\\\\")
               .replace ("%", "\\%")
               .replace ("_", "\\_");
}

const char* selectColumns =
    "id, type, file_path, display_name, created_at, duration, sample_rate, "
    "channels, bit_depth, format, favorite, file_size, thumbnail_path, waveform_path";
} // namespace

// =============================================================================
// Lifecycle
// =============================================================================
Database::~Database() { close(); }

bool Database::open (const juce::File& databaseFile, juce::String& errorOut)
{
    const juce::ScopedLock sl (lock);

    if (db != nullptr)
        return true;

    databaseFile.getParentDirectory().createDirectory();

    if (sqlite3_open (databaseFile.getFullPathName().toUTF8(), &db) != SQLITE_OK)
    {
        errorOut = "Could not open the library database:\n" + databaseFile.getFullPathName();
        db = nullptr;
        return false;
    }

    // Local-first defaults: WAL keeps the UI responsive while a worker writes.
    sqlite3_exec (db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);

    if (! ensureSchema (errorOut))
    {
        close();
        return false;
    }
    return true;
}

void Database::close()
{
    const juce::ScopedLock sl (lock);
    if (db != nullptr)
        sqlite3_close (db);
    db = nullptr;
}

int Database::schemaVersion() const
{
    const juce::ScopedLock sl (lock);
    if (db == nullptr) return 0;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2 (db, "PRAGMA user_version;", -1, &stmt, nullptr) != SQLITE_OK)
        return 0;
    const int version = sqlite3_step (stmt) == SQLITE_ROW ? sqlite3_column_int (stmt, 0) : 0;
    sqlite3_finalize (stmt);
    return version;
}

bool Database::executeRaw (const juce::String& sql)
{
    const juce::ScopedLock sl (lock);
    return db != nullptr && sqlite3_exec (db, sql.toRawUTF8(), nullptr, nullptr, nullptr) == SQLITE_OK;
}

bool Database::ensureSchema (juce::String& errorOut)
{
    const int version = schemaVersion();

    if (version > currentSchemaVersion)
    {
        errorOut = "This library was created by a newer version of Otoha.";
        return false;
    }

    if (version < 1)
    {
        const char* createSql =
            "BEGIN;"
            "CREATE TABLE IF NOT EXISTS media ("
            " id INTEGER PRIMARY KEY AUTOINCREMENT,"
            " type TEXT NOT NULL DEFAULT 'audio',"
            " file_path TEXT NOT NULL UNIQUE,"
            " display_name TEXT NOT NULL,"
            " created_at INTEGER NOT NULL,"
            " duration REAL NOT NULL DEFAULT 0,"
            " sample_rate INTEGER NOT NULL DEFAULT 0,"
            " channels INTEGER NOT NULL DEFAULT 0,"
            " bit_depth INTEGER NOT NULL DEFAULT 0,"
            " format TEXT NOT NULL DEFAULT 'WAV',"
            " favorite INTEGER NOT NULL DEFAULT 0,"
            " file_size INTEGER NOT NULL DEFAULT 0,"
            " thumbnail_path TEXT NOT NULL DEFAULT '',"
            " waveform_path TEXT NOT NULL DEFAULT '');"
            "CREATE INDEX IF NOT EXISTS idx_media_created ON media (created_at);"
            "COMMIT;";

        char* errorMessage = nullptr;
        const bool ok = sqlite3_exec (db, createSql, nullptr, nullptr, &errorMessage) == SQLITE_OK;
        if (! ok)
        {
            errorOut = "Could not create the library database: "
                       + juce::String::fromUTF8 (errorMessage == nullptr ? "?" : errorMessage);
            sqlite3_free (errorMessage);
            return false;
        }
    }

    if (version < currentSchemaVersion)
        runMigrations (version);

    return executeRaw ("PRAGMA user_version = " + juce::String (currentSchemaVersion) + ";");
}

void Database::runMigrations (int fromVersion)
{
    // Future milestones add steps here, e.g.:
    //   if (fromVersion < 2) { ...create new tables/columns... }
    juce::ignoreUnused (fromVersion);
}

// =============================================================================
// CRUD
// =============================================================================
bool Database::insertMedia (otoha::MediaItem& item)
{
    const juce::ScopedLock sl (lock);
    if (db == nullptr) return false;

    static constexpr const char* sql =
        "INSERT INTO media (type, file_path, display_name, created_at, duration, sample_rate,"
        " channels, bit_depth, format, favorite, file_size, thumbnail_path, waveform_path)"
        " VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13);";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2 (db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    const auto bindText = [&stmt] (int index, const juce::String& text)
    {
        return sqlite3_bind_text (stmt, index, text.toRawUTF8(), -1, SQLITE_TRANSIENT);
    };

    bool ok = sqlite3_bind_text (stmt, 1, otoha::mediaTypeToString (item.type), -1, SQLITE_STATIC) == SQLITE_OK
           && bindText (2, item.file.getFullPathName()) == SQLITE_OK
           && bindText (3, item.displayName) == SQLITE_OK
           && sqlite3_bind_int64 (stmt, 4, item.createdAt.toMilliseconds()) == SQLITE_OK
           && sqlite3_bind_double (stmt, 5, item.durationSeconds) == SQLITE_OK
           && sqlite3_bind_int (stmt, 6, item.sampleRate) == SQLITE_OK
           && sqlite3_bind_int (stmt, 7, item.channels) == SQLITE_OK
           && sqlite3_bind_int (stmt, 8, item.bitDepth) == SQLITE_OK
           && bindText (9, item.format) == SQLITE_OK
           && sqlite3_bind_int (stmt, 10, item.favorite ? 1 : 0) == SQLITE_OK
           && sqlite3_bind_int64 (stmt, 11, item.fileSizeBytes) == SQLITE_OK
           && bindText (12, item.thumbnailPath) == SQLITE_OK
           && bindText (13, item.waveformCachePath) == SQLITE_OK;

    if (ok && sqlite3_step (stmt) == SQLITE_DONE)
        item.id = sqlite3_last_insert_rowid (db);
    else
        ok = false;

    sqlite3_finalize (stmt);
    return ok;
}

bool Database::updateDisplayName (juce::int64 id, const juce::String& newName)
{
    const juce::ScopedLock sl (lock);
    if (db == nullptr) return false;
    if (newName.trim().isEmpty()) return false;   // a recording always has a name

    static constexpr const char* sql = "UPDATE media SET display_name = ?1 WHERE id = ?2;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2 (db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_text (stmt, 1, newName.toRawUTF8(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64 (stmt, 2, id);
    const bool ok = sqlite3_step (stmt) == SQLITE_DONE;
    sqlite3_finalize (stmt);
    return ok;
}

bool Database::updateFavorite (juce::int64 id, bool favorite)
{
    const juce::ScopedLock sl (lock);
    if (db == nullptr) return false;

    static constexpr const char* sql = "UPDATE media SET favorite = ?1 WHERE id = ?2;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2 (db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_int (stmt, 1, favorite ? 1 : 0);
    sqlite3_bind_int64 (stmt, 2, id);
    const bool ok = sqlite3_step (stmt) == SQLITE_DONE;
    sqlite3_finalize (stmt);
    return ok;
}

bool Database::removeMedia (juce::int64 id)
{
    const juce::ScopedLock sl (lock);
    if (db == nullptr) return false;

    static constexpr const char* sql = "DELETE FROM media WHERE id = ?1;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2 (db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_int64 (stmt, 1, id);
    const bool ok = sqlite3_step (stmt) == SQLITE_DONE;
    sqlite3_finalize (stmt);
    return ok;
}

otoha::MediaItem Database::getMedia (juce::int64 id) const
{
    const juce::ScopedLock sl (lock);
    otoha::MediaItem item; // id 0 => "missing"

    if (db == nullptr) return item;

    const juce::String sql = juce::String ("SELECT ") + selectColumns + " FROM media WHERE id = ?1;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2 (db, sql.toRawUTF8(), -1, &stmt, nullptr) != SQLITE_OK)
        return item;

    sqlite3_bind_int64 (stmt, 1, id);
    if (sqlite3_step (stmt) == SQLITE_ROW)
        item = rowToItem (stmt);

    sqlite3_finalize (stmt);
    return item;
}

std::vector<otoha::MediaItem> Database::all() const
{
    return query ({}, otoha::LibraryFilter::all, otoha::LibrarySort::newestFirst);
}

std::vector<otoha::MediaItem> Database::query (const juce::String& searchText,
                                               otoha::LibraryFilter filter,
                                               otoha::LibrarySort sort) const
{
    const juce::ScopedLock sl (lock);
    std::vector<otoha::MediaItem> results;

    if (db == nullptr) return results;

    juce::String sql = juce::String ("SELECT ") + selectColumns + " FROM media WHERE 1";

    switch (filter)
    {
        case otoha::LibraryFilter::audio:     sql += " AND type = 'audio'";    break;
        case otoha::LibraryFilter::video:     sql += " AND type = 'video'";    break;
        case otoha::LibraryFilter::favorites: sql += " AND favorite = 1";      break;
        case otoha::LibraryFilter::all:                                        break;
    }

    if (searchText.isNotEmpty())
        sql += " AND display_name LIKE '%" + escapeLike (searchText) + "%' ESCAPE '\\'";

    switch (sort)
    {
        case otoha::LibrarySort::newestFirst:    sql += " ORDER BY created_at DESC"; break;
        case otoha::LibrarySort::oldestFirst:    sql += " ORDER BY created_at ASC";  break;
        case otoha::LibrarySort::nameAscending:  sql += " ORDER BY display_name COLLATE NOCASE ASC";  break;
        case otoha::LibrarySort::nameDescending: sql += " ORDER BY display_name COLLATE NOCASE DESC"; break;
        case otoha::LibrarySort::longestFirst:   sql += " ORDER BY duration DESC"; break;
        case otoha::LibrarySort::shortestFirst:  sql += " ORDER BY duration ASC";  break;
    }

    sql += ";";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2 (db, sql.toRawUTF8(), -1, &stmt, nullptr) != SQLITE_OK)
        return results;

    while (sqlite3_step (stmt) == SQLITE_ROW)
        results.push_back (rowToItem (stmt));

    sqlite3_finalize (stmt);
    return results;
}
