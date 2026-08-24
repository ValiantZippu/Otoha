#pragma once

#include <juce_core/juce_core.h>

namespace otoha
{
class AudioDocument;
}

/*
    ProjectFormat — the portable ".otoha" project container (M12 #6–#8).

    A project is a DIRECTORY (never a giant JSON blob of audio):

        My Recording.otoha/
            project.json     metadata + timeline + DSP config (small, text)
            audio/           optional local copies when the user chose "import copy"
            waveform/        regenerable peak cache (safe to delete, #43)

    Raw audio is NEVER embedded in project.json (#6): sources are referenced.
    The timeline/DSP payload reuses AudioDocument's serialization; this module
    adds the versioned envelope and migration seam so future format changes
    are non-destructive.

    Cross-platform by construction: paths inside project.json are stored as
    they were written; missing sources surface as ErrorCategory::fileUnavailable,
    never as a crash (#59/#66).
*/

namespace otoha::project
{
constexpr int currentFormatVersion = 1;

/** Wraps an AudioDocument's state in the versioned envelope. */
juce::var buildProjectJSON (const AudioDocument& document,
                            const juce::String& title);

/** Reads project.json text, migrates if needed, applies to `document`.
    `document` must already have its source audio loaded (loadFromFile or the
    test constructor). Returns false with a user-facing message on failure. */
bool applyProjectJSON (AudioDocument& document,
                       const juce::var& parsed,
                       juce::String& errorOut);

// --- directory-level operations ---------------------------------------------

/** The conventional project directory for a source file: "<Name>.otoha". */
juce::File projectDirectoryFor (const juce::File& sourceFile);

/** Writes <dir>/project.json atomically (temp + move). Creates the directory
    layout on first save. */
bool saveProject (const AudioDocument& document,
                  const juce::File& projectDir,
                  const juce::String& title,
                  juce::String& errorOut);

/** Loads <dir>/project.json into `document` (source audio must be loadable —
    see class comment). */
bool loadProject (const juce::File& projectDir,
                  AudioDocument& document,
                  juce::String& errorOut);

/** Migration hook (#8). Currently v1 is the only version, so this is identity;
    future versions add steps here and bump currentFormatVersion. */
bool migrateToCurrent (const juce::var& parsed, int fromVersion, juce::var& migrated);
} // namespace otoha::project
