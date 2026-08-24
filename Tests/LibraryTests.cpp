/*
    LibraryTests — headless tests for the Library: database lifecycle, query
    semantics and filesystem reconciliation. Uses temporary directories only;
    no audio hardware required.
*/
#include "../Source/Library/LibraryService.h"

#include <cstdio>

namespace
{
bool expect (bool condition, const char* message)
{
    if (! condition)
        std::printf ("FAIL: %s\n", message);
    return condition;
}

/** Writes a tiny valid mono 16-bit WAV so metadata extraction has real work to do. */
juce::File writeTestWav (const juce::File& dir, const juce::String& name,
                         int seconds = 1, double rate = 48000.0)
{
    juce::WavAudioFormat wavFormat;

    const auto file = dir.getChildFile (name);
    auto stream = file.createOutputStream();
    if (stream == nullptr) return {};

    std::unique_ptr<juce::AudioFormatWriter> writer (
        wavFormat.createWriterFor (stream, rate, 1, 16, {}, 0));
    if (writer == nullptr) return {};

    juce::AudioBuffer<float> silence (1, (int) (seconds * rate));
    writer->writeFromAudioSampleBuffer (silence, 0, silence.getNumSamples());
    writer.reset();
    return file;
}

otoha::MediaItem makeItem (const juce::File& f, const juce::String& name,
                           double duration, bool favorite = false,
                           otoha::MediaType type = otoha::MediaType::audio)
{
    otoha::MediaItem item;
    item.type = type;
    item.file = f;
    item.displayName = name;
    item.createdAt = juce::Time::getCurrentTime();
    item.durationSeconds = duration;
    item.favorite = favorite;
    return item;
}
} // namespace

int main()
{
    bool ok = true;

    const auto root = juce::File::createTempFile ("otoha_lib_test_dir");
    root.createDirectory();
    const auto dbFile = root.getChildFile ("Database").getChildFile ("library.sqlite");

    {
        Database db;
        juce::String error;

        // --- creation + schema -------------------------------------------------
        ok &= expect (db.open (dbFile, error), "database should open");
        ok &= expect (error.isEmpty(), "no error on fresh open");
        ok &= expect (db.schemaVersion() == 1, "schema version must be initialised to 1");

        // --- insert / get ------------------------------------------------------
        auto a = writeTestWav (root.getChildFile ("Library").getChildFile ("Audio"), "a.wav");
        ok &= expect (a.existsAsFile(), "test wav written");

        otoha::MediaItem item = makeItem (a, "First Idea", 12.5);
        ok &= expect (db.insertMedia (item), "insert succeeds");
        ok &= expect (item.id != 0, "insert assigns an id");

        const auto fetched = db.getMedia (item.id);
        ok &= expect (fetched.id == item.id && fetched.displayName == "First Idea"
                          && fetched.type == otoha::MediaType::audio,
                      "inserted row round-trips");

        // --- rename ------------------------------------------------------------
        ok &= expect (db.updateDisplayName (item.id, "My New Song Idea"), "rename succeeds");
        ok &= expect (db.getMedia (item.id).displayName == "My New Song Idea", "rename persists");
        ok &= expect (! db.updateDisplayName (item.id, "   "), "blank rename is rejected");

        // --- favorites ---------------------------------------------------------
        ok &= expect (db.updateFavorite (item.id, true), "favorite set");
        ok &= expect (db.getMedia (item.id).favorite, "favorite persists");
        ok &= expect ((int) db.query ({}, otoha::LibraryFilter::favorites,
                                       otoha::LibrarySort::newestFirst).size() == 1,
                      "favorites filter finds it");

        // --- search + LIKE escaping -------------------------------------------
        otoha::MediaItem percentItem = makeItem (writeTestWav (root, "b.wav"), "100% Live Take");
        db.insertMedia (percentItem);

        ok &= expect (db.query ("live", otoha::LibraryFilter::all,
                                otoha::LibrarySort::newestFirst).size() == 1,
                      "case-insensitive substring search matches");
        ok &= expect (db.query ("100%", otoha::LibraryFilter::all,
                                otoha::LibrarySort::newestFirst).size() == 1,
                      "'%' in search text matches literally");
        ok &= expect (db.query ("zzz_nothing", otoha::LibraryFilter::all,
                                otoha::LibrarySort::newestFirst).empty(),
                      "non-matching search returns nothing");

        // --- special characters in names --------------------------------------
        otoha::MediaItem weird = makeItem (writeTestWav (root, "c.wav"),
                                           "Ünïcode \"quoted\" & <tagged> 🎙");
        ok &= expect (db.insertMedia (weird), "special-char insert succeeds");
        ok &= expect (db.getMedia (weird.id).displayName == weird.displayName,
                      "special characters round-trip");

        // --- sorting ------------------------------------------------------------
        otoha::MediaItem old = makeItem (writeTestWav (root, "d.wav"), "Aaa Old");
        old.createdAt = juce::Time::getCurrentTime() - juce::RelativeTime::days (10);
        old.durationSeconds = 3.0;
        db.insertMedia (old);

        const auto newest = db.query ({}, otoha::LibraryFilter::all, otoha::LibrarySort::newestFirst);
        ok &= expect (newest.size() >= 4 && newest[0].displayName == weird.displayName,
                      "newest first by default");
        ok &= expect (db.query ({}, otoha::LibraryFilter::all, otoha::LibrarySort::oldestFirst)[0].id
                          == old.id, "oldest first works");
        ok &= expect (db.query ({}, otoha::LibraryFilter::all, otoha::LibrarySort::longestFirst)[0].id
                          == item.id, "longest first works");
        ok &= expect (db.query ({}, otoha::LibraryFilter::all, otoha::LibrarySort::shortestFirst)[0].id
                          == old.id, "shortest first works");

        const auto nameAsc = db.query ({}, otoha::LibraryFilter::all, otoha::LibrarySort::nameAscending);
        ok &= expect (nameAsc[0].displayName <= nameAsc[nameAsc.size() - 1].displayName,
                      "name ascending is ordered");

        // --- filters ------------------------------------------------------------
        otoha::MediaItem video = makeItem (root.getChildFile ("Library").getChildFile ("Video")
                                               .getChildFile ("clip.mp4"),
                                           "Clip", 30.0, true, otoha::MediaType::video);
        video.file.create();  // empty stand-in file; the DB never reads it back as audio
        db.insertMedia (video);

        ok &= expect (db.query ({}, otoha::LibraryFilter::video, otoha::LibrarySort::newestFirst).size() == 1,
                      "video filter isolates videos");
        ok &= expect (db.query ({}, otoha::LibraryFilter::audio, otoha::LibrarySort::newestFirst).size()
                              == (int) db.all().size() - 1,
                      "audio filter excludes video");

        // --- delete -------------------------------------------------------------
        const int before = (int) db.all().size();
        ok &= expect (db.removeMedia (percentItem.id), "delete removes the row");
        ok &= expect ((int) db.all().size() == before - 1, "row count drops by one");
        ok &= expect (db.getMedia (percentItem.id).id == 0, "deleted row no longer resolves");

        // --- missing-file recovery through the service --------------------------
        LibraryService service (root);
        ok &= expect (service.initialise (error), "service initialises against existing database");

        const auto stale = makeItem (root.getChildFile ("gone.wav"), "Vanished", 1.0);
        db.insertMedia (stale);

        // An unregistered but valid wav sitting in Library/Audio gets recovered.
        const auto orphan = writeTestWav (root.getChildFile ("Library").getChildFile ("Audio"),
                                          "orphan.wav", 2);
        juce::ignoreUnused (orphan);

        const auto scan = service.performStartupScan();
        ok &= expect (scan.staleRemoved >= 1, "missing-file rows are cleaned up");
        ok &= expect (scan.recovered >= 1, "unregistered files are recovered");
        ok &= expect (db.getMedia (stale.id).id == 0, "vanished file's entry removed");
        ok &= expect (! db.query ("orphan", otoha::LibraryFilter::all,
                                  otoha::LibrarySort::newestFirst).empty(),
                      "recovered recording appears in queries");

        // --- migration hook -----------------------------------------------------
        Database migrated;
        ok &= expect (migrated.open (dbFile, error), "reopen for migration check");
        ok &= expect (migrated.executeRaw ("PRAGMA user_version = 0;"), "reset version for test");
        migrated.close();

        Database reopened;
        juce::String migrationError;
        ok &= expect (reopened.open (dbFile, migrationError), "open at v0 re-runs ensureSchema");
        ok &= expect (reopened.schemaVersion() == 1, "version restored to 1");
        ok &= expect (! reopened.all().empty(), "data survives the migration pass");
    }

    root.deleteRecursively();

    if (! ok) return 1;
    std::printf ("PASS: library database, service and scan\n");
    return 0;
}
