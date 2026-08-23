#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include <memory>
#include <vector>

/*
    AudioDocument — the whole M4 editing engine, deliberately small.

    Model: ONE continuous source buffer (decoded once when the recording is
    opened) plus a list of Clips that reference ranges of it:

        timeline  =  clip[0] ++ clip[1] ++ ... ++ clip[n]

    Every edit just rewires the clip list, so edits are non-destructive and
    undo/redo snapshots cost a few dozen bytes — never a new WAV file.

    All positions are SAMPLES in timeline space; clips store (sourceStart,
    length). Mono and stereo are preserved as-is. The original file is never
    touched by editing; rendering happens only on Save/Export.
*/
namespace otoha
{
struct Clip
{
    juce::int64 sourceStart = 0;   // offset into the source buffer
    juce::int64 length = 0;        // frames

    juce::int64 end() const  { return sourceStart + length; }
};

/** Internal audio clipboard: real samples + their properties (never the OS text clipboard). */
struct AudioClipboard
{
    juce::AudioBuffer<float> data;
    double sampleRate = 0.0;
    int numChannels = 0;

    bool isEmpty() const  { return data.getNumSamples() == 0; }
    void clear()          { data.setSize (1, 0); sampleRate = 0.0; numChannels = 0; }
};

class AudioDocument
{
public:
    AudioDocument() = default;

    /** Decodes a recording fully into memory (voice-note sized; see limitations).
        Returns false with a user-facing message on unreadable/corrupt files. */
    bool loadFromFile (const juce::File& sourceFile, juce::String& errorOut);

    /** Test/programmatic construction without a file. */
    AudioDocument (juce::AudioBuffer<float> sourceData, double sampleRate);

    // --- properties ----------------------------------------------------------
    juce::int64 totalSamples() const;
    double getSampleRate() const         { return sampleRate; }
    int getNumChannels() const           { return source.getNumChannels(); }
    const juce::File& getSourceFile() const { return sourceFile; }
    juce::uint32 getVersion() const      { return version; }   // bumped on every mutation

    // --- timeline access -------------------------------------------------------
    const std::vector<Clip>& getClips() const            { return clips; }
    /** Copies `numFrames` frames starting at timeline `start` into dest channel pointers. */
    void readRange (juce::int64 start, int numFrames, float* const* dest, int destChannels) const;
    juce::AudioBuffer<float> readRangeToBuffer (juce::int64 start, int numFrames) const;

    // --- selection -------------------------------------------------------------
    struct Selection
    {
        juce::int64 start = 0;
        juce::int64 end = 0;

        bool isEmpty() const       { return end <= start; }
        juce::int64 length() const { return juce::jmax ((juce::int64) 0, end - start); }
        void normalize()           { if (end < start) std::swap (start, end); clamp(); }
        void clampTo (juce::int64 total)
        {
            start = juce::jlimit ((juce::int64) 0, total, start);
            end   = juce::jlimit ((juce::int64) 0, total, end);
        }
    };

    Selection getSelection() const;
    void setSelection (juce::int64 start, juce::int64 end);
    void clearSelection()                    { selection = {}; }

    // --- edits (all undoable; all sample-accurate) ------------------------------
    void rippleDelete (juce::int64 start, juce::int64 length);
    void trimToSelection();
    void copySelectedRange (AudioClipboard& out) const;
    void cutSelectedRange (AudioClipboard& out);
    /** Inserts clipboard audio at `position` (insertion, never overwrite).
        Cross-rate paste is resampled; channel counts are adapted (mono<->stereo). */
    bool pasteAt (juce::int64 position, const AudioClipboard& clipboard, juce::String& errorOut);

    void undo();
    void redo();
    bool canUndo() const  { return ! undoStack.empty(); }
    bool canRedo() const  { return ! redoStack.empty(); }

    // --- persistence (lightweight edit-state sidecar, not a project format) -----
    juce::var toJSON() const;
    bool fromJSON (const juce::var& state);

    static juce::File sidecarPathFor (const juce::File& sourceFile);
    bool autosaveState() const;                 // writes the .otoha-edit.json sidecar
    bool hasRestorableState() const;            // sidecar exists?
    bool restoreFromSidecar();                  // returns false if unparsable (caller ignores)
    void clearSavedState() const;               // on Save/Discard

    bool isModified() const                     { return modified; }
    void markUnmodified()                       { modified = false; }

    /** Test hook: associate a sidecar location without a real file load. */
    juce::File& sourceFileForTest()             { return sourceFile; }

private:
    void pushUndoSnapshot();
    void applyClips (std::vector<Clip> newClips);
    juce::int64 timelineToSource (juce::int64 timelinePos, juce::int64* offsetInClip = nullptr) const;

    juce::File sourceFile;
    juce::AudioBuffer<float> source;     // decoded original, immutable after load
    double sampleRate = 0.0;
    std::vector<Clip> clips;
    Selection selection;

    std::vector<std::vector<Clip>> undoStack;
    std::vector<std::vector<Clip>> redoStack;
    bool modified = false;
    juce::uint32 version = 0;
};

/** Resamples + adapts channels so a clipboard can be pasted into a document
    whose rate/channel count differ. Off-thread by construction (UI calls this
    before handing the result to pasteAt). */
juce::AudioBuffer<float> prepareClipboardForDocument (const AudioClipboard& clipboard,
                                                      double destSampleRate, int destChannels,
                                                      bool* wasConverted = nullptr);
} // namespace otoha
