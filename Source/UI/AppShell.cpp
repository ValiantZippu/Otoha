#include "AppShell.h"

AppShell::AppShell (juce::AudioDeviceManager& dm, Recorder& rec, Player& pl, LibraryService& lib)
    : library (lib), recorder (rec), player (pl)
{
    recordView = std::make_unique<RecordView> (dm, recorder, player, library,
                                               [this] { openCurrentRecordingInEditor(); });
    libraryView = std::make_unique<LibraryView> (library, player,
                                                 [this] { showRecording(); },
                                                 [this] (const otoha::MediaItem& i) { return openInEditor (i); },
                                                 [this] (const juce::File& f) { return editorView->isEditingFile (f); });
    editorView  = std::make_unique<EditorView> (player, library, [this] { showLibrary(); });

    addChildComponent (*recordView);
    addChildComponent (*libraryView);
    addChildComponent (*editorView);

    for (auto* b : { &libraryButton, &recordButton, &cameraButton, &settingsButton })
        addAndMakeVisible (*b);

    libraryButton.setClickingTogglesState (true);
    recordButton.setClickingTogglesState (true);
    libraryButton.setRadioGroupId (10);
    recordButton.setRadioGroupId (10);

    cameraButton.setEnabled (false);     // Milestone 7 — video
    settingsButton.setEnabled (false);
    cameraButton.setTooltip ("Coming in the video milestone");
    settingsButton.setTooltip ("Coming soon");

    libraryButton.onClick = [this] { showLibrary(); };
    recordButton.onClick  = [this] { showRecording(); };

    // Record is the home screen; the editor opens from the Library or Record's
    // Edit button.
    recordButton.setToggleState (true, juce::dontSendNotification);
    recordView->setVisible (true);
}

void AppShell::resized()
{
    auto bounds = getLocalBounds();

    auto nav = bounds.removeFromTop (44).reduced (12, 6);
    libraryButton.setBounds  (nav.removeFromLeft (90).withHeight (30));
    nav.removeFromLeft (8);
    recordButton.setBounds   (nav.removeFromLeft (90).withHeight (30));
    nav.removeFromLeft (16);
    cameraButton.setBounds   (nav.removeFromLeft (88).withHeight (30));
    nav.removeFromLeft (8);
    settingsButton.setBounds (nav.removeFromLeft (88).withHeight (30));

    recordView->setBounds  (bounds);
    libraryView->setBounds (bounds);
    editorView->setBounds  (bounds);
}

void AppShell::showLibrary()
{
    libraryButton.setToggleState (true, juce::dontSendNotification);
    recordView->setVisible (false);
    editorView->setVisible (false);
    libraryView->setVisible (true);
    libraryView->refreshItemsForDisplay();
    libraryView->grabDefaultFocus();
}

void AppShell::showRecording()
{
    recordButton.setToggleState (true, juce::dontSendNotification);
    libraryView->setVisible (false);
    editorView->setVisible (false);
    recordView->setVisible (true);
    recordView->grabKeyboardFocus();
}

void AppShell::showEditor()
{
    libraryView->setVisible (false);
    recordView->setVisible (false);
    editorView->setVisible (true);
    editorView->grabKeyboardFocus();
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
