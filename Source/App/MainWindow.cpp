#include "MainWindow.h"

#include "../UI/OtohaTheme.h"

MainWindow::MainWindow (juce::String name, juce::AudioDeviceManager& deviceManager,
                        Recorder& recorder, Player& player, LibraryService& library,
                        otoha::AppSettings* appSettings)
    : DocumentWindow (name,
                      otoha::theme::colors::background(),
                      DocumentWindow::allButtons)
{
    setUsingNativeTitleBar (true);
    setContentOwned (new AppShell (deviceManager, recorder, player, library, appSettings), true);
    setResizable (true, false);
    centreWithSize (1000, 680);
    setVisible (true);
}

void MainWindow::closeButtonPressed()
{
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}
