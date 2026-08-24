#pragma once

#include "../App/AppLifecycle.h"
#include "../Core/AppSettings.h"
#include "../Dsp/UserPresets.h"
#include "../Sound/SoundEngine.h"
#include "../Sound/platform/UnsupportedAudioBackend.h"
#include "../Sound/platform/WindowsAudioBackend.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <string>
#include <vector>

/*
    SoundView — Otoha Sound's deliberately simple control panel:

        ON/OFF · Enhance · Bass / Clarity / Space · Preset · Output · meter

    Milestone 10 additions:
      * single AppLifecycle instance drives the status text (#18/#23)
      * actionable errors and recovery messages (#20/#21), never HRESULTs
      * built-in + custom presets in one combo (#13); hidden in Safe Mode
      * Advanced menu: EQ/compressor/limiter panel, diagnostics export (#44),
        About (#35), audio reset (#47)
      * persists its prefs through the shared AppSettings store

    The DSP lives in SoundEngine + the Otoha DSP Core; this class only sends
    parameter updates and draws meters. On non-Windows platforms the backend
    reports "not implemented" and the view says exactly that — no fake UI.
*/
class SoundView : public juce::Component,
                  private juce::Timer
{
public:
    SoundView (juce::File baseDirectory,
               otoha::AppSettings* settings = nullptr,
               bool startInSafeMode = false);
    ~SoundView() override;

    SoundView() = delete;

    void resized() override;
    void paint (juce::Graphics&) override;

    /** Applies the onboarding screen's choices (#3/#4) after first launch. */
    void applyFirstLaunchChoices (bool enhanceOn, const juce::String& presetName);

private:
    void buildUi();
    void refreshOutputDevices();
    void persistActiveProfile();
    void handleDeviceChange();
    void rebuildPresetCombo (const juce::String& selectName = {});
    void showSavePresetDialog();
    void showAdvancedMenu();
    void exportDiagnosticsReport();
    void updateStatusText();
    void applyPresetSelection();

    // Backend: Windows gets the real thing; everyone else an honest stub.
    #if JUCE_WINDOWS
    otoha::platform::WindowsAudioBackend backend;
    #else
    otoha::platform::UnsupportedAudioBackend backend { juce::SystemStats::getOperatingSystemName().toStdString() };
    #endif

    juce::File baseDirectory;                 // profiles + custom presets + settings
    otoha::AppSettings* settings = nullptr;   // owned by the application shell
    otoha::AppLifecycle lifecycle;
    bool safeMode = false;

    SoundEngine engine;
    otoha::platform::ProfileManager profiles;
    otoha::UserPresetStore userPresets;
    std::string activeProfileId = "default";
    bool autoSwitchProfiles = false;

    juce::ToggleButton powerToggle { "ON" };
    juce::Slider enhanceSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxBelow };
    juce::Slider bassSlider    { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::Slider claritySlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::Slider spaceSlider   { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };

    juce::Label presetLabel { {}, "Preset" }, outputLabel { {}, "Output" },
                statusLabel, latencyLabel, meterLabel { {}, "Output" };

    juce::ComboBox presetCombo, outputCombo;
    juce::TextButton advancedButton { "Advanced" };

    std::unique_ptr<juce::FileChooser> chooser;

    std::vector<std::string> outputIds;   // parallel to outputCombo items (item 1 = default)
    std::vector<juce::String> outputNames;
    juce::Rectangle<int> meterRect;
    float meterLevel = 0.0f;   // smoothed for painting

    // Combo bookkeeping: built-ins first, then custom ids, then Save Preset…
    static constexpr int kFirstCustomItemId = 1000;
    static constexpr int kSavePresetItemId  = 9999;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundView)
};
