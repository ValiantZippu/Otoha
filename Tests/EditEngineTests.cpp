/*
    EditEngineTests — headless tests for the M4 editing engine (no UI, no
    audio hardware). Synthetic source buffers use distinct constant values per
    region so timeline contents are trivially verifiable.
*/
#include "../Source/Editor/AudioDocument.h"
#include "../Source/Editor/TimelineRenderer.h"

#include <cstdio>

using otoha::AudioDocument;
using otoha::AudioClipboard;

namespace
{
bool expect (bool condition, const char* message)
{
    if (! condition)
        std::printf ("FAIL: %s\n", message);
    return condition;
}

constexpr int blockSamples = 1000;

/** Fills a channel range [from, from+count) with `value`. */
void fill (juce::AudioBuffer<float>& b, int ch, int from, int count, float value)
{
    for (int i = 0; i < count; ++i)
        b.setSample (ch, from + i, value);
}

float firstSampleOfRegion (const AudioDocument& doc, juce::int64 timelinePos)
{
    auto buf = doc.readRangeToBuffer (timelinePos, 1);
    return buf.getSample (0, 0);
}

/** A|B|C|D document: four 1000-sample blocks valued .1/.2/.3/.4 */
std::shared_ptr<AudioDocument> makeABCD (int channels = 1, double rate = 48000.0)
{
    juce::AudioBuffer<float> data (channels, blockSamples * 4);
    for (int ch = 0; ch < channels; ++ch)
    {
        fill (data, ch, 0,                    blockSamples, 0.1f);   // A
        fill (data, ch, blockSamples,         blockSamples, 0.2f);   // B
        fill (data, ch, blockSamples * 2,     blockSamples, 0.3f);   // C
        fill (data, ch, blockSamples * 3,     blockSamples, 0.4f);   // D
    }
    return std::make_shared<AudioDocument> (data, rate);
}

bool timelineEqualsBlocks (const AudioDocument& doc, const char* pattern)
{
    const juce::String blocks (pattern);
    if ((juce::int64) blocks.length() * blockSamples != doc.totalSamples())
        return false;

    for (int i = 0; i < blocks.length(); ++i)
    {
        const float expected = 0.1f + 0.1f * (float) (blocks[(juce::uint32) i] - 'A');
        auto probe = doc.readRangeToBuffer ((juce::int64) i * blockSamples + blockSamples / 2, 1);
        if (std::abs (probe.getSample (0, 0) - expected) > 0.001f)
            return false;
    }
    return true;
}
} // namespace

int main()
{
    bool ok = true;

    // --- selection ------------------------------------------------------------
    {
        auto doc = makeABCD();
        doc->setSelection (3000, 500);
        ok &= expect (doc->getSelection().start == 500 && doc->getSelection().end == 3000,
                      "reversed selection is normalized");
        doc->setSelection (700, 700);
        ok &= expect (doc->getSelection().isEmpty(), "zero-length selection counts as empty");
    }

    // --- cut -------------------------------------------------------------------
    {
        auto doc = makeABCD();
        AudioClipboard clip;
        doc->setSelection (blockSamples, blockSamples * 2);   // B..C
        doc->cutSelectedRange (clip);

        ok &= expect (timelineEqualsBlocks (*doc, "AD"), "cut B..C leaves A D");
        ok &= expect (! clip.isEmpty() && clip.data.getNumSamples() == blockSamples * 2,
                      "clipboard holds the cut audio");
        ok &= expect (std::abs (clip.data.getSample (0, 0) - 0.2f) < 0.001f
                          && std::abs (clip.data.getSample (0, clip.data.getNumSamples() - 1) - 0.3f) < 0.001f,
                      "clipboard starts in B and ends in C");
    }

    // --- ripple delete ----------------------------------------------------------
    {
        auto doc = makeABCD();
        doc->rippleDelete (blockSamples, blockSamples);       // remove B only
        ok &= expect (timelineEqualsBlocks (*doc, "ACD"), "ripple delete closes the gap");

        // Selection spanning two clips deletes both parts correctly.
        auto doc2 = makeABCD();
        doc2->rippleDelete (blockSamples / 2, blockSamples);  // half B + half C
        ok &= expect (doc2->totalSamples() == blockSamples * 3, "spanning delete keeps length right");
        ok &= expect (std::abs (firstSampleOfRegion (*doc2, blockSamples / 2 - 10) - 0.1f) < 0.001f,
                      "A intact before the cut");
        ok &= expect (std::abs (firstSampleOfRegion (*doc2, blockSamples / 2 + 10) - 0.35f) < 0.05f,
                      "second half of C follows immediately");
    }

    // --- trim --------------------------------------------------------------------
    {
        auto doc = makeABCD();
        doc->setSelection (blockSamples, blockSamples * 2);   // B..C
        doc->trimToSelection();
        ok &= expect (timelineEqualsBlocks (*doc, "BC"), "trim keeps only the selection");
    }

    // --- paste ---------------------------------------------------------------------
    {
        auto doc = makeABCD();
        AudioClipboard clip;
        juce::String pasteError;
        doc->setSelection (0, blockSamples);                  // copy A
        doc->copySelectedRange (clip);
        doc->clearSelection();

        doc->pasteAt (doc->totalSamples(), clip, pasteError);         // append at end
        ok &= expect (timelineEqualsBlocks (*doc, "ABCDA"), "paste inserts without overwriting");

        // Insert into the middle: cursor between B and C.
        auto doc2 = makeABCD();
        doc2->pasteAt (blockSamples * 2, clip, pasteError);
        ok &= expect (timelineEqualsBlocks (*doc2, "ABACD"), "mid-timeline insert is A B A C D");
        ok &= expect (doc2->totalSamples() == blockSamples * 5, "insert grows the timeline by clipboard length");
        ok &= expect (std::abs (firstSampleOfRegion (*doc2, blockSamples * 2) - 0.1f) < 0.001f,
                      "pasted material starts at the cursor");
        ok &= expect (std::abs (firstSampleOfRegion (*doc2, blockSamples * 3) - 0.3f) < 0.001f,
                      "existing audio continues after pasted material");
    }

    // --- undo / redo ------------------------------------------------------------------
    {
        auto doc = makeABCD();
        AudioClipboard clip;
        doc->setSelection (blockSamples, blockSamples * 2);
        doc->cutSelectedRange (clip);
        ok &= expect (timelineEqualsBlocks (*doc, "AD"), "edited state");

        doc->undo();
        ok &= expect (timelineEqualsBlocks (*doc, "ABCD"), "undo restores original");
        ok &= expect (doc->canRedo(), "redo available after undo");

        doc->redo();
        ok &= expect (timelineEqualsBlocks (*doc, "AD"), "redo re-applies the edit");
        ok &= expect (doc->canUndo(), "undo available after redo");

        doc->undo();
        doc->undo();
        ok &= expect (doc->canUndo() == false && timelineEqualsBlocks (*doc, "ABCD"),
                      "undo stack bottoms out at the initial state");

        // Invalid operations must be safe no-ops.
        const int64_t before = doc->totalSamples();
        doc->rippleDelete (5, 0);
        doc->trimToSelection();     // empty selection
        ok &= expect (doc->totalSamples() == before, "empty-selection edits do nothing");
    }

    // --- multi-channel -----------------------------------------------------------------
    {
        auto doc = makeABCD (2);
        ok &= expect (doc->getNumChannels() == 2, "stereo document stays stereo");
        doc->setSelection (blockSamples, blockSamples * 2);
        doc->rippleDelete (blockSamples, blockSamples);

        auto mid = doc->readRangeToBuffer (blockSamples / 2, 1);
        ok &= expect (std::abs (mid.getSample (1, 0) - 0.1f) < 0.001f, "right channel edited identically");
        ok &= expect (timelineEqualsBlocks (*doc, "ACD"), "stereo timeline matches mono behaviour");
    }

    // --- cross-sample-rate paste --------------------------------------------------------
    {
        auto doc = makeABCD (1, 48000.0);

        AudioClipboard foreign;
        foreign.sampleRate = 44100.0;
        foreign.numChannels = 2;
        foreign.data.setSize (2, 44100);      // one second of stereo @44.1k
        for (int ch = 0; ch < 2; ++ch)
            fill (foreign.data, ch, 0, 44100, 0.25f);

        juce::String crossRateError;
        ok &= expect (doc->pasteAt (0, foreign, crossRateError), "cross-rate paste succeeds");
        // 44100 frames @44.1k == exactly one second == 48000 frames @48k.
        ok &= expect (doc->totalSamples() == blockSamples * 4 + 48000,
                      "pasted length converted to destination sample rate");

        auto head = doc->readRangeToBuffer (24000, 1);
        ok &= expect (std::abs (head.getSample (0, 0) - 0.25f) < 0.01f,
                      "converted samples land at the insertion point");
    }

    // --- sidecar persistence --------------------------------------------------------------
    {
        const auto sidecarTarget = juce::File::createTempFile ("otoha_edit_src.wav");

        // Same ABCD source; edits live in the clip list only.
        auto doc = makeABCD();
        doc->sourceFileForTest() = sidecarTarget;

        doc->setSelection (blockSamples, blockSamples * 2);
        doc->rippleDelete (blockSamples, blockSamples);
        ok &= expect (doc->autosaveState(), "autosave writes the sidecar");
        ok &= expect (doc->hasRestorableState(), "sidecar exists");

        // A freshly opened document pointing at the same location restores it.
        auto reopened = makeABCD();
        reopened->sourceFileForTest() = sidecarTarget;
        ok &= expect (reopened->restoreFromSidecar(), "sidecar restores");
        ok &= expect (timelineEqualsBlocks (*reopened, "ACD"), "restored state matches saved edits");

        reopened->clearSavedState();
        ok &= expect (! reopened->hasRestorableState(), "clearSavedState removes the sidecar");
        sidecarTarget.deleteFile();
    }

    // --- renderer ------------------------------------------------------------------------------
    {
        auto doc = makeABCD();
        doc->setSelection (blockSamples, blockSamples * 3);
        doc->trimToSelection();

        const otoha::TimelineRenderer renderer (doc);
        const auto dest = juce::File::createTempFile ("otoha_render_test.wav");
        juce::String error;

        ok &= expect (renderer.renderToWav (dest, error), "render succeeds");
        ok &= expect (error.isEmpty(), "no error reported");
        ok &= expect (dest.existsAsFile() && dest.getSize() > 44, "rendered file exists with content");

        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (dest));
        ok &= expect (reader != nullptr && reader->lengthInSamples == (juce::uint64) (blockSamples * 2),
                      "rendered duration equals the edited timeline");
        dest.deleteFile();
    }

    // --- M13 #14/#15: timeline edge cases -------------------------------------
    {
        juce::String errorSink;

        // delete at start
        {
            auto doc = makeABCD();
            doc->rippleDelete (0, blockSamples);
            ok &= expect (timelineEqualsBlocks (*doc, "BCD"), "delete at start removes A");
        }

        // delete at end (length past EOF is clamped)
        {
            auto doc = makeABCD();
            doc->rippleDelete (blockSamples * 3, blockSamples * 99);
            ok &= expect (timelineEqualsBlocks (*doc, "ABC"), "delete at end is clamped to source length");
        }

        // delete ENTIRE recording: applyClips refuses an empty timeline, so the
        // document must stay valid and unchanged (#15 — no zero-sample state)
        {
            auto doc = makeABCD();
            doc->rippleDelete (0, doc->totalSamples());
            ok &= expect (timelineEqualsBlocks (*doc, "ABCD"),
                          "deleting everything is refused; timeline stays intact");
            ok &= expect (! doc->canUndo(), "refused edit leaves no dangling undo entry");
        }

        // zero-length delete / empty selection are no-ops
        {
            auto doc = makeABCD();
            const auto versionBefore = doc->getVersion();
            doc->rippleDelete (blockSamples, 0);
            ok &= expect (doc->getVersion() == versionBefore && timelineEqualsBlocks (*doc, "ABCD"),
                          "zero-length delete changes nothing");
            doc->setSelection (700, 700);
            doc->trimToSelection();
            ok &= expect (timelineEqualsBlocks (*doc, "ABCD"),
                          "trim with empty selection changes nothing");
        }

        // selection larger than source clamps instead of crashing
        {
            auto doc = makeABCD();
            doc->setSelection (-5000, 999999);
            const auto sel = doc->getSelection();
            ok &= expect (sel.start == 0 && sel.end == doc->totalSamples(),
                          "oversized selection clamps to the document");
        }

        // paste at beginning / end + undo + redo invalidation (#14)
        {
            auto doc = makeABCD();
            AudioClipboard clip;
            doc->setSelection (blockSamples, blockSamples * 2);   // B
            doc->cutSelectedRange (clip);                          // A D

            doc->pasteAt (0, clip, errorSink);                     // B C A D
            ok &= expect (timelineEqualsBlocks (*doc, "BCAD")
                              && std::abs (firstSampleOfRegion (*doc, 0) - 0.2f) < 0.001f,
                          "paste at beginning inserts before everything");

            doc->undo();
            ok &= expect (timelineEqualsBlocks (*doc, "AD"), "undo after paste restores prior state");

            doc->redo();
            ok &= expect (timelineEqualsBlocks (*doc, "BCAD"), "redo after undo reapplies paste");

            doc->rippleDelete (0, blockSamples);                   // fresh edit -> C A D
            ok &= expect (! doc->canRedo(), "a new edit invalidates the redo branch");
            ok &= expect (timelineEqualsBlocks (*doc, "CAD"), "post-redo-invalidation state correct");

            // paste at the very end
            doc->pasteAt (doc->totalSamples(), clip, errorSink);   // A D B
            ok &= expect (std::abs (firstSampleOfRegion (*doc, doc->totalSamples() - 1) - 0.3f) < 0.001f,
                          "paste at end appends after everything");
        }

        // reading zero samples from a valid document is safe (#15)
        {
            auto doc = makeABCD();
            const auto empty = doc->readRangeToBuffer (0, 0);
            ok &= expect (empty.getNumSamples() == 0,
                          "zero-frame reads return an empty buffer without error");
        }
    }

    if (! ok) return 1;
    std::printf ("PASS: editing engine\n");
    return 0;
}
