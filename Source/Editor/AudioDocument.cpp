#include "AudioDocument.h"

#include "../Dsp/SampleRateConverter.h"

#include <algorithm>
#include <cmath>

namespace otoha
{
namespace
{
constexpr int decodeChunkFrames = 1 << 16;

/** Converts a whole file to float samples via the int reader path (always available). */
juce::AudioBuffer<float> readWholeFile (juce::AudioFormatReader& reader, juce::String& errorOut)
{
    const int channels = (int) reader.numChannels;
    const auto total = reader.lengthInSamples;

    juce::AudioBuffer<float> result (channels, (int) total);
    juce::AudioBuffer<int> chunk (channels, decodeChunkFrames);

    for (juce::int64 pos = 0; pos < total; pos += decodeChunkFrames)
    {
        const int frames = (int) juce::jmin ((juce::int64) decodeChunkFrames, total - pos);
        if (! reader.read (&chunk, 0, frames, pos, true, true))
        {
            errorOut = "Couldn't open this recording.\nThe file could not be read correctly.";
            return {};
        }

        for (int ch = 0; ch < channels; ++ch)
        {
            const auto* src = chunk.getReadPointer (ch);
            auto* dst = result.getWritePointer (ch, (int) pos);
            for (int i = 0; i < frames; ++i)
                dst[i] = (float) ((double) src[i] / 2147483648.0);
        }
    }
    return result;
}

// Resampler/channel adapter live in Dsp/SampleRateConverter (shared with export).
} // namespace

// =============================================================================
// Construction / loading
// =============================================================================
bool AudioDocument::loadFromFile (const juce::File& file, juce::String& errorOut)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->sampleRate <= 0 || reader->lengthInSamples <= 0)
    {
        errorOut = "Couldn't open this recording.\nThe file could not be read correctly.";
        return false;
    }

    auto data = readWholeFile (*reader, errorOut);
    if (data.getNumSamples() == 0)
        return false;

    *this = AudioDocument (std::move (data), reader->sampleRate);
    sourceFile = file;
    return true;
}

AudioDocument::AudioDocument (juce::AudioBuffer<float> data, double rate)
    : source (std::move (data)), sampleRate (rate)
{
    if (! source.isEmpty())
        clips.push_back ({ 0, source.getNumSamples() });
}

// =============================================================================
// Timeline access
// =============================================================================
juce::int64 AudioDocument::totalSamples() const
{
    juce::int64 total = 0;
    for (const auto& c : clips)
        total += c.length;
    return total;
}

juce::int64 AudioDocument::timelineToSource (juce::int64 timelinePos, juce::int64* offsetInClip) const
{
    juce::int64 consumed = 0;
    for (const auto& c : clips)
    {
        if (timelinePos < consumed + c.length)
        {
            if (offsetInClip != nullptr)
                *offsetInClip = timelinePos - consumed;
            return c.sourceStart + (timelinePos - consumed);
        }
        consumed += c.length;
    }

    if (offsetInClip != nullptr)
        *offsetInClip = 0;
    return clips.empty() ? 0 : clips.back().end();
}

void AudioDocument::readRange (juce::int64 start, int numFrames,
                               float* const* dest, int destChannels) const
{
    if (numFrames <= 0 || clips.empty())
        return;

    start = juce::jlimit ((juce::int64) 0, juce::jmax ((juce::int64) 0, totalSamples() - 1), start);

    const int channels = juce::jmin (destChannels, source.getNumChannels());
    juce::int64 remaining = numFrames;
    juce::int64 pos = start;

    while (remaining > 0 && ! clips.empty())
    {
        // Locate clip containing pos.
        const Clip* current = nullptr;
        juce::int64 consumed = 0;
        for (const auto& c : clips)
        {
            if (pos < consumed + c.length) { current = &c; break; }
            consumed += c.length;
        }
        if (current == nullptr)   // past the end — silence
            break;

        const juce::int64 intoClip = pos - consumed;
        const juce::int64 availableInClip = current->length - intoClip;
        const int chunk = (int) juce::jmin (remaining, availableInClip);

        for (int ch = 0; ch < channels; ++ch)
            juce::FloatVectorOperations::copy (dest[ch],
                                               source.getReadPointer (ch, (int) (current->sourceStart + intoClip)),
                                               chunk);

        pos += chunk;
        remaining -= chunk;

        if (remaining > 0 && pos >= totalSamples())   // ran off the end — silence
        {
            for (int ch = 0; ch < channels; ++ch)
                juce::FloatVectorOperations::clear (dest[ch], (int) remaining);
            remaining = 0;
        }
    }
}

juce::AudioBuffer<float> AudioDocument::readRangeToBuffer (juce::int64 start, int numFrames) const
{
    juce::AudioBuffer<float> buffer (juce::jmax (1, getNumChannels()), juce::jmax (0, numFrames));
    float* ptrs[2] = { numFrames > 0 ? buffer.getWritePointer (0) : nullptr,
                       buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr };
    readRange (start, numFrames, ptrs, buffer.getNumChannels());
    return buffer;
}

// =============================================================================
// Selection
// =============================================================================
AudioDocument::Selection AudioDocument::getSelection() const { return selection; }

void AudioDocument::setSelection (juce::int64 start, juce::int64 end)
{
    selection.start = start;
    selection.end = end;
    selection.normalize();
    selection.clampTo (totalSamples());
}

// =============================================================================
// Edits
// =============================================================================
void AudioDocument::pushUndoSnapshot()
{
    undoStack.push_back (clips);
    redoStack.clear();          // a new edit invalidates the redo branch
    modified = true;
    ++version;
}

void AudioDocument::applyClips (std::vector<Clip> newClips)
{
    newClips.erase (std::remove_if (newClips.begin(), newClips.end(),
                                    [] (const Clip& c) { return c.length <= 0; }),
                    newClips.end());

    if (newClips.empty())       // never leave a document with no audio at all
    {
        undoStack.pop_back();   // roll back the snapshot we just pushed
        return;
    }

    clips = std::move (newClips);
    selection.clearSelection();
}

void AudioDocument::rippleDelete (juce::int64 start, juce::int64 length)
{
    if (length <= 0 || clips.empty())
        return;

    start = juce::jlimit ((juce::int64) 0, totalSamples(), start);
    length = juce::jmin (length, totalSamples() - start);
    if (length <= 0)
        return;

    pushUndoSnapshot();

    std::vector<Clip> result;
    result.reserve (clips.size() + 1);
    juce::int64 consumed = 0;
    const juce::int64 delEnd = start + length;

    for (const auto& c : clips)
    {
        const juce::int64 clipStart = consumed;
        const juce::int64 clipEnd = consumed + c.length;
        consumed = clipEnd;

        if (delEnd <= clipStart || start >= clipEnd)         // untouched
            result.push_back (c);
        else if (start <= clipStart && delEnd >= clipEnd)    // fully removed
            continue;
        else if (start > clipStart && delEnd < clipEnd)      // split
        {
            result.push_back ({ c.sourceStart, start - clipStart });
            result.push_back ({ c.sourceStart + (delEnd - clipStart), clipEnd - delEnd });
        }
        else if (start <= clipStart)                          // trim from front
            result.push_back ({ c.sourceStart + (delEnd - clipStart), clipEnd - delEnd });
        else                                                  // trim from back
            result.push_back ({ c.sourceStart, start - clipStart });
    }

    applyClips (std::move (result));
}

void AudioDocument::trimToSelection()
{
    if (selection.isEmpty() || clips.empty())
        return;

    // Pure clip operation: keep only what intersects the selection. The source
    // buffer stays intact so undo can always restore the previous timeline.
    pushUndoSnapshot();

    const juce::int64 keepStart = selection.start;
    const juce::int64 keepEnd = selection.end;

    std::vector<Clip> result;
    juce::int64 consumed = 0;

    for (const auto& c : clips)
    {
        const juce::int64 clipStart = consumed;
        const juce::int64 clipEnd = consumed + c.length;
        consumed = clipEnd;

        const juce::int64 from = juce::jmax (clipStart, keepStart);
        const juce::int64 to   = juce::jmin (clipEnd, keepEnd);
        if (to > from)
            result.push_back ({ c.sourceStart + (from - clipStart), to - from });
    }

    applyClips (std::move (result));
}

void AudioDocument::copySelectedRange (AudioClipboard& out) const
{
    if (selection.isEmpty())
        return;

    out.clear();
    out.data = readRangeToBuffer ((int) selection.start, (int) selection.length());
    out.sampleRate = sampleRate;
    out.numChannels = getNumChannels();
}

void AudioDocument::cutSelectedRange (AudioClipboard& out)
{
    copySelectedRange (out);
    rippleDelete (selection.start, selection.length());
}

bool AudioDocument::pasteAt (juce::int64 position, const AudioClipboard& clipboard, juce::String& errorOut)
{
    if (clipboard.isEmpty())
    {
        errorOut = "Nothing to paste yet. Copy or cut something first.";
        return false;
    }

    bool converted = false;
    auto prepared = prepareClipboardForDocument (clipboard, sampleRate, getNumChannels(), &converted);
    if (prepared.getNumSamples() == 0)
    {
        errorOut = "Couldn't apply this edit.\nThe clipboard audio could not be converted.";
        return false;
    }
    juce::ignoreUnused (converted);

    // Pasted material becomes part of the source buffer (appended), referenced by
    // an inserted clip — still non-destructive to the ORIGINAL region layout and
    // cheap to undo (snapshots stay tiny).
    pushUndoSnapshot();

    const int insertAt = (int) source.getNumSamples();
    source.setSize (getNumChannels(), insertAt + prepared.getNumSamples(), true, false, false);

    for (int ch = 0; ch < getNumChannels(); ++ch)
        juce::FloatVectorOperations::copy (source.getWritePointer (ch, insertAt),
                                           prepared.getReadPointer (juce::jmin (ch, prepared.getNumChannels() - 1)),
                                           prepared.getNumSamples());

    position = juce::jlimit ((juce::int64) 0, totalSamples(), position);

    std::vector<Clip> result;
    result.reserve (clips.size() + 1);
    juce::int64 consumed = 0;

    for (const auto& c : clips)
    {
        if (position == consumed)                    // insert before this clip
        {
            result.push_back ({ (juce::int64) insertAt, prepared.getNumSamples() });
            position = -1;                            // marker: already inserted
        }
        else if (position > consumed && position < consumed + c.length)
        {
            const juce::int64 firstLen = position - consumed;
            result.push_back ({ c.sourceStart, firstLen });
            result.push_back ({ (juce::int64) insertAt, prepared.getNumSamples() });
            result.push_back ({ c.sourceStart + firstLen, c.length - firstLen });
            position = -1;
            consumed += c.length;
            continue;
        }

        result.push_back (c);
        consumed += c.length;
    }

    if (position != -1)                              // insert at the very end
        result.push_back ({ (juce::int64) insertAt, prepared.getNumSamples() });

    clips = std::move (result);
    return true;
}

// =============================================================================
// Undo / redo
// =============================================================================
void AudioDocument::undo()
{
    if (undoStack.empty())
        return;

    redoStack.push_back (clips);
    clips = std::move (undoStack.back());
    undoStack.pop_back();
    selection.clearSelection();
    ++version;
    modified = true;
}

void AudioDocument::redo()
{
    if (redoStack.empty())
        return;

    undoStack.push_back (clips);
    clips = std::move (redoStack.back());
    redoStack.pop_back();
    selection.clearSelection();
    ++version;
    modified = true;
}

// =============================================================================
// Sidecar persistence (lightweight JSON of the clip list only)
// =============================================================================
juce::var AudioDocument::toJSON() const
{
    juce::Array<juce::var> clipArray;
    for (const auto& c : clips)
        clipArray.add (juce::var (new juce::DynamicObject())); // replaced below

    // Build objects explicitly (clearer than the one-liner above).
    clipArray.clear();
    for (const auto& c : clips)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("s", (double) c.sourceStart);
        obj->setProperty ("l", (double) c.length);
        clipArray.add (juce::var (obj));
    }

    auto* root = new juce::DynamicObject();
    root->setProperty ("version", 1);
    root->setProperty ("source", sourceFile.getFullPathName());
    root->setProperty ("clips", clipArray);
    root->setProperty ("dsp", processing.toJSON());
    return juce::var (root);
}

bool AudioDocument::fromJSON (const juce::var& state)
{
    if (state.isVoid() || ! state.hasProperty ("clips"))
        return false;

    const auto* clipArray = state.getProperty ("clips", {}).getArray();
    if (clipArray == nullptr || clipArray->isEmpty())
        return false;

    std::vector<Clip> parsed;
    for (const auto& entry : *clipArray)
    {
        Clip c;
        c.sourceStart = (juce::int64) (double) entry.getProperty ("s", -1.0);
        c.length      = (juce::int64) (double) entry.getProperty ("l", -1.0);
        if (c.length <= 0 || c.sourceStart < 0
            || c.sourceStart + c.length > (juce::int64) source.getNumSamples())
            return false;   // corrupt sidecar: caller falls back to the full take
        parsed.push_back (c);
    }

    clips = std::move (parsed);
    selection.clearSelection();

    // Restore processing state if present (older sidecars simply keep defaults).
    processing = ProcessingState::fromJSON (state.getProperty ("dsp", {}));

    ++version;
    return true;
}

juce::File AudioDocument::sidecarPathFor (const juce::File& sourceFile)
{
    return sourceFile.withFileExtension ("otoha-edit.json");
}

bool AudioDocument::autosaveState() const
{
    if (sourceFile == juce::File{})
        return false;

    // Write to a temp file first, then move into place — a crash mid-write can
    // only lose the sidecar, never corrupt it or the recording.
    const auto json = juce::JSON::toString (toJSON(), true);
    const auto path = sidecarPathFor (sourceFile);
    const auto temp = path.withFileExtension ("tmp");

    if (! temp.replaceWithText (json))
        return false;

    if (temp.moveFileTo (path))
        return true;

    temp.deleteFile();
    return path.replaceWithText (json);
}

bool AudioDocument::hasRestorableState() const
{
    return sourceFile != juce::File{} && sidecarPathFor (sourceFile).existsAsFile();
}

bool AudioDocument::restoreFromSidecar()
{
    if (! hasRestorableState())
        return false;

    const auto parsed = juce::JSON::parse (sidecarPathFor (sourceFile));
    return fromJSON (parsed);
}

void AudioDocument::clearSavedState() const
{
    if (sourceFile != juce::File{})
        sidecarPathFor (sourceFile).deleteFile();
}

// =============================================================================
// Clipboard preparation (rate conversion + channel adaptation)
// =============================================================================
juce::AudioBuffer<float> prepareClipboardForDocument (const AudioClipboard& clipboard,
                                                      double destSampleRate, int destChannels,
                                                      bool* wasConverted)
{
    if (wasConverted != nullptr)
        *wasConverted = false;

    if (clipboard.isEmpty())
        return {};

    auto converted = clipboard.data;

    if (clipboard.sampleRate > 0.0 && clipboard.sampleRate != destSampleRate)
    {
        converted = resampleLinear (converted, clipboard.sampleRate, destSampleRate);
        if (wasConverted != nullptr) *wasConverted = true;
    }

    if (destChannels > 0 && converted.getNumChannels() != destChannels)
    {
        converted = adaptChannels (converted, destChannels);
        if (wasConverted != nullptr) *wasConverted = true;
    }

    return converted;
}
} // namespace otoha
