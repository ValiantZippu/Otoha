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

/*
    EditorView — select part of a recording → make a small edit → preview → save.

    One timeline, no track headers. Waveform peaks are computed once from the
    decoded document and reused at every zoom level; repaints never touch disk.
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
    bool isEditingFile (const juce::File& file) const;   // delete-safety guard for the Library

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    void timerCallback() override;

    // Hooks used by WaveformDisplay (nested classes may access these)
    int64_t playbackPositionSamples() const;
    void onWaveformClick (int64_t sample);
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
    void playPause();          // selection if any, else whole take
    void stopPlayback();
    void seek (double seconds);
    void ensurePlaybackSource();

    // Save / export / close
    void saveChanges();        // renders an "(edited)" item into the Library
    void exportAs();           // user-chosen destination
    bool confirmDiscardOrSave (const std::function<void(bool /*proceed*/)>& onDecided);
    void closeEditor();

    void refreshButtonsAndTitle();

    /** The state actually audible right now: processing with the A/B switch
        applied ("Original" previews a bypassed chain). Export never uses this. */
    otoha::ProcessingState effectiveProcessing() const;
    void dspChanged();

    class WaveformDisplay;
    std::unique_ptr<WaveformDisplay> wave;

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
    juce::uint32 loadedSourceVersion = 0xFFFFFFFF;   // document version behind the transport
    DspPreviewSource* activePreview = nullptr;       // owned by the Player's unique_ptr
    bool enhancePanelBuilt = false;

    juce::TextButton backButton { "<-" }, menuButton { "..." };
    juce::Label titleLabel;

    juce::TextButton cutButton { "Cut" }, copyButton { "Copy" }, pasteButton { "Paste" },
        rippleDeleteButton { "Ripple Delete" }, trimButton { "Trim" },
        undoButton { "Undo" }, redoButton { "Redo" },
        playButton { "Play" },
        zoomInButton { "+" }, zoomOutButton { "-" }, zoomFitButton { "Fit" },
        enhanceButton { "Enhance" }, exportButton { "Export" }, saveButton { "Save" };

    juce::Label timeLabel;   // cursor / selection readout
    std::unique_ptr<EnhancePanel> enhancePanel;

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditorView)
};
