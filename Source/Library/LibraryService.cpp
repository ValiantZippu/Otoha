#include "LibraryService.h"

#include "../Core/RecordingSupport.h"

#include <set>

LibraryService::LibraryService (const juce::File& baseDirectory)
    : baseDir (baseDirectory),
      waveformCache (baseDirectory.getChildFile ("Cache").getChildFile ("Waveforms"))
{
    formatManager.registerBasicFormats();
}

LibraryService::~LibraryService() = default;

juce::File otohaBaseDirectory()
{
    auto base = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Otoha");
    if (base.getFullPathName().isEmpty())
        base = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Otoha");
    return base;
}

bool LibraryService::initialise (juce::String& errorOut)
{
    baseDir.getChildFile ("Library").getChildFile ("Audio").createDirectory();
    baseDir.getChildFile ("Library").getChildFile ("Video").createDirectory();
    baseDir.getChildFile ("Cache").getChildFile ("Waveforms").createDirectory();
    baseDir.getChildFile ("Cache").getChildFile ("Thumbnails").createDirectory();

    return db.open (baseDir.getChildFile ("Database").getChildFile ("library.sqlite"), errorOut);
}

// =============================================================================
// Startup scan — cheap: directory listing + row checks, no audio decoding of
// files that are already registered.
// =============================================================================
LibraryService::ScanResult LibraryService::performStartupScan()
{
    ScanResult result;

    // 1. Drop rows whose files vanished (graceful recovery; media is never here).
    for (const auto& item : db.all())
    {
        if (! item.file.existsAsFile())
        {
            if (db.removeMedia (item.id))
            {
                ++result.staleRemoved;
                // Clean any orphaned waveform cache entry.
                if (item.waveformCachePath.isNotEmpty())
                    juce::File (item.waveformCachePath).deleteFile();
                else
                    baseDir.getChildFile ("Cache").getChildFile ("Waveforms")
                        .getChildFile ("wave-" + juce::String (item.id) + ".owf").deleteFile();
            }
        }
    }

    // 2. Recover files on disk that the database does not know about.
    //    Scanned locations: Library/Audio (canonical) + the legacy root folder
    //    used before Milestone 3, so older recordings are adopted automatically.
    std::set<juce::String> knownPaths;
    for (const auto& item : db.all())
        knownPaths.insert (item.file.getFullPathName());

    juce::Array<juce::File> candidates;
    for (const auto& dir : { baseDir.getChildFile ("Library").getChildFile ("Audio"),
                             baseDir })
    {
        for (const auto& ext : juce::StringArray { "*.wav", "*.aif", "*.aiff", "*.flac" })
            for (const auto& f : dir.findChildFiles (juce::File::findFiles, false, ext))
                if (! knownPaths.contains (f.getFullPathName()))
                    candidates.add (f);
    }

    for (const auto& f : candidates)
    {
        if (knownPaths.contains (f.getFullPathName()))
            continue;   // may appear in both scanned dirs

        if (registerAudioFile (f) != 0)
            ++result.recovered;
        else
            ++result.unreadable;   // broken file: skipped, left on disk, never fatal
    }

    return result;
}

// =============================================================================
// Registration
// =============================================================================
juce::int64 LibraryService::registerAudioFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return 0;

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr || reader->sampleRate <= 0)
        return 0;

    otoha::MediaItem item;
    item.type            = otoha::MediaType::audio;
    item.file            = file;
    item.displayName     = file.getFileNameWithoutExtension();
    item.createdAt       = file.getLastModificationTime();
    item.durationSeconds = reader->sampleRate > 0
        ? (double) reader->lengthInSamples / reader->sampleRate : 0.0;
    item.sampleRate      = (int) reader->sampleRate;
    item.channels        = (int) reader->numChannels;
    item.bitDepth        = (int) reader->bitsPerSample;
    item.format          = file.getFileExtension().replace (".", "").toUpperCase();
    item.fileSizeBytes   = file.getSize();

    return db.insertMedia (item) ? item.id : 0;
}

juce::int64 LibraryService::duplicateMedia (juce::int64 id)
{
    const auto source = db.getMedia (id);
    if (source.id == 0 || ! source.file.existsAsFile())
        return 0;

    // "Name copy.ext" next to the original; keep suffixing if even that exists.
    const auto ext = source.file.getFileExtension();
    auto target = source.file.getSiblingFile (source.file.getFileNameWithoutExtension()
                                              + " copy" + ext);
    int n = 2;
    while (target.existsAsFile())
        target = source.file.getSiblingFile (source.file.getFileNameWithoutExtension()
                                             + " copy " + juce::String (n++) + ext);

    if (! source.file.copyFileTo (target))
        return 0;

    const auto newId = registerAudioFile (target);
    if (newId != 0)
        db.updateDisplayName (newId, source.displayName + " copy");   // #18: name is metadata only
    return newId;
}

bool LibraryService::rename (juce::int64 id, const juce::String& newDisplayName)
{
    const auto trimmed = newDisplayName.trim();
    if (trimmed.isEmpty())
        return false;
    return db.updateDisplayName (id, trimmed);
}

bool LibraryService::setFavorite (juce::int64 id, bool favorite)
{
    return db.updateFavorite (id, favorite);
}

bool LibraryService::deleteMedia (juce::int64 id)
{
    const auto item = db.getMedia (id);
    if (item.id == 0)
        return false;

    if (! db.removeMedia (id))
        return false;

    bool filesClean = true;

    if (item.file.existsAsFile())
    {
        // Prefer the platform trash; fall back to deletion.
        if (! item.file.moveToTrash())
            filesClean = item.file.deleteFile();
    }

    // Waveform cache cleanup: trust the stored path, fall back to the convention.
    if (item.waveformCachePath.isNotEmpty())
        juce::File (item.waveformCachePath).deleteFile();
    else
        baseDir.getChildFile ("Cache").getChildFile ("Waveforms")
            .getChildFile ("wave-" + juce::String (id) + ".owf").deleteFile();

    return filesClean;
}

std::vector<otoha::MediaItem> LibraryService::query (const juce::String& searchText,
                                                     otoha::LibraryFilter filter,
                                                     otoha::LibrarySort sort) const
{
    return db.query (searchText, filter, sort);
}
