#pragma once

#include <juce_core/juce_core.h>

/*
    AppPaths — logical storage locations (M12 #9/#11).

    Shared code asks for a PURPOSE ("where do recordings go?"), never an OS
    path. Mapping uses JUCE's own cross-platform special locations, which are
    already correct on Windows / macOS / Linux / Android / iOS:

        appData      Windows:%APPDATA%\Otoha   macOS:~/Library/Application Support/Otoha
                     Linux:~/.local/share/Otoha   Android/iOS:app-private files dir
        documents    the user's visible Documents folder (desktop only)
        recordings   inside appData by default (private, survives upgrades)
        cache        OS temp/cache location — always safe to delete
        exports      documents\Otoha Exports on desktop; appData\exports on mobile

    Nothing here hardcodes C:\, /Users/, or Android absolute paths.
*/

namespace otoha::paths
{
/** The single root all Otoha user data lives under, per platform. */
inline juce::File appDataRoot()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Otoha");
    // On some Linux setups userApplicationDataDirectory resolves oddly;
    // JUCE guarantees a writable fallback through userHomeDirectory check below.
    if (dir.getFullPathName().trim().isEmpty())
        dir = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".otoha");
    return dir;
}

inline juce::File recordings()     { return appDataRoot().getChildFile ("Recordings"); }
inline juce::File cache()          { return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Otoha"); }
inline juce::File waveformCache()  { return appDataRoot().getChildFile ("WaveformCache"); }
inline juce::File profiles()       { return appDataRoot().getChildFile ("profiles.json"); }
inline juce::File settings()       { return appDataRoot().getChildFile ("settings.json"); }

/** Where finished exports land. Desktop users expect to find files in
    Documents; mobile sandboxes make Documents either virtual or restricted,
    so exports stay app-private there and reach the user via Share (#26). */
inline juce::File exports (bool mobileSurface)
{
    if (mobileSurface)
        return appDataRoot().getChildFile ("Exports");

    auto docs = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Otoha Exports");
    return docs;
}

/** Best-effort creation of every location the app needs at startup. */
inline void ensureStandardDirectories()
{
    appDataRoot().createDirectory();
    recordings().createDirectory();
    waveformCache().createDirectory();
    exports (false).createDirectory();
}
} // namespace otoha::paths
