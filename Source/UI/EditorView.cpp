#include "EditorView.h"

#include "OtohaTheme.h"
#include "Components/OtohaIcons.h"
#include "../Core/RecordingSupport.h"
#include "../Editor/TimelineRenderer.h"
#include "../Editor/TimelineSource.h"
#include "../Export/FfmpegSupport.h"
#include "ExportUi.h"

#include <algorithm>
#include <cmath>

// =============================================================================
// WaveformDisplay  (unchanged — cached peaks, never touches disk during paint)
// =============================================================================
class EditorView::WaveformDisplay : public juce::Component
{
public:
    explicit WaveformDisplay (EditorView& parent) : view (parent) {}

    void setDocument (std::shared_ptr<otoha::AudioDocument> newDoc)
    {
        doc = std::move (newDoc);
        zoomSamplesPerPixel = 0.0;
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
        if (doc == nullptr || doc->getClips().empty()) return;
        if (getWidth() <= 0) { pendingFit = true; return; }
        zoomSamplesPerPixel = std::max (1.0, (double) doc->totalSamples() / (double) getWidth());
        viewStartSample = 0;
        pendingFit = false;
        repaint();
    }

    void setZoom (double samplesPerPixel)
    {
        if (doc == nullptr || doc->getClips().empty()) return;
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

    void resized() override { if (pendingFit) fitAll(); }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat().reduced (8.0f);

        // Card-like background
        g.setColour (otoha::theme::colors::surface());
        g.fillRoundedRectangle (area, (float) otoha::theme::Radius::large);
        g.setColour (otoha::theme::colors::borderSubtle());
        g.drawRoundedRectangle (area, (float) otoha::theme::Radius::large, 1.0f);

        if (doc == nullptr || doc->getClips().empty())
        {
            g.setColour (otoha::theme::colors::textMuted());
            g.setFont (otoha::theme::font (otoha::theme::TextSize::body));
            g.drawText ("No recording open.", area, juce::Justification::centred);
            return;
        }

        if (zoomSamplesPerPixel <= 0.0 && getWidth() > 0)
            fitAll();

        const auto total = doc->totalSamples();
        const auto viewEnd = xToSample ((double) getWidth());

        // Selection highlight
        const auto sel = doc->getSelection();
        if (! sel.isEmpty())
        {
            const float sx = (float) sampleToX (sel.start);
            const float ex = (float) sampleToX (sel.end);
            g.setColour (otoha::theme::colors::waveform().withAlpha (0.22f));
            g.fillRect (sx, area.getY(), std::max (2.0f, ex - sx), area.getHeight());
            g.setColour (otoha::theme::colors::waveform());
            g.drawVerticalLine ((int) sx, area.getY(), area.getBottom());
            g.drawVerticalLine ((int) ex, area.getY(), area.getBottom());
        }

        // Waveform bars
        g.setColour (otoha::theme::colors::waveform());
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
                if (s1 <= 0 || s0 >= total) continue;

                const int b0 = bucketIndex (juce::jlimit ((juce::int64) 0, total, s0), bucketCount);
                const int b1 = juce::jlimit ((int) b0, (int) bucketCount - 1,
                                             (int) bucketIndex (juce::jlimit ((juce::int64) 0, total, s1), bucketCount));

                float m = 0.0f;
                for (int i = b0; i <= b1; ++i)
                    m = std::max (m, peaks[(size_t) i]);

                const float h = std::max (1.5f, m * halfHeight);
                g.fillRect ((float) px, midY - h, 1.0f, h * 2.0f);
            }
        }

        // Playback cursor
        const auto playPos = view.playbackPositionSamples();
        if (playPos >= viewStartSample && playPos <= viewEnd)
        {
            g.setColour (otoha::theme::colors::playhead());
            g.drawVerticalLine ((int) sampleToX (playPos), area.getY(), area.getBottom());
        }

        // Time ruler
        g.setColour (otoha::theme::colors::textMuted());
        g.setFont (otoha::theme::font (otoha::theme::TextSize::caption));
        g.drawText (otoha::formatDuration ((double) viewStartSample / doc->getSampleRate()),
                    area.removeFromTop (16).withTrimmedLeft (6), juce::Justification::centredLeft);
        g.drawText (otoha::formatDuration ((double) juce::jmin (viewEnd, total) / doc->getSampleRate()),
                    area.withHeight (16), juce::Justification::centredRight);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (doc == nullptr) return;
        dragAnchor = xToSample ((double) e.position.x);
        view.onWaveformClick (dragAnchor);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (doc == nullptr) return;
        doc->setSelection (dragAnchor, xToSample ((double) e.position.x));
        view.selectionChanged();
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        if (wheel.deltaY != 0.0f)
            setZoom (zoomSamplesPerPixel * (wheel.deltaY > 0.0f ? 0.8 : 1.25));
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
    double zoomSamplesPerPixel = 0.0;
    juce::int64 viewStartSample = 0;
    juce::int64 dragAnchor = 0;
    bool pendingFit = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformDisplay)
};

// =============================================================================
// EditorView
// =============================================================================
EditorView::~EditorView() {}

EditorView::EditorView (Player& pl, LibraryService& lib, std::function<void()> back,
                        otoha::ExportManager& exportManagerRef,
                        otoha::ExportSettingsStore& exportStoreRef)
    : player (pl), library (lib), backToLibrary (std::move (back)),
      exportManager (exportManagerRef), exportStore (exportStoreRef)
{
    // --- Header bar ---
    addAndMakeVisible (headerCard);
    addAndMakeVisible (backButton);
    backButton.onClick = [this]
    {
        confirmDiscardOrSave ([this] (bool proceed)
        {
            if (proceed) closeEditor();
        });
    };

    titleLabel.setFont (otoha::theme::font (otoha::theme::TextSize::title));
    titleLabel.setColour (juce::Label::textColourId, otoha::theme::colors::textPrimary());
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    menuButton.onClick = [this]
    {
        juce::Array<otoha::ds::MenuItem> items;
        items.add ({ "Save", {}, {}, false, false, false, true, [this] { saveChanges(); } });
        items.add ({ "Export...", {}, {}, false, false, false, true, [this] { exportAs(); } });
        items.add ({ {}, {}, {}, false, false, false, true, {} });  // separator
        items.add ({ "Discard changes", {}, {}, false, false, true, true, [this]
        {
            confirmDiscardOrSave ([this] (bool proceed)
            {
                if (proceed)
                {
                    doc->clearSavedState();
                    closeEditor();
                }
            });
        } });

        auto btnBounds = menuButton.getBounds();
        auto localPos = getTopLevelComponent()->getScreenPosition();
        otoha::ds::showMenuPopup (this, items,
            juce::Point<int> (btnBounds.getX(), btnBounds.getBottom()) + localPos - getScreenPosition());
    };
    addAndMakeVisible (menuButton);

    // --- Action strip ---
    addAndMakeVisible (actionStrip);
    addAndMakeVisible (undoButton);
    addAndMakeVisible (redoButton);
    addAndMakeVisible (cutButton);
    addAndMakeVisible (copyButton);
    addAndMakeVisible (pasteButton);
    addAndMakeVisible (deleteButton);
    addAndMakeVisible (trimButton);
    addAndMakeVisible (playButton);
    addAndMakeVisible (zoomInButton);
    addAndMakeVisible (zoomOutButton);
    addAndMakeVisible (zoomFitButton);

    undoButton.onClick         = [this] { undo(); };
    redoButton.onClick         = [this] { redo(); };
    cutButton.onClick          = [this] { cutSelected(); };
    copyButton.onClick         = [this] { copySelected(); };
    pasteButton.onClick        = [this] { pasteAtCursor(); };
    deleteButton.onClick       = [this] { rippleDeleteSelection(); };
    trimButton.onClick         = [this] { trimSelection(); };
    playButton.onClick         = [this] { playPause(); };
    zoomInButton.onClick       = [this] { wave->setZoom (wave->getZoomSafe() * 0.5); };
    zoomOutButton.onClick      = [this] { wave->setZoom (wave->getZoomSafe() * 2.0); };
    zoomFitButton.onClick      = [this] { wave->fitAll(); };

    otoha::theme::label (undoButton, "Undo last edit", "Reverse the last editing operation");
    otoha::theme::label (redoButton, "Redo", "Re-apply the last undone operation");
    otoha::theme::label (cutButton, "Cut selection", "Copy and remove selected audio");
    otoha::theme::label (copyButton, "Copy selection", "Copy selected audio to clipboard");
    otoha::theme::label (pasteButton, "Paste", "Insert clipboard audio at cursor");
    otoha::theme::label (deleteButton, "Delete selection",
                         "Remove selected audio and close the gap");
    otoha::theme::label (trimButton, "Keep selection",
                         "Remove everything except the selected audio");
    otoha::theme::label (playButton, "Play or pause");
    otoha::theme::label (zoomInButton, "Zoom in");
    otoha::theme::label (zoomOutButton, "Zoom out");
    otoha::theme::label (zoomFitButton, "Fit whole recording", "Zoom to show the entire timeline");

    // --- Timeline card ---
    addAndMakeVisible (timelineCard);
    wave = std::make_unique<WaveformDisplay> (*this);
    addAndMakeVisible (*wave);

    // --- Bottom info row ---
    timeLabel.setFont (otoha::theme::font (otoha::theme::TextSize::bodySmall));
    timeLabel.setJustificationType (juce::Justification::centredLeft);
    timeLabel.setColour (juce::Label::textColourId, otoha::theme::colors::textMuted());
    addAndMakeVisible (timeLabel);

    // --- Sound panel ---
    addAndMakeVisible (soundPanelCard);
    soundSectionLabel.setText ("Sound", juce::dontSendNotification);
    soundSectionLabel.setFont (otoha::theme::font (otoha::theme::TextSize::heading));
    soundSectionLabel.setColour (juce::Label::textColourId, otoha::theme::colors::textPrimary());
    soundSectionLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (soundSectionLabel);

    soundSectionDesc.setText ("Audio enhancement", juce::dontSendNotification);
    soundSectionDesc.setFont (otoha::theme::font (otoha::theme::TextSize::caption));
    soundSectionDesc.setColour (juce::Label::textColourId, otoha::theme::colors::textMuted());
    soundSectionDesc.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (soundSectionDesc);

    addAndMakeVisible (enhanceToggleButton);
    enhanceToggleButton.onClick = [this]
    {
        if (! isOpen())
        {
            enhanceToggleButton.setToggleState (false, juce::dontSendNotification);
            return;
        }

        if (! enhancePanelBuilt)
        {
            enhancePanel = std::make_unique<EnhancePanel> (doc->processing, [this] { dspChanged(); });
            addChildComponent (*enhancePanel);
            enhancePanelBuilt = true;
        }

        const bool show = ! enhanceToggleButton.getToggleState();
        if (show && ! doc->processing.enabled)
        {
            doc->processing = otoha::presetToState (otoha::DspPreset::natural);
            doc->autosaveState();
            enhancePanel->resized();
        }
        enhancePanel->setVisible (show);
        enhancePanel->resized();
        enhanceToggleButton.setButtonText (show ? "Enhanced" : "Enhance");
        dspChanged();
    };

    // --- Save / Export ---
    addAndMakeVisible (saveButton);
    addAndMakeVisible (exportButton);
    saveButton.onClick   = [this] { saveChanges(); };
    exportButton.onClick = [this] { exportAs(); };

    // Feedback label
    feedbackLabel.setFont (otoha::theme::font (otoha::theme::TextSize::body));
    feedbackLabel.setColour (juce::Label::textColourId, otoha::theme::colors::accent());
    feedbackLabel.setJustificationType (juce::Justification::centred);
    feedbackLabel.setInterceptsMouseClicks (false, false);
    addChildComponent (feedbackLabel);

    // M34: toast overlay (must be added last so it covers everything)
    addAndMakeVisible (toastHost);

    refreshButtonsAndTitle();
    startTimerHz (30);
}

bool EditorView::openItem (const otoha::MediaItem& mediaItem, juce::String& errorOut)
{
    stopPlayback();

    auto newDoc = std::make_shared<otoha::AudioDocument>();
    if (! newDoc->loadFromFile (mediaItem.file, errorOut))
        return false;

    newDoc->restoreFromSidecar();

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

// =============================================================================
// Layout
// =============================================================================
void EditorView::paint (juce::Graphics& g)
{
    g.fillAll (otoha::theme::colors::background());
}

void EditorView::resized()
{
    auto bounds = getLocalBounds();

    // 1. Header bar (top)
    layoutHeader (bounds.removeFromTop (50));

    // 2. Action strip
    layoutActionStrip (bounds.removeFromTop (44));

    // 3. Bottom: info row + save/export
    auto bottomArea = bounds.removeFromBottom (42);
    {
        auto row = bottomArea.reduced (16, 4);
        timeLabel.setBounds (row.removeFromLeft (row.getWidth() / 2));
        auto rightButtons = row;
        exportButton.setBounds (rightButtons.removeFromRight (80).reduced (2, 4));
        saveButton.setBounds (rightButtons.removeFromRight (72).reduced (2, 4));
    }

    // 4. Main content: timeline (left) | sound panel (right)
    layoutMainContent (bounds);
}

void EditorView::layoutHeader (juce::Rectangle<int> area)
{
    headerCard.setBounds (area);
    auto row = area.reduced (12, 6);

    backButton.setBounds (row.removeFromLeft (40).withHeight (28));
    row.removeFromLeft (6);
    menuButton.setBounds (row.removeFromRight (36).withHeight (28));
    row.removeFromRight (6);
    titleLabel.setBounds (row);
}

void EditorView::layoutActionStrip (juce::Rectangle<int> area)
{
    actionStrip.setBounds (area);
    auto row = area.reduced (12, 6);

    const int btnH = 28;
    const int gap = 4;

    // Primary: Play
    playButton.setBounds (row.removeFromLeft (70).withHeight (btnH));
    row.removeFromLeft (gap * 2);

    // Editing actions
    undoButton.setBounds (row.removeFromLeft (52).withHeight (btnH));
    row.removeFromLeft (gap);
    redoButton.setBounds (row.removeFromLeft (52).withHeight (btnH));
    row.removeFromLeft (gap * 2);
    cutButton.setBounds (row.removeFromLeft (48).withHeight (btnH));
    row.removeFromLeft (gap);
    copyButton.setBounds (row.removeFromLeft (56).withHeight (btnH));
    row.removeFromLeft (gap);
    pasteButton.setBounds (row.removeFromLeft (56).withHeight (btnH));
    row.removeFromLeft (gap);
    deleteButton.setBounds (row.removeFromLeft (58).withHeight (btnH));
    row.removeFromLeft (gap);
    trimButton.setBounds (row.removeFromLeft (100).withHeight (btnH));

    // Right side: zoom
    zoomFitButton.setBounds (row.removeFromRight (40).withHeight (btnH));
    row.removeFromRight (gap);
    zoomOutButton.setBounds (row.removeFromRight (28).withHeight (btnH));
    row.removeFromRight (gap);
    zoomInButton.setBounds (row.removeFromRight (28).withHeight (btnH));
}

void EditorView::layoutMainContent (juce::Rectangle<int> area)
{
    const int soundPanelWidth = juce::jmin (320, area.getWidth() / 3);
    const int gap = 8;

    if (area.getWidth() > 500 && isOpen())
    {
        // Split: timeline | sound panel
        auto soundArea = area.removeFromRight (soundPanelWidth);
        area.removeFromRight (gap);

        soundPanelCard.setBounds (soundArea.reduced (2));
        auto sp = soundArea.reduced (12, 10);
        soundSectionLabel.setBounds (sp.removeFromTop (22));
        sp.removeFromTop (2);
        soundSectionDesc.setBounds (sp.removeFromTop (16));
        sp.removeFromTop (8);
        enhanceToggleButton.setBounds (sp.removeFromTop (32).reduced (0, 2));
        sp.removeFromTop (8);

        if (enhancePanel != nullptr && enhancePanel->isVisible())
            enhancePanel->setBounds (sp);
    }
    else
    {
        // Compact: full-width timeline, sound panel below
        auto soundArea = area.removeFromBottom (juce::jmax (200, area.getHeight() / 3));
        area.removeFromBottom (gap);

        soundPanelCard.setBounds (soundArea.reduced (2));
        auto sp = soundArea.reduced (12, 10);
        soundSectionLabel.setBounds (sp.removeFromTop (22));
        sp.removeFromTop (2);
        soundSectionDesc.setBounds (sp.removeFromTop (16));
        sp.removeFromTop (8);
        enhanceToggleButton.setBounds (sp.removeFromTop (32).reduced (0, 2));
        sp.removeFromTop (8);

        if (enhancePanel != nullptr && enhancePanel->isVisible())
            enhancePanel->setBounds (sp);
    }

    timelineCard.setBounds (area.reduced (2));
    wave->setBounds (area.reduced (8, 4));

    // Feedback floats over the waveform
    feedbackLabel.setBounds (wave->getBounds().removeFromTop (24)
                                 .withSizeKeepingCentre (juce::jmin (280, wave->getWidth()), 22));

    toastHost.setBounds (getLocalBounds());
}

// =============================================================================
// Keyboard
// =============================================================================
bool EditorView::keyPressed (const juce::KeyPress& key)
{
    if (! isOpen()) return false;

    const auto cmd = key.getModifiers().isCommandDown();

    if (key.isKeyCode (juce::KeyPress::spaceKey))                        { playPause(); return true; }
    if (key.isKeyCode (juce::KeyPress::deleteKey))                       { rippleDeleteSelection(); return true; }
    if (cmd && key.isKeyCode ('Z') && key.getModifiers().isShiftDown())  { redo(); return true; }
    if (cmd && key.isKeyCode ('Z'))                                      { undo(); return true; }
    if (cmd && key.isKeyCode ('Y'))                                      { redo(); return true; }
    if (cmd && key.isKeyCode ('X'))                                      { cutSelected(); return true; }
    if (cmd && key.isKeyCode ('C'))                                      { copySelected(); return true; }
    if (cmd && key.isKeyCode ('V'))                                      { pasteAtCursor(); return true; }
    if (cmd && key.isKeyCode ('A'))                                      { doc->setSelection (0, doc->totalSamples()); selectionChanged(); return true; }
    if (cmd && key.isKeyCode ('S'))                                      { saveChanges(); return true; }

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
// Playback hooks
// =============================================================================
int64_t EditorView::playbackPositionSamples() const
{
    if (doc == nullptr || doc->getSampleRate() <= 0.0) return 0;
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
// DSP
// =============================================================================
otoha::ProcessingState EditorView::effectiveProcessing() const
{
    auto state = doc != nullptr ? doc->processing : otoha::ProcessingState {};
    if (enhancePanel != nullptr && ! enhancePanel->previewingEnhanced())
        state.enabled = false;
    return state;
}

void EditorView::dspChanged()
{
    if (activePreview != nullptr)
        activePreview->setParameters (effectiveProcessing());
    doc->autosaveState();
    refreshButtonsAndTitle();
}

void EditorView::ensurePlaybackSource()
{
    if (doc == nullptr || loadedSourceVersion == doc->getVersion()) return;
    stopPlayback();

    auto preview = std::make_unique<DspPreviewSource> (
        std::make_unique<TimelineSource> (doc), doc->getSampleRate());
    preview->setUpstreamMono (doc->getNumChannels() == 1);
    preview->setParameters (effectiveProcessing());

    activePreview = preview.get();
    player.loadCustomSource (std::move (preview), doc->getSampleRate());
    loadedSourceVersion = doc->getVersion();
}

// =============================================================================
// Transport
// =============================================================================
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
    const double seconds = (double) doc->getSelection().length() / doc->getSampleRate();
    doc->cutSelectedRange (clipboard);
    afterEditRebuild();
    showFeedback ("Cut " + otoha::formatDuration (seconds));
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
        toastHost.show (otoha::ds::ToastHost::Kind::error, error);
    else
    {
        afterEditRebuild();
        const double seconds = (double) clipboard.data.getNumSamples() / doc->getSampleRate();
        showFeedback ("Pasted " + otoha::formatDuration (seconds));
    }
}

void EditorView::rippleDeleteSelection()
{
    if (! isOpen() || doc->getSelection().isEmpty()) return;
    stopPlayback();
    const auto sel = doc->getSelection();
    const double seconds = (double) sel.length() / doc->getSampleRate();
    doc->rippleDelete (sel.start, sel.length());
    afterEditRebuild();
    showFeedback ("Deleted " + otoha::formatDuration (seconds));
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
    loadedSourceVersion = 0xFFFFFFFF;
    activePreview = nullptr;
    doc->autosaveState();
    wave->rebuildPeaks();
    wave->fitAll();
    refreshButtonsAndTitle();
}

// =============================================================================
// Save / export / close
// =============================================================================
void EditorView::saveChanges()
{
    if (! isOpen() || ! doc->isModified()) return;
    stopPlayback();

    const otoha::TimelineRenderer renderer (doc);
    const auto destName = item.file.getFileNameWithoutExtension() + " (edited).wav";
    const auto destination = library.getBaseDirectory()
                                 .getChildFile ("Library").getChildFile ("Audio")
                                 .getChildFile (destName);

    juce::String error;
    if (! renderer.renderToWav (destination, error, &doc->processing))
    {
        toastHost.show (otoha::ds::ToastHost::Kind::error, error);
        return;
    }

    library.registerAudioFile (destination);
    doc->clearSavedState();
    doc->markUnmodified();
    item.file = destination;
    item.displayName = destination.getFileNameWithoutExtension();

    refreshButtonsAndTitle();
    toastHost.show (otoha::ds::ToastHost::Kind::success,
                     "Saved edited copy: " + destName);
}

void EditorView::exportAs()
{
    if (! isOpen()) return;

    otoha::FfmpegLocator locator;
    otoha::FfmpegInfo info;
    const bool ffmpegAvailable = locator.locate (info) == otoha::EncoderStatus::available;

    const auto choice = runExportOptionsDialog (this, exportStore, 1, ffmpegAvailable);
    if (! choice.confirmed) return;

    const auto startDir = exportStore.getLastDirectory();
    chooser = std::make_unique<juce::FileChooser> ("Choose the output folder", startDir);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [this, choice] (const juce::FileChooser& fc)
                          {
                              const auto dir = fc.getResult();
                              if (dir == juce::File{}) return;

                              exportStore.remember (choice.format, choice.quality, dir);

                              otoha::ExportRequest request;
                              request.sourceFile = item.file;
                              request.openDocument = doc;
                              request.baseName = item.displayName;
                              request.destinationDirectory = dir;
                              request.format = choice.format;
                              request.quality = choice.quality;

                              exportManager.submit (request);
                              showExportProgressWindow (this, exportManager);
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
        true);

    return true;
}

void EditorView::closeEditor()
{
    stopPlayback();
    player.unload();
    activePreview = nullptr;
    editorActive = false;
    doc = nullptr;
    if (enhancePanel) { enhancePanel->setVisible (false); }
    enhanceToggleButton.setToggleState (false, juce::dontSendNotification);
    enhanceToggleButton.setButtonText ("Enhance");
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
    if (! isOpen()) return;

    if (feedbackTicksLeft > 0 && --feedbackTicksLeft == 0)
        feedbackLabel.setVisible (false);

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

void EditorView::showFeedback (const juce::String& message)
{
    feedbackLabel.setText (message, juce::dontSendNotification);
    feedbackLabel.setVisible (true);
    feedbackTicksLeft = 60;
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
    deleteButton.setEnabled (hasSel);
    trimButton.setEnabled (hasSel);
    pasteButton.setEnabled (open && ! clipboard.isEmpty());
    undoButton.setEnabled (open && doc->canUndo());
    redoButton.setEnabled (open && doc->canRedo());
    playButton.setEnabled (open);
    exportButton.setEnabled (open);
    saveButton.setEnabled (open && doc->isModified());
}
