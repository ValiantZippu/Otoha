#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Audio/Player.h"
#include "../Audio/Recorder.h"
#include "../Export/ExportManager.h"
#include "../Export/ExportPresets.h"
#include "../Library/LibraryService.h"
#include "EditorView.h"
#include "LibraryView.h"
#include "RecordView.h"

/*
    AppShell — desktop navigation:  Library | Record | Camera | Settings

    Milestone 3 implements Library and Record; Camera and Settings are visible
    but disabled placeholders so the architecture is already four-area shaped
    (per the product spec) without implementing future milestones.
*/
class AppShell : public juce::Component
{
public:
    AppShell (juce::AudioDeviceManager& deviceManager,
              Recorder& recorder, Player& player, LibraryService& library);

    void resized() override;

    /** Opens a library item in the editor view. */
    bool openInEditor (const otoha::MediaItem& item);

    otoha::ExportSettingsStore exportStore;

private:
    void showLibrary();
    void showRecording();
    void showEditor();

    LibraryService& library;
    Recorder& recorder;
    Player& player;
    otoha::ExportManager exportManager;

    juce::TextButton libraryButton { "Library" }, recordButton { "Record" },
                     cameraButton { "Camera" }, settingsButton { "Settings" };

    std::unique_ptr<RecordView> recordView;
    std::unique_ptr<LibraryView> libraryView;
    std::unique_ptr<EditorView> editorView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AppShell)
};
