#include "AppShell.h"

AppShell::AppShell (juce::AudioDeviceManager& dm, Recorder& rec, Player& pl, LibraryService& lib,
                    otoha::AppSettings* appSettings)
    : library (lib), recorder (rec), player (pl), settings (appSettings),
      exportStore (otohaBaseDirectory()),
      exportManager (juce::File{})   // locator discovers FFmpeg itself
{
    recordView = std::make_unique<RecordView> (dm, recorder, player, library,
                                               [this] { openCurrentRecordingInEditor(); });
    libraryView = std::make_unique<LibraryView> (library, player,
                                                 [this] { showRecording(); },
                                                 [this] (const otoha::MediaItem& i) { return openInEditor (i); },
                                                 [this] (const juce::File& f) { return editorView->isEditingFile (f); },
                                                 exportManager, exportStore);
    editorView  = std::make_unique<EditorView> (player, library, [this] { showLibrary(); },
                                                exportManager, exportStore);
    soundView   = std::make_unique<SoundView> (otohaBaseDirectory(), settings,
                                               settings != nullptr && settings->safeModeSession);

    homeView = std::make_unique<HomeView> (library);
    homeView->onRecord      = [this] { showRecording(); };
    homeView->onViewLibrary = [this] { showLibrary(); };
    homeView->onOpenItem    = [this] (const otoha::MediaItem& item) { openInEditor (item); };

    addChildComponent (*homeView);
    addChildComponent (*recordView);
    addChildComponent (*libraryView);
    addChildComponent (*editorView);
    addChildComponent (*soundView);

    for (auto* b : { &studioButton, &libraryButton, &recordButton, &soundButton, &cameraButton, &settingsButton })
        addAndMakeVisible (*b);

    studioButton.setClickingTogglesState (true);
    libraryButton.setClickingTogglesState (true);
    recordButton.setClickingTogglesState (true);
    soundButton.setClickingTogglesState (true);
    studioButton.setRadioGroupId (10);
    libraryButton.setRadioGroupId (10);
    recordButton.setRadioGroupId (10);
    soundButton.setRadioGroupId (10);

    cameraButton.setEnabled (false);     // video milestone
    settingsButton.setEnabled (false);
    cameraButton.setTooltip ("Coming in the video milestone");
    settingsButton.setTooltip ("Coming soon");

    studioButton.onClick  = [this] { showHome(); };
    libraryButton.onClick = [this] { showLibrary(); };
    recordButton.onClick  = [this] { showRecording(); };
    soundButton.onClick   = [this] { showSound(); };

    // Studio Home is the landing screen (M11 #2/#3): Record + Recent + Library.
    // Sound lives in its own top-level section — never mixed into Studio.
    studioButton.setToggleState (true, juce::dontSendNotification);
    showHome();

    // --- first launch (#3): one short screen before anything else -------------
    if (settings != nullptr && ! settings->firstLaunchComplete)
    {
        onboarding = std::make_unique<OnboardingView>();
        onboarding->onFinished = [this] (bool enhanceOn, const juce::String& presetName)
        {
            if (settings != nullptr)
            {
                settings->firstLaunchComplete = true;
                saveAppSettings (*settings, otoha::defaultSettingsDirectory());
            }
            soundView->applyFirstLaunchChoices (enhanceOn, presetName);

            juce::Component::SafePointer<AppShell> safeSelf { this };
            onboarding.reset();          // remove overlay
            if (safeSelf != nullptr) repaint();
        };
        addAndMakeVisible (*onboarding);  // stays on top of all views
    }
}void AppShell::showHome()
{
    if (gallery != nullptr) gallery->setVisible (false);
    studioButton.setToggleState (true, juce::dontSendNotification);
    recordView->setVisible (false);
    libraryView->setVisible (false);
    editorView->setVisible (false);
    soundView->setVisible (false);
    homeView->refreshRecents();
    homeView->setVisible (true);
}

bool AppShell::keyPressed (const juce::KeyPress& key)
{
    // Dev-only design-system gallery (M18): Ctrl+Shift+D.
    if (key.isKeyCode ('d') && key.getModifiers().isCtrlDown()
        && key.getModifiers().isShiftDown())
    {
        if (gallery == nullptr)
        {
            gallery = std::make_unique<otoha::ComponentsGallery>();
            addAndMakeVisible (*gallery);
            gallery->setBounds (getLocalBounds());
        }
        gallery->setVisible (! gallery->isVisible());
        return true;
    }
    if (gallery != nullptr && gallery->isVisible() && key.isKeyCode (juce::KeyPress::escapeKey))
    {
        gallery->setVisible (false);
        return true;
    }
    return Component::keyPressed (key);
}

void AppShell::resized()
{
    auto bounds = getLocalBounds();

    auto nav = bounds.removeFromTop (44).reduced (12, 6);
    studioButton.setBounds (nav.removeFromLeft (80).withHeight (30));
    nav.removeFromLeft (8);
    libraryButton.setBounds (nav.removeFromLeft (90).withHeight (30));
    nav.removeFromLeft (8);
    recordButton.setBounds  (nav.removeFromLeft (90).withHeight (30));
    nav.removeFromLeft (8);
    soundButton.setBounds   (nav.removeFromLeft (80).withHeight (30));
    nav.removeFromLeft (16);
    cameraButton.setBounds   (nav.removeFromLeft (88).withHeight (30));
    nav.removeFromLeft (8);
    settingsButton.setBounds (nav.removeFromLeft (88).withHeight (30));

    homeView->setBounds    (bounds);
    recordView->setBounds  (bounds);
    libraryView->setBounds (bounds);
    editorView->setBounds (bounds);
    soundView->setBounds   (bounds);

    if (gallery != nullptr)
        gallery->setBounds (bounds);

    if (onboarding != nullptr)
        onboarding->setBounds (getLocalBounds());   // full-screen overlay while visible
}

void AppShell::showLibrary()
{
    if (gallery != nullptr) gallery->setVisible (false);
    libraryButton.setToggleState (true, juce::dontSendNotification);
    homeView->setVisible (false);
    recordView->setVisible (false);
    editorView->setVisible (false);
    soundView->setVisible (false);
    libraryView->setVisible (true);
    libraryView->refreshItemsForDisplay();
    libraryView->grabDefaultFocus();
}

void AppShell::showRecording()
{
    if (gallery != nullptr) gallery->setVisible (false);
    recordButton.setToggleState (true, juce::dontSendNotification);
    homeView->setVisible (false);
    libraryView->setVisible (false);
    editorView->setVisible (false);
    soundView->setVisible (false);
    recordView->setVisible (true);
    recordView->grabKeyboardFocus();
}

void AppShell::showEditor()
{
    if (gallery != nullptr) gallery->setVisible (false);
    homeView->setVisible (false);
    libraryView->setVisible (false);
    recordView->setVisible (false);
    soundView->setVisible (false);
    editorView->setVisible (true);
    editorView->grabKeyboardFocus();
}

void AppShell::showSound()
{
    if (gallery != nullptr) gallery->setVisible (false);
    soundButton.setToggleState (true, juce::dontSendNotification);
    homeView->setVisible (false);
    libraryView->setVisible (false);
    recordView->setVisible (false);
    editorView->setVisible (false);
    soundView->setVisible (true);
}

bool AppShell::openInEditor (const otoha::MediaItem& item)
{
    juce::String error;
    if (! editorView->openItem (item, error))
    {
        if (error.isNotEmpty())
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                    "Couldn't open recording", error);
        return false;
    }

    showEditor();
    return true;
}

void AppShell::openCurrentRecordingInEditor()
{
    const auto file = recordView->getCurrentRecordingFile();
    if (file == juce::File{})
        return;

    otoha::MediaItem item;
    item.file = file;
    item.displayName = file.getFileNameWithoutExtension();
    openInEditor (item);
}
