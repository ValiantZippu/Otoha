#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdint>

#include "../Audio/Player.h"
#include "../Dsp/DspPreviewSource.h"
#include "../Editor/AudioDocument.h"
#include "../Export/ExportManager.h"
#include "../Export/ExportPresets.h"
#include "../Library/LibraryModel.h"
#include "../Library/LibraryService.h"
#include "EnhancePanel.h"
#include "Components/DsButton.h"
#include "Components/DsControls.h"
#include "Components/DsDialog.h"
#include "Components/DsMenu.h"
#include "Components/DsSurfaces.h"
#include "Components/DsToast.h"

/*
    EditorView — M32 Kaiteyo-aligned editor.

    ┌───────────────────────────────────────────────────────┐
    │ ◀ Back    Recording Name              Save  Export   │  ← header bar
    ├───────────────────────────────────────────────────────┤
    │ [Undo] [Redo] [Cut] [Copy] [Delete] [Keep] ▶ Zoom  │  ← action strip
    ├───────────────────────────────────────────────────────┤
    │                                                       │
    │              Timeline / Waveform                      │  ← primary
    │                                                       │
    ├──────────────────────────────┬────────────────────────┤
    │       00:00 → 02:31         │     Sound / Enhance    │  ← info + DSP
    └──────────────────────────────┴────────────────────────┘

    Preserves: source → timeline → DSP chain → renderer → export.
    Does NOT introduce multitrack, plugin rack, or DAW complexity.
*/
class TimelineSource;

class EditorView : public juce::Component,
                   private juce::Timer
{
public:
    EditorView (Player& player, LibraryService& library, std::function<void()> backToLibrary,
                otoha::ExportManager& exportManager, otoha::ExportSettingsStore& exportStore);

    /** Opens a media item in the editor (decodes it, restores autosave if present). */
    bool openItem (const otoha::MediaItem& item, juce::String& errorOut);

    bool isOpen() const;
    bool isEditingFile (const juce::File& file) const;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;
    ~EditorView() override;

private:
    void timerCallback() override;

    // Hooks used by WaveformDisplay
    int64_t playbackPositionSamples() const;
    void onWaveformClick (juce::int64 sample);
    void selectionChanged();

    // Edit commands
    void cutSelected();
    void copySelected();
    void pasteAtCursor();
    void rippleDeleteSelection();
    void trimSelection();
    void undo();
    void redo();
    void afterEditRebuild();

    // Transport
    void playPause();
    void stopPlayback();
    void seek (double seconds);
    void ensurePlaybackSource();

    // Save / export / close
    void saveChanges();
    void exportAs();
    bool confirmDiscardOrSave (const std::function<void(bool /*proceed*/)>& onDecided);
    void closeEditor();

    void refreshButtonsAndTitle();
    void layoutHeader (juce::Rectangle<int> area);
    void layoutActionStrip (juce::Rectangle<int> area);
    void layoutMainContent (juce::Rectangle<int> area);

    otoha::ProcessingState effectiveProcessing() const;
    void dspChanged();

    // --- Nested waveform display (unchanged from M22) ---
    class WaveformDisplay;
    std::unique_ptr<WaveformDisplay> wave;

    // --- Core references ---
    Player& player;
    LibraryService& library;
    std::function<void()> backToLibrary;
    otoha::ExportManager& exportManager;
    otoha::ExportSettingsStore& exportStore;

    std::shared_ptr<otoha::AudioDocument> doc;
    otoha::AudioClipboard clipboard;
    otoha::MediaItem item;
    bool playingSelection = false;
    bool editorActive = false;
    juce::uint32 loadedSourceVersion = 0xFFFFFFFF;
    DspPreviewSource* activePreview = nullptr;
    bool enhancePanelBuilt = false;

    // --- Header bar ---
    otoha::ds::Card headerCard { "Editor header", false };
    otoha::ds::Button backButton { "Back", otoha::ds::ButtonVariant::tertiary };
    juce::Label titleLabel;
    otoha::ds::Button menuButton { "⋯", otoha::ds::ButtonVariant::tertiary };

    // --- Action strip (editing actions) ---
    otoha::ds::Card actionStrip { "Editing actions", false };
    otoha::ds::Button undoButton { "Undo", otoha::ds::ButtonVariant::secondary };
    otoha::ds::Button redoButton { "Redo", otoha::ds::ButtonVariant::secondary };
    otoha::ds::Button cutButton { "Cut", otoha::ds::ButtonVariant::secondary };
    otoha::ds::Button copyButton { "Copy", otoha::ds::ButtonVariant::secondary };
    otoha::ds::Button pasteButton { "Paste", otoha::ds::ButtonVariant::secondary };
    otoha::ds::Button deleteButton { "Delete", otoha::ds::ButtonVariant::secondary };
    otoha::ds::Button trimButton { "Keep Selection", otoha::ds::ButtonVariant::secondary };
    otoha::ds::Button playButton { "Play", otoha::ds::ButtonVariant::primary };

    // Zoom controls (compact, right side of strip)
    otoha::ds::Button zoomInButton { "+", otoha::ds::ButtonVariant::tertiary };
    otoha::ds::Button zoomOutButton { "-", otoha::ds::ButtonVariant::tertiary };
    otoha::ds::Button zoomFitButton { "Fit", otoha::ds::ButtonVariant::tertiary };

    // --- Main content: timeline + sound panel ---
    otoha::ds::Card timelineCard { "Audio timeline", false };

    // Bottom info row
    juce::Label timeLabel;

    // Sound panel (right side)
    otoha::ds::Card soundPanelCard { "Sound", false };
    juce::Label soundSectionLabel;
    juce::Label soundSectionDesc;
    otoha::ds::Button enhanceToggleButton { "Enhance", otoha::ds::ButtonVariant::primary };
    std::unique_ptr<EnhancePanel> enhancePanel;

    // Action buttons in the bottom strip
    otoha::ds::Button saveButton { "Save", otoha::ds::ButtonVariant::primary };
    otoha::ds::Button exportButton { "Export", otoha::ds::ButtonVariant::secondary };

    // Feedback
    juce::Label feedbackLabel;
    int feedbackTicksLeft = 0;
    void showFeedback (const juce::String& message);

    std::unique_ptr<juce::FileChooser> chooser;

    // --- M34: canonical toast system ---
    otoha::ds::ToastHost toastHost;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditorView)
};
