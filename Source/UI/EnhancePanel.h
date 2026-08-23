#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "../Dsp/Presets.h"
#include "../Dsp/ProcessingState.h"

/*
    EnhancePanel — the whole M5 user experience in one small component.

        [ ON ]  Preset [Natural v]   (Original | Enhanced)   Reset

        Bass / Mids / Treble sliders
        Compression amount
        Noise reduction: Off / Gentle / Strong
        Limiter: ON
        [ Advanced ... ]

    The panel only mutates a ProcessingState and calls onChange(); it contains
    zero DSP code (UI -> ProcessingState -> DspChain, never the reverse).
*/
class EnhancePanel : public juce::Component,
                     private juce::Timer
{
public:
    EnhancePanel (otoha::ProcessingState& stateRef, std::function<void()> onChanged);

    void paint (juce::Graphics&) override;
    void resized() override;

    /** True when the listener is on the "Enhanced" side of the A/B switch. */
    bool previewingEnhanced() const  { return abEnhanced.getToggleState(); }

private:
    void timerCallback() override;   // preset-modified marker refresh

    void applyPreset (otoha::DspPreset p);
    void changed();
    juce::String presetLabel() const;

    otoha::ProcessingState& state;
    std::function<void()> onChange;

    juce::ToggleButton enableToggle { "Enhance" };
    juce::ComboBox presetCombo;
    juce::ToggleButton abOriginal { "Original" }, abEnhanced { "Enhanced" };
    juce::TextButton resetButton { "Reset" };

    // Basic controls
    juce::Slider bassSlider, midSlider, trebleSlider, compressionSlider;
    juce::ComboBox noiseCombo;
    juce::ToggleButton limiterToggle { "Limiter" };

    // Advanced section
    juce::ToggleButton advancedToggle { "Advanced" };
    juce::Component advancedArea;
    juce::Slider eqLowMid, eqHighMid, compThreshold, compRatio, compAttack,
                 compRelease, compMakeup, limCeiling, nrStrength;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnhancePanel)
};
