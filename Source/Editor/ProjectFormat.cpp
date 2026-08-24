#include "ProjectFormat.h"

#include "AudioDocument.h"
#include "../Core/OtohaError.h"
#include "../Core/OtohaLog.h"

namespace otoha::project
{
juce::var buildProjectJSON (const AudioDocument& document, const juce::String& title)
{
    auto* root = new juce::DynamicObject();

    root->setProperty ("formatVersion", currentFormatVersion);
    root->setProperty ("application",   "Otoha");
    root->setProperty ("title",         title);
    root->setProperty ("createdAt",     juce::Time::getCurrentTime().toISO8601 (true));
    // The timeline + DSP payload — identical structure to the autosave sidecar,
    // so a sidecar and a project stay interchangeable at the payload level.
    root->setProperty ("document",      document.toJSON());

    return juce::var (root);
}

bool applyProjectJSON (AudioDocument& document, const juce::var& parsed, juce::String& errorOut)
{
    if (parsed.isVoid() || ! parsed.hasProperty ("formatVersion"))
    {
        errorOut = "This project file is damaged or incomplete.";
        return false;
    }

    const int version = (int) (double) parsed.getProperty ("formatVersion", 0.0);
    if (version <= 0)
    {
        errorOut = "This project file is damaged or incomplete.";
        return false;
    }
    if (version > currentFormatVersion)
    {
        // Written by a NEWER Otoha: refuse gracefully rather than corrupt (#59).
        errorOut = "This project was made with a newer version of Otoha. Please update Otoha.";
        return false;
    }

    juce::var migrated;
    if (! migrateToCurrent (parsed, version, migrated))
    {
        errorOut = "Couldn't open this project's format.";
        return false;
    }

    // migrateToCurrent leaves `migrated` empty when no migration was needed.
    const auto payload = (migrated.isVoid() ? parsed : migrated).getProperty ("document", {});
    if (! document.fromJSON (payload))
    {
        errorOut = "This project doesn't match its source audio.";
        return false;
    }

    return true;
}

// --- directory-level operations ---------------------------------------------

juce::File projectDirectoryFor (const juce::File& sourceFile)
{
    if (sourceFile == juce::File{})
        return {};
    return sourceFile.withFileExtension ("otoha");
}

bool saveProject (const AudioDocument& document,
                  const juce::File& projectDir,
                  const juce::String& title,
                  juce::String& errorOut)
{
    if (projectDir == juce::File{})
    {
        errorOut = userMessage (ErrorCategory::fileUnavailable, ErrorSurface::desktop);
        return false;
    }

    if (! projectDir.createDirectory())
    {
        errorOut = userMessage (ErrorCategory::storageUnavailable, ErrorSurface::desktop);
        return false;
    }

    // Conventional layout (#7): audio/ + waveform/ exist even while empty so
    // the container is self-describing; caches are regenerable (#43).
    projectDir.getChildFile ("audio").createDirectory();
    projectDir.getChildFile ("waveform").createDirectory();

    const auto jsonText = juce::JSON::toString (
        buildProjectJSON (document, title), true);

    const auto target = projectDir.getChildFile ("project.json");
    const auto temp   = projectDir.getChildFile ("project.json.tmp");

    if (! temp.replaceWithText (jsonText))
    {
        errorOut = userMessage (ErrorCategory::storageUnavailable, ErrorSurface::desktop);
        return false;
    }
    if (! temp.moveFileTo (target))
    {
        temp.deleteFile();
        if (! target.replaceWithText (jsonText))   // same fallback policy as the sidecar
        {
            errorOut = userMessage (ErrorCategory::storageUnavailable, ErrorSurface::desktop);
            return false;
        }
    }

    otoha::log::info ("project saved: " + target.getFileName());
    return true;
}

bool loadProject (const juce::File& projectDir,
                  AudioDocument& document,
                  juce::String& errorOut)
{
    const auto jsonFile = projectDir.getChildFile ("project.json");
    if (! jsonFile.existsAsFile())
    {
        errorOut = "This isn't an Otoha project folder.";
        return false;
    }

    const auto parsed = juce::JSON::parse (jsonFile);
    if (! applyProjectJSON (document, parsed, errorOut))
        return false;

    otoha::log::info ("project loaded: " + jsonFile.getFileName());
    return true;
}

bool migrateToCurrent (const juce::var& /*parsed*/, int fromVersion, juce::var& migrated)
{
    // v1 -> v1: identity. Future migrations chain here:
    //   if (fromVersion < 2) { ...upgrade fields...; fromVersion = 2; }
    if (fromVersion != currentFormatVersion)
        return false;

    migrated = juce::var();   // signal "unchanged"; callers use the original
    return true;
}
} // namespace otoha::project
