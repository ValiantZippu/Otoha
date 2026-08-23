#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/*
    OnboardingView — the entire first-launch experience (#3/#4).

    One screen, three decisions, done:

        OTOHA
        Make your audio sound better.
        [ Get Started ]
            Output   [ System Default v ]
            Enhance  [ ON ]
            Preset   [ Natural v ]
        [ Done ]

    Deliberately NOT a multi-page tutorial. When Done is pressed the view
    reports the choices and the shell persists firstLaunchComplete.
*/
class OnboardingView : public juce::Component
{
public:
    /** Called once when the user finishes; view is still alive during it. */
    std::function<void (bool enhanceOn, juce::String presetName)> onFinished;

    OnboardingView();

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void revealSetup();

    juce::Label title       { {}, "OTOHA" };
    juce::Label tagline     { {}, "Make your audio sound better." };
    juce::TextButton getStartedButton { "Get Started" };

    juce::Label outputLabel { {}, "Output" }, enhanceLabel { {}, "Enhance" },
                presetLabel { {}, "Preset" }, hintLabel;
    juce::ComboBox outputCombo;
    juce::ToggleButton enhanceToggle { "ON" };
    juce::ComboBox presetCombo;
    juce::TextButton doneButton { "Done" };

    bool setupRevealed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OnboardingView)
};
