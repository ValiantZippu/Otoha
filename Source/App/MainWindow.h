#pragma once

#include "../Audio/Player.h"
#include "../Audio/Recorder.h"
#include "../Library/LibraryService.h"
#include "../UI/AppShell.h"

/*
    MainWindow — a plain resizable DocumentWindow hosting the app shell
    (Library / Record navigation).
*/
class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow (juce::String name, juce::AudioDeviceManager& deviceManager,
                Recorder& recorder, Player& player, LibraryService& library);

    void closeButtonPressed() override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
};
