#include "EditorView.h"

#include "../Core/RecordingSupport.h"
#include "../Editor/TimelineRenderer.h"
#include "../Editor/TimelineSource.h"

#include <algorithm>
#include <cmath>

// =============================================================================
// WaveformDisplay
//
// Peaks are cached per EDIT STATE, indexed by timeline position (rebuildPeaks
// runs on open and after every edit). Drawing any zoom level is pure array
// lookups — repaints never decode audio or touch disk.
// =============================================================================
class EditorView::WaveformDisplay : public juce::Component
{
public:
    explicit WaveformDisplay (EditorView& parent) : view (parent) {}

    void setDocument (std::shared_ptr<otoha::AudioDocument> newDoc)
    {
        doc = std::move (newDoc);
        zoomSamplesPerPixel = 0.0;   // force fit-all on first layout/paint
        viewStartSample = 0;
        rebuildPeaks();
    }

    void rebuildPeaks()
    {
        peaks.clear();
        if (doc == nullptr || doc->getClips().empty())
            return;

        const auto totalTimeline = doc->totalSamples();
        const double seconds = (double) totalTimeline / doc->getSampleRate();
        const int bucketCount = juce::jlimit (256, 100000, (int) (seconds * 40.0));
        peaks.assign ((size_t) bucketCount, 0.0f);

        constexpr int chunkFrames = 1 << 15;
        const int channels = doc->getNumChannels();
        juce::AudioBuffer<float> chunk (channels, chunkFrames);
        float* ptrs[2] = { chunk.getWritePointer (0),
                           channels > 1 ? chunk.getWritePointer (1) : nullptr };

        for (juce::int64 pos = 0; pos < totalTimeline; pos += chunkFrames)
        {
            const int frames = (int) juce::jmin ((juce::int64) chunkFrames, totalTimeline - pos);
            doc->readRange (pos, frames, ptrs, channels);

            for (int i = 0; i < frames; ++i)
            {
                float m = 0.0f;
                for (int ch = 0; ch < channels; ++ch)
                    m = juce::jmax (m, std::abs (ptrs[ch][i]));

                const size_t bucket = bucketIndex (pos + i, bucketCount);
                peaks[bucket] = juce::jmax (peaks[bucket], m);
            }
        }

        if (zoomSamplesPerPixel <= 0.0)
        {
            if (getWidth() > 0)
                fitAll();
            else
                pendingFit = true;
        }
    }

    void fitAll()
    {
        if (doc == nullptr || doc->getClips().empty())
            return;
        if (getWidth() <= 0)
        {
            pendingFit = true;
            return;
        }
        zoomSamplesPerPixel = std::max (1.0, (double) doc->totalSamples() / (double) getWidth());
        viewStartSample = 0;
        pendingFit = false;
        repaint();
    }

    void setZoom (double samplesPerPixel)
    {
        if (doc == nullptr || doc->getClips().empty())
            return;

        const auto centre = viewStartSample
                          + (juce::int64) ((double) getWidth() * zoomSamplesPerPixel * 0.5);
        zoomSamplesPerPixel = juce::jlimit (1.0,
                                            std::max (1.0, (double) doc->totalSamples() / 32.0),
                                            samplesPerPixel);
        viewStartSample = juce::jmax ((juce::int64) 0,
                                      centre - (juce::int64) ((double) getWidth() * zoomSamplesPerPixel * 0.5));
        clampScroll();
        repaint();
    }

    juce::int64 xToSample (double x) const
    {
        return viewStartSample + (juce::int64) (x * zoomSamplesPerPixel);
    }

    double sampleToX (juce::int64 sample) const
    {
        if (zoomSamplesPerPixel <= 0.0) return -1000.0;
        return (double) (sample - viewStartSample) / zoomSamplesPerPixel;
    }

    double getZoomSafe() const { return zoomSamplesPerPixel > 0.0 ? zoomSamplesPerPixel : 1.0; }

    void resized() override
    {
        if (pendingFit)
            fitAll();
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat().reduced (8.0f);
        g.setColour (findColour (juce::ResizableWindow::backgroundColourId).contrasting (0.06f));
        g.fillRoundedRectangle (area, 10.0f);
        g.drawRoundedRectangle (area, 10.0f, 1.0f);

        if (doc == nullptr || doc->getClips().empty())
        {
            g.setColour (juce::Colours::grey);
            g.setFont (juce::FontOptions (15.0f));
            g.drawText ("No recording open.", area, juce::Justification::centred);
            return;
        }

        if (zoomSamplesPerPixel <= 0.0 && getWidth() > 0)
            fitAll();

        const auto total = doc->totalSamples();
        const auto viewEnd = xToSample ((double) getWidth());

        // --- selection highlight -------------------------------------------------
        const auto sel = doc->getSelection();
        if (! sel.isEmpty())
        {
            const float sx = (float) sampleToX (sel.start);
            const float ex = (float) sampleToX (sel.end);
            g.setColour (juce::Colour (0xff4fc3a1).withAlpha (0.22f));
            g.fillRect (sx, area.getY(), std::max (2.0f, ex - sx), area.getHeight());
            g.setColour (juce::Colour (0xff4fc3a1));
            g.drawVerticalLine ((int) sx, area.getY(), area.getBottom());
            g.drawVerticalLine ((int) ex, area.getY(), area.getBottom());
        }

        // --- waveform bars ---------------------------------------------------------
        g.setColour (juce::Colour (0xff4fc3a1));
        const int bucketCount = (int) peaks.size();
        const float midY = area.getCentreY();
        const float halfHeight = area.getHeight() * 0.46f;

        if (bucketCount > 0)
        {
            const int xa = (int) std::max (area.getX(), 0.0f);
            const int xb = (int) std::min ((double) area.getRight(), (double) getWidth());

            for (int px = xa; px < xb; ++px)
            {
                const auto s0 = xToSample ((double) px);
                const auto s1 = xToSample ((double) px + 1.0);
                if (s1 <= 0 || s0 >= total)
                    continue;

                const int b0 = bucketIndex (juce::jlimit ((juce::int64) 0, total, s0), bucketCount);
                const int b1 = juce::jlimit (b0, bucketCount - 1,
                                             bucketIndex (juce::jlimit ((juce::int64) 0, total, s1), bucketCount));

                float m = 0.0f;
                for (int i = b0; i <= b1; ++i)
                    m = std::max (m, peaks[(size_t) i]);

                const float h = std::max (1.5f, m * halfHeight);
                g.fillRect ((float) px, midY - h, 1.0f, h * 2.0f);
            }
        }

        // --- playback cursor ----------------------------------------------------------
        const auto playPos = view.playbackPositionSamples();
        if (playPos >= viewStartSample && playPos <= viewEnd)
        {
            g.setColour (juce::Colours::white);
            g.drawVerticalLine ((int) sampleToX (playPos), area.getY(), area.getBottom());
        }

        // --- time ruler ------------------------------------------------------------------
        g.setColour (juce::Colours::grey);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (otoha::formatDuration ((double) viewStartSample / doc->getSampleRate()),
                    area.removeFromTop (16).withTrimmedLeft (6), juce::Justification::centredLeft);
        g.drawText (otoha::formatDuration ((double) juce::jmin (viewEnd, total) / doc->getSampleRate()),
                    area.withHeight (16), juce::Justification::centredRight);
    }

    // --- mouse: click = seek cursor, drag = selection -------------------------------
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (doc == nullptr) return;
        dragAnchor = xToSample ((double) e.position.x);
        view.onWaveformClick (dragAnchor);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (doc == nullptr) return;
        doc->setSelection (dragAnchor, xToSample ((double) e.position.x));   // normalizes + clamps
        view.selectionChanged();
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        if (wheel.deltaY != 0.0f)
            setZoom (zoomSamplesPerPixel * (wheel.deltaY > 0.0f ? 0.8 : 1.25));   // wheel = zoom
    }

private:
    size_t bucketIndex (juce::int64 timelineSample, int bucketCount) const
    {
        const auto total = doc != nullptr ? doc->totalSamples() : 1;
        return (size_t) juce::jlimit (0, bucketCount - 1,
                                      (int) ((timelineSample * (juce::int64) bucketCount) / total));
    }

    void clampScroll()
    {
        if (doc == nullptr) return;
        const auto last = doc->totalSamples()
                        - (juce::int64) ((double) getWidth() * zoomSamplesPerPixel);
        viewStartSample = juce::jlimit ((juce::int64) 0,
                                        juce::jmax ((juce::int64) 0, last),
                                        viewStartSample);
    }

    EditorView& view;
    std::shared_ptr<otoha::AudioDocument> doc;
    std::vector<float> peaks;
    double zoomSamplesPerPixel = 0.0;     // <=0 means "fit on first layout"
    juce::int64 viewStartSample = 0;
    juce::int64 dragAnchor = 0;
    bool pendingFit = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformDisplay)
};

// =============================================================================
// EditorView
// =============================================================================
EditorView::EditorView (Player& pl, LibraryService& lib, std::function<void()> back)
    : player (pl), library (lib), backToLibrary (std::move (back))
{
    addAndMakeVisible (backButton);
    backButton.onClick = [this]
    {
        confirmDiscardOrSave ([this] (bool proceed)
        {
            if (proceed)
                closeEditor();
        });
    };

    titleLabel.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    menuButton.onClick = [this]
    {
        juce::PopupMenu m;
        m.addItem (1, "Save");
        m.addItem (2, "Export...");
        m.addSeparator();
        m.addItem (3, "Discard changes");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&menuButton),
                         [this] (int r)
                         {
                             if (r == 1)      saveChanges();
                             else if (r == 2) exportAs();
                             else if (r == 3)
                                 confirmDiscardOrSave ([this] (bool proceed)
                                 {
                                     if (proceed)
                                     {
                                         doc->clearSavedState();
                                         closeEditor();
                                     }
                                 });
                         });
    };
    addAndMakeVisible (menuButton);

    wave = std::make_unique<WaveformDisplay> (*this);
    addAndMakeVisible (*wave);

    timeLabel.setFont (juce::FontOptions (14.0f));
    timeLabel.setJustificationType (juce::Justification::centred);
    timeLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (timeLabel);

    for (auto* b : { &cutButton, &copyButton, &pasteButton, &rippleDeleteButton,
                     &trimButton, &undoButton, &redoButton, &playButton,
                     &zoomInButton, &zoomOutButton, &zoomFitButton,
                     &enhanceButton, &exportButton, &saveButton })
        addAndMakeVisible (*b);

    cutButton.onClick          = [this] { cutSelected(); };
    copyButton.onClick         = [this] { copySelected(); };
    pasteButton.onClick        = [this] { pasteAtCursor(); };
    rippleDeleteButton.onClick = [this] { rippleDeleteSelection(); };
    trimButton.onClick         = [this] { trimSelection(); };
    undoButton.onClick         = [this] { undo(); };
    redoButton.onClick         = [this] { redo(); };
    playButton.onClick         = [this] { playPause(); };   // selection-aware
    zoomInButton.onClick       = [this] { wave->setZoom (wave->getZoomSafe() * 0.5); };
    zoomOutButton.onClick      = [this] { wave->setZoom (wave->getZoomSafe() * 2.0); };
    zoomFitButton.onClick      = [this] { wave->fitAll(); };
    exportButton.onClick       = [this] { exportAs(); };
    saveButton.onClick         = [this] { saveChanges(); };

    enhanceButton.setEnabled (false);   // Milestone 5

    refreshButtonsAndTitle();
    startTimerHz (30);
}

bool EditorView::openItem (const otoha::MediaItem& mediaItem, juce::String& errorOut)
{
    stopPlayback();

    auto newDoc = std::make_shared<otoha::AudioDocument>();
    if (! newDoc->loadFromFile (mediaItem.file, errorOut))
        return false;

    // Autosave recovery: restore the previous edit state when one exists.
    newDoc->restoreFromSidecar();   // corrupt sidecars simply fall back to full take

    item = mediaItem;
    doc = newDoc;
    editorActive = true;
    clipboard.clear();
    playingSelection = false;
    loadedSourceVersion = 0xFFFFFFFF;

    wave->setDocument (doc);
    refreshButtonsAndTitle();
    return true;
}

bool EditorView::isOpen() const { return editorActive && doc != nullptr; }

bool EditorView::isEditingFile (const juce::File& file) const
{
    return isOpen() && item.file == file;
}

void EditorView::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void EditorView::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop (54);   // title strip handled by header row below

    auto header = bounds.removeFromTop (40).reduced (16, 4);
    backButton.setBounds (header.removeFromLeft (44).withHeight (30));
    header.removeFromLeft (8);
    titleLabel.setBounds (header.removeFromLeft (header.getWidth() - 48));
    menuButton.setBounds (header.removeFromRight (44).withHeight (30));

    auto labelRow = bounds.removeFromTop (26).reduced (16, 0);
    timeLabel.setBounds (labelRow);

    // Bottom: Enhance (placeholder) | zoom controls | Export + Save.
    auto bottomRow = bounds.removeFromBottom (38).reduced (16, 2);
    enhanceButton.setBounds (bottomRow.removeFromLeft (90).reduced (2, 2));
    saveButton.setBounds (bottomRow.removeFromRight (76).reduced (2, 2));
    exportButton.setBounds (bottomRow.removeFromRight (84).reduced (2, 2));
    zoomFitButton.setBounds (bottomRow.removeFromRight (52).reduced (2, 2));
    zoomOutButton.setBounds (bottomRow.removeFromRight (36).reduced (2, 2));
    zoomInButton.setBounds (bottomRow.removeFromRight (36).reduced (2, 2));

    // Edit buttons: two rows above the bottom strip.
    auto buttonRows = bounds.removeFromBottom (88).reduced (16, 4);
    auto row1 = buttonRows.removeFromTop (42);
    rippleDeleteButton.setBounds (row1.removeFromRight (130).reduced (2, 3));
    pasteButton.setBounds (row1.removeFromRight (78).reduced (2, 3));
    copyButton.setBounds (row1.removeFromRight (72).reduced (2, 3));
    cutButton.setBounds (row1.removeFromRight (66).reduced (2, 3));
    playButton.setBounds (row1.withSizeKeepingCentre (110, 32));

    auto row2 = buttonRows.removeFromTop (42);
    trimButton.setBounds (row2.removeFromRight (80).reduced (2, 3));
    redoButton.setBounds (row2.removeFromRight (72).reduced (2, 3));
    undoButton.setBounds (row2.removeFromRight (72).reduced (2, 3));

    wave->setBounds (bounds.reduced (16, 6));
}

bool EditorView::keyPressed (const juce::KeyPress& key)
{
    if (! isOpen())
        return false;

    const auto cmd = key.getModifiers().isCommandDown();

    if (key.isKeyCode (juce::KeyPress::spaceKey))                        { playPause(); return true; }
    if (key.isKeyCode (juce::KeyPress::deleteKey))                       { rippleDeleteSelection(); return true; }
    if (cmd && key.isKeyCode ('Z') && key.getModifiers().isShiftDown())  { redo(); return true; }
    if (cmd && key.isKeyCode ('Z'))                                      { undo(); return true; }
    if (cmd && key.isKeyCode ('Y'))                                      { redo(); return true; } // Windows convention
    if (cmd && key.isKeyCode ('X'))                                      { cutSelected(); return true; }
    if (cmd && key.isKeyCode ('C'))                                      { copySelected(); return true; }
    if (cmd && key.isKeyCode ('V'))                                      { pasteAtCursor(); return true; }
    if (cmd && key.isKeyCode ('A'))                                      { doc->setSelection (0, doc->totalSamples()); selectionChanged(); return true; }

    // Shift+Arrow selection nudging.
    if (key.getModifiers().isShiftDown())
    {
        const auto sel = doc->getSelection();
        const auto step = juce::jmax ((juce::int64) 1, (juce::int64) (doc->getSampleRate() / 20));
        if (key.isKeyCode (juce::KeyPress::leftKey))  { doc->setSelection (sel.start, sel.end - step); selectionChanged(); return true; }
        if (key.isKeyCode (juce::KeyPress::rightKey)) { doc->setSelection (sel.start, sel.end + step); selectionChanged(); return true; }
    }

    return false;
}

// =============================================================================
// Playback hooks (used by WaveformDisplay)
// =============================================================================
int64_t EditorView::playbackPositionSamples() const
{
    if (doc == nullptr || doc->getSampleRate() <= 0.0)
        return 0;
    return (juce::int64) (player.getPositionSeconds() * doc->getSampleRate());
}

void EditorView::onWaveformClick (juce::int64 sample)
{
    if (! isOpen()) return;

    playingSelection = false;
    ensurePlaybackSource();
    seek ((double) juce::jlimit ((juce::int64) 0, doc->totalSamples(), sample) / doc->getSampleRate());
    wave->repaint();
}

void EditorView::selectionChanged()
{
    refreshButtonsAndTitle();
    wave->repaint();
}

// =============================================================================
// Transport
// =============================================================================
void EditorView::ensurePlaybackSource()
{
    if (doc == nullptr || loadedSourceVersion == doc->getVersion())
        return;

    stopPlayback();
    player.loadCustomSource (std::make_unique<TimelineSource> (doc), doc->getSampleRate());
    loadedSourceVersion = doc->getVersion();
}

void EditorView::playPause()
{
    if (! isOpen()) return;
    ensurePlaybackSource();

    if (player.isPlaying())
    {
        player.pause();
        playingSelection = false;
        return;
    }

    const auto sel = doc->getSelection();
    playingSelection = ! sel.isEmpty();
    seek ((playingSelection ? sel.start : 0) / doc->getSampleRate());
    player.play();
}

void EditorView::stopPlayback()
{
    playingSelection = false;
    player.stop();
}

void EditorView::seek (double seconds)
{
    player.setPositionSeconds (seconds);
}

// =============================================================================
// Edit commands
// =============================================================================
void EditorView::cutSelected()
{
    if (! isOpen() || doc->getSelection().isEmpty()) return;
    stopPlayback();
    doc->cutSelectedRange (clipboard);
    afterEditRebuild();
}

void EditorView::copySelected()
{
    if (! isOpen() || doc->getSelection().isEmpty()) return;
    doc->copySelectedRange (clipboard);
    refreshButtonsAndTitle();
}

void EditorView::pasteAtCursor()
{
    if (! isOpen() || clipboard.isEmpty()) return;
    stopPlayback();

    juce::String error;
    if (! doc->pasteAt (doc->getSelection().start, clipboard, error))
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Paste", error);
    else
        afterEditRebuild();
}

void EditorView::rippleDeleteSelection()
{
    if (! isOpen() || doc->getSelection().isEmpty()) return;
    stopPlayback();
    const auto sel = doc->getSelection();
    doc->rippleDelete (sel.start, sel.length());
    afterEditRebuild();
}

void EditorView::trimSelection()
{
    if (! isOpen() || doc->getSelection().isEmpty()) return;
    stopPlayback();
    doc->trimToSelection();
    afterEditRebuild();
}

void EditorView::undo()
{
    if (! isOpen() || ! doc->canUndo()) return;
    stopPlayback();
    doc->undo();
    afterEditRebuild();
}

void EditorView::redo()
{
    if (! isOpen() || ! doc->canRedo()) return;
    stopPlayback();
    doc->redo();
    afterEditRebuild();
}

void EditorView::afterEditRebuild()
{
    loadedSourceVersion = 0xFFFFFFFF;   // stale timeline — rebuilt on next Play
    doc->autosaveState();               // tiny JSON, never audio data
    wave->rebuildPeaks();
    wave->fitAll();
    refreshButtonsAndTitle();
}

// =============================================================================
// Save / export / close
// =============================================================================
void EditorView::saveChanges()
{
    if (! isOpen() || ! doc->isModified())
        return;

    stopPlayback();

    const otoha::TimelineRenderer renderer (doc);
    const auto destName = item.file.getFileNameWithoutExtension() + " (edited).wav";
    const auto destination = library.getBaseDirectory()
                                 .getChildFile ("Library").getChildFile ("Audio")
                                 .getChildFile (destName);

    juce::String error;
    if (! renderer.renderToWav (destination, error))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                "Couldn't save your changes", error);
        return;
    }

    // Register as its own library item: the original stays untouched and both
    // remain recoverable. Library metadata for the new item is fresh by design.
    library.registerAudioFile (destination);

    doc->clearSavedState();
    doc->markUnmodified();
    item.file = destination;
    item.displayName = destination.getFileNameWithoutExtension();

    refreshButtonsAndTitle();
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                            "Saved",
                                            "Saved an edited copy to your Library:\n" + destName);
}

void EditorView::exportAs()
{
    if (! isOpen()) return;

    chooser = std::make_unique<juce::FileChooser> ("Export edited recording as WAV",
                                                   juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                                       .getChildFile (item.displayName + ".wav"),
                                                   "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto target = fc.getResult();
                              if (target == juce::File{}) return;

                              const otoha::TimelineRenderer renderer (doc);
                              juce::String error;
                              if (! renderer.renderToWav (target, error))
                                  juce::AlertWindow::showMessageBoxAsync (
                                      juce::MessageBoxIconType::WarningIcon, "Export failed", error);
                              else
                                  juce::AlertWindow::showMessageBoxAsync (
                                      juce::MessageBoxIconType::InfoIcon, "Exported",
                                      "Exported to:\n" + target.getFullPathName());
                          });
}

bool EditorView::confirmDiscardOrSave (const std::function<void(bool)>& onDecided)
{
    if (! isOpen() || ! doc->isModified())
    {
        onDecided (true);
        return false;
    }

    auto* window = new juce::AlertWindow ("Unsaved changes",
                                          "You have unsaved changes.\n\n"
                                          "Keep editing, save them as an edited copy,\n"
                                          "or discard them (your original stays safe).",
                                          juce::MessageBoxIconType::QuestionIcon);
    window->addButton ("Keep Editing", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Discard", 2);

    window->enterModalState (true,
        juce::ModalCallbackFunction::create ([this, window, onDecided] (int result)
        {
            if (result == 1)
                saveChanges();
            else if (result == 2)
                doc->clearSavedState();

            onDecided (result != 0);
        }),
        true /* deleteWhenDismissed */);

    return true;
}

void EditorView::closeEditor()
{
    stopPlayback();
    player.unload();               // release the timeline from the transport
    editorActive = false;
    doc = nullptr;
    loadedSourceVersion = 0xFFFFFFFF;
    refreshButtonsAndTitle();
    wave->repaint();

    if (backToLibrary)
        backToLibrary();
}

// =============================================================================
// Timer / state
// =============================================================================
void EditorView::timerCallback()
{
    if (! isOpen())
        return;

    // Auto-stop at the end of a selection preview.
    if (playingSelection && player.isPlaying())
    {
        const auto sel = doc->getSelection();
        if (player.getPositionSeconds() * doc->getSampleRate() >= (double) sel.end)
        {
            player.pause();
            player.setPositionSeconds ((double) sel.start / doc->getSampleRate());
            playingSelection = false;
        }
    }

    // Time readout: cursor, or selection span while one exists.
    const auto sel = doc->getSelection();
    if (! sel.isEmpty())
    {
        timeLabel.setText (otoha::formatDuration ((double) sel.start / doc->getSampleRate())
                               + "  →  " + otoha::formatDuration ((double) sel.end / doc->getSampleRate())
                               + "   (" + otoha::formatDuration ((double) sel.length() / doc->getSampleRate()) + ")",
                           juce::dontSendNotification);
    }
    else
    {
        timeLabel.setText (otoha::formatDuration ((double) playbackPositionSamples() / doc->getSampleRate()),
                           juce::dontSendNotification);
    }

    wave->repaint();
}

void EditorView::refreshButtonsAndTitle()
{
    const bool open = isOpen();
    const bool hasSel = open && ! doc->getSelection().isEmpty();

    titleLabel.setText (open ? (doc->isModified()
                                    ? item.displayName + " *"
                                    : item.displayName)
                             : "Editor",
                        juce::dontSendNotification);

    cutButton.setEnabled (hasSel);
    copyButton.setEnabled (hasSel);
    rippleDeleteButton.setEnabled (hasSel);
    trimButton.setEnabled (hasSel);
    pasteButton.setEnabled (open && ! clipboard.isEmpty());
    undoButton.setEnabled (open && doc->canUndo());
    redoButton.setEnabled (open && doc->canRedo());
    playButton.setEnabled (open);
    exportButton.setEnabled (open);
    saveButton.setEnabled (open && doc->isModified());
}
