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
#include "Components/DsNavigation.h"

/*
    AppShell — Otoha application shell (M19):

    Floating sidebar navigation with vector icons, wrapping the existing
    page/view architecture.  The sidebar is the sole navigation mechanism;
    pages remain as before.
*/
class AppShell : public juce::Component,
                 private juce::ChangeListener
{
public:
    AppShell (juce::AudioDeviceManager& deviceManager,
              Recorder& recorder, Player& player, LibraryService& library,
              otoha::AppSettings* appSettings = nullptr);

    ~AppShell() override;

    void resized() override;
    void paint (juce::Graphics& g) override;

    /** Dev-only: Ctrl+Shift+D toggles the M18 design-system gallery. */
    bool keyPressed (const juce::KeyPress& key) override;

    /** Opens a library item in the editor view. */
    bool openInEditor (const otoha::MediaItem& item);
    void openCurrentRecordingInEditor();

    otoha::ExportSettingsStore exportStore;

private:
    // Navigation ids match sidebar item ids
    static constexpr int idStudio  = 1;
    static constexpr int idLibrary = 2;
    static constexpr int idRecord  = 3;
    static constexpr int idSound   = 4;
    static constexpr int idSettings = 5;

    void showHome();
    void showLibrary();
    void showRecording();
    void showEditor();
    void showSound();
    void showPage (int pageId);
    void navigateTo (int id);

    // ChangeListener for theme updates
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    LibraryService& library;
    Recorder& recorder;
    Player& player;
    otoha::AppSettings* settings = nullptr;
    otoha::ExportManager exportManager;

    // M19: floating sidebar replaces the old button row
    otoha::ds::Sidebar sidebar;
    juce::Component contentArea;  // backdrop behind the sidebar
    int currentPageId = idStudio;

    std::unique_ptr<HomeView> homeView;
    std::unique_ptr<RecordView> recordView;
    std::unique_ptr<LibraryView> libraryView;
    std::unique_ptr<EditorView> editorView;
    std::unique_ptr<SoundView> soundView;
    std::unique_ptr<otoha::ComponentsGallery> gallery;   // dev-only (Ctrl+Shift+D)
    std::unique_ptr<OnboardingView> onboarding;   // first launch only (#3)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AppShell)
};
