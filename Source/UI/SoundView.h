#pragma once

#include "../Sound/SoundEngine.h"
#include "../Sound/platform/UnsupportedAudioBackend.h"
#include "../Sound/platform/WindowsAudioBackend.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <string>
#include <vector>

/*
    SoundView — Otoha Sound's deliberately simple control panel:

        ON/OFF · Enhance · Bass / Clarity / Space · Profile · Output · meter

    The DSP lives in SoundEngine + the Otoha DSP Core; this class only sends
    parameter updates and draws meters. On non-Windows platforms the backend
    reports "not implemented" and the view says exactly that — no fake UI.
*/
class SoundView : public juce::Component,
                  private juce::Timer
{
public:
    explicit SoundView (juce::File profileStoreDirectory);
    ~SoundView() override;

    SoundView() = delete;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    void buildUi();
    void refreshOutputDevices();
    void applyProfileToUi (const otoha::platform::AudioProfile& p);
    void persistActiveProfile();
    void handleDeviceChange();

    // Backend: Windows gets the real thing; everyone else an honest stub.
    #if JUCE_WINDOWS
    otoha::platform::WindowsAudioBackend backend;
    #else
    otoha::platform::UnsupportedAudioBackend backend { juce::SystemStats::getOperatingSystemName().toStdString() };
    #endif

    SoundEngine engine;
    otoha::platform::ProfileManager profiles;
    juce::File profileStoreDirectory;
    std::string activeProfileId = "default";
    bool autoSwitchProfiles = false;

    juce::ToggleButton powerToggle { "ON" };
    juce::Slider enhanceSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxBelow };
    juce::Slider bassSlider    { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::Slider claritySlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::Slider spaceSlider   { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };

    juce::Label presetLabel { {}, "Profile" }, outputLabel { {}, "Output" },
                statusLabel, latencyLabel, meterLabel { {}, "Output" };

    juce::ComboBox presetCombo, outputCombo;
    juce::TextButton advancedButton { "Advanced" };

    std::vector<std::string> outputIds;   // parallel to outputCombo items (item 1 = default)
    juce::Rectangle<int> meterRect;
    float meterLevel = 0.0f;   // smoothed for painting

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundView)
};
