#include "AppShell.h"

AppShell::AppShell (juce::AudioDeviceManager& dm, Recorder& rec, Player& pl, LibraryService& lib,
                    otoha::AppSettings* appSettings)
    : library (lib), recorder (rec), player (pl), settings (appSettings),
      exportStore (otohaBaseDirectory()),
      exportManager (juce::File{})
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
    homeView->onViewSound   = [this] { showSound(); };
    homeView->onOpenItem    = [this] (const otoha::MediaItem& item) { openInEditor (item); };

    addChildComponent (*homeView);
    addChildComponent (*recordView);
    addChildComponent (*libraryView);
    addChildComponent (*editorView);
    addChildComponent (*soundView);

    // --- M19: floating sidebar navigation ----------------------------------------
    addAndMakeVisible (sidebar);
    addAndMakeVisible (contentArea);

    sidebar.addItem (idStudio,  "Studio",  otoha::icons::home(),     "Studio",  false);
    sidebar.addItem (idRecord,  "Record",  otoha::icons::record(),   "Record",  false);
    sidebar.addItem (idLibrary, "Library", otoha::icons::library(),  "Library", false);
    sidebar.addItem (idSound,   "Sound",   otoha::icons::sound(),    "Sound",   false);
    sidebar.addItem (idSettings, "Settings", otoha::icons::settings(), "Settings", true);

    sidebar.onNavigate = [this] (int id) { navigateTo (id); };
    sidebar.setActiveItem (idStudio);

    // Studio Home is the landing screen (M11 #2/#3)
    showHome();

    // --- first launch (#3): one short screen before anything else ---------------
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
            onboarding.reset();
            if (safeSelf != nullptr) repaint();
        };
        addAndMakeVisible (*onboarding);
    }

    // Listen for theme changes (M24 prep)
    otoha::theme::themeChangedBroadcaster().addChangeListener (this);
}

AppShell::~AppShell()
{
    otoha::theme::themeChangedBroadcaster().removeChangeListener (this);
}

// --- Navigation ---------------------------------------------------------------

void AppShell::navigateTo (int id)
{
    sidebar.setActiveItem (id);
    currentPageId = id;

    switch (id)
    {
        case idStudio:   showHome();      break;
        case idLibrary:  showLibrary();   break;
        case idRecord:   showRecording(); break;
        case idSound:    showSound();     break;
        case idSettings: /* placeholder */ break;
    }
}

void AppShell::showPage (int pageId)
{
    currentPageId = pageId;
    sidebar.setActiveItem (pageId);
}

void AppShell::showHome()
{
    if (gallery != nullptr) gallery->setVisible (false);
    showPage (idStudio);
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
        }
        gallery->setVisible (! gallery->isVisible());
        if (gallery->isVisible())
            gallery->setBounds (getLocalBounds());
        return true;
    }
    if (gallery != nullptr && gallery->isVisible() && key.isKeyCode (juce::KeyPress::escapeKey))
    {
        gallery->setVisible (false);
        return true;
    }

    // Navigation shortcuts: 1-5 map to sidebar items
    const int digit = key.getTextCharacter() - '0';
    if (digit >= 1 && digit <= 5 && ! key.getModifiers().isAnyModifierKeyDown())
    {
        static const int pageMap[] = { 0, idStudio, idLibrary, idRecord, idSound, idSettings };
        navigateTo (pageMap[digit]);
        return true;
    }

    return Component::keyPressed (key);
}

// --- Layout -------------------------------------------------------------------

void AppShell::resized()
{
    auto bounds = getLocalBounds();

    // sidebar decides its own width based on available height
    const int sidebarW = otoha::ds::NavItem::fullWidth();
    sidebar.setBounds (bounds.removeFromLeft (sidebarW)
                          .reduced (otoha::theme::Spacing::md));

    // content fills the remainder
    auto content = bounds.reduced (otoha::theme::Spacing::sm);

    homeView->setBounds    (content);
    recordView->setBounds  (content);
    libraryView->setBounds (content);
    editorView->setBounds  (content);
    soundView->setBounds   (content);

    if (gallery != nullptr)
        gallery->setBounds (getLocalBounds());

    if (onboarding != nullptr)
        onboarding->setBounds (getLocalBounds());
}

void AppShell::paint (juce::Graphics& g)
{
    // Content area background
    g.setColour (otoha::theme::colors::background());
    g.fillRect (getLocalBounds());
}

// --- View management (preserved from original) ---------------------------------

void AppShell::showLibrary()
{
    if (gallery != nullptr) gallery->setVisible (false);
    showPage (idLibrary);
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
    showPage (idRecord);
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
    showPage (idSound);
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

void AppShell::changeListenerCallback (juce::ChangeBroadcaster*)
{
    repaint();
}
