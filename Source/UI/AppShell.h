#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Audio/Player.h"
#include "../Audio/Recorder.h"
#include "../Core/AppSettings.h"
#include "../Export/ExportManager.h"
#include "../Export/ExportPresets.h"
#include "../Library/LibraryService.h"
#include "OnboardingView.h"
#include "ComponentsGallery.h"
#include "EditorView.h"
#include "HomeView.h"
#include "LibraryView.h"
#include "RecordView.h"
#include "SoundView.h"

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
              Recorder& recorder, Player& player, LibraryService& library,
              otoha::AppSettings* appSettings = nullptr);

    void resized() override;

    /** Dev-only: Ctrl+Shift+D toggles the M18 design-system gallery. */
    bool keyPressed (const juce::KeyPress& key) override;

    /** Opens a library item in the editor view. */
    bool openInEditor (const otoha::MediaItem& item);
    void openCurrentRecordingInEditor();

    otoha::ExportSettingsStore exportStore;

private:
    void showHome();
    void showLibrary();
    void showRecording();
    void showEditor();
    void showSound();

    LibraryService& library;
    Recorder& recorder;
    Player& player;
    otoha::AppSettings* settings = nullptr;
    otoha::ExportManager exportManager;

    juce::TextButton studioButton { "Studio" }, libraryButton { "Library" }, recordButton { "Record" },
                     soundButton { "Sound" }, cameraButton { "Camera" }, settingsButton { "Settings" };

    std::unique_ptr<HomeView> homeView;
    std::unique_ptr<RecordView> recordView;
    std::unique_ptr<LibraryView> libraryView;
    std::unique_ptr<EditorView> editorView;
    std::unique_ptr<SoundView> soundView;
    std::unique_ptr<otoha::ComponentsGallery> gallery;   // dev-only (Ctrl+Shift+D)
    std::unique_ptr<OnboardingView> onboarding;   // first launch only (#3)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AppShell)
};
