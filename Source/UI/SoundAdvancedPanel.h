#pragma once

#include "../Dsp/ProcessingState.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

/*
    SoundAdvancedPanel — everything that must NOT live on the main screen (#7).

        EQ          five band sliders (-12..+12 dB), Otoha's existing 5-band EQ
        Compressor  one Amount slider (0% = off; scales ratio 1:1 -> 6:1)
        Limiter     ON/OFF, with a confirmation before disabling (#10)

    All changes stream out live through `onChange` so the audio path receives
    the same smoothed parameter updates the main window produces.
    Shown modally via showAdvancedPanel(); the dialog manages its own lifetime.
*/
namespace otoha::ui
{
class SoundAdvancedPanel : public juce::Component
{
public:
    std::function<void (const otoha::ProcessingState&)> onChange;

    explicit SoundAdvancedPanel (otoha::ProcessingState initial)
        : state (initial)
    {
        auto styleSlider = [] (juce::Slider& s)
        {
            s.setRange (-12.0, 12.0, 0.5);
            s.setTextValueSuffix (" dB");
            s.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
            s.setColour (juce::Label::textColourId, juce::Colours::white);
        };

        labels[0].setText ("EQ Low", juce::dontSendNotification);
        labels[1].setText ("EQ Low-Mid", juce::dontSendNotification);
        labels[2].setText ("EQ Mid", juce::dontSendNotification);
        labels[3].setText ("EQ High-Mid", juce::dontSendNotification);
        labels[4].setText ("EQ High", juce::dontSendNotification);

        for (int i = 0; i < 5; ++i)
        {
            styleSlider (eqSliders[i]);
            eqSliders[i].setValue (state.eq.gainsDb[i], juce::dontSendNotification);
            eqSliders[i].onValueChange = [this, i]
            { state.eq.gainsDb[i] = (float) eqSliders[i].getValue(); publish(); };
            addAndMakeVisible (labels[i]);
            addAndMakeVisible (eqSliders[i]);
        }

        compressorLabel.setText ("Compressor", juce::dontSendNotification);
        compressorLabel.setColour (juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible (compressorLabel);

        compressorAmount.setRange (0.0, 1.0, 0.01);   // 0 == off (#9)
        compressorAmount.setValue (state.compressor.enabled
                                       ? juce::jlimit (0.0, 1.0,
                                                       (state.compressor.ratio - 1.0f) / 5.0f)
                                       : 0.0,
                                   juce::dontSendNotification);
        compressorAmount.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
        compressorAmount.onValueChange = [this]
        {
            const float amount = (float) compressorAmount.getValue();
            state.compressor.enabled = amount > 0.001f;
            state.compressor.ratio      = 1.0f + 5.0f * amount;
            state.compressor.thresholdDb = -24.0f;
            publish();
        };
        addAndMakeVisible (compressorAmount);

        limiterToggle.setButtonText ("Limiter (output protection)");
        limiterToggle.setToggleState (state.limiter.enabled, juce::dontSendNotification);
        limiterToggle.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
        limiterToggle.onClick = [this]
        {
            const bool enableIt = limiterToggle.getToggleState();
            if (enableIt)
            {
                state.limiter.enabled = true;
                publish();
                return;
            }

            // Disabling protection deserves friction (#10).
            limiterToggle.setToggleState (true, juce::dontSendNotification);
            juce::AlertWindow::showOkCancelBox (
                juce::MessageBoxIconType::WarningIcon, "Disable limiter?",
                "The limiter protects your output from clipping.\n"
                "Disabling it can cause distortion at loud passages.\n\n"
                "Disable anyway?",
                "Disable", "Keep it", this,
                juce::ModalCallbackFunction::create ([this] (int result)
                {
                    if (result != 1) return;
                    limiterToggle.setToggleState (false, juce::dontSendNotification);
                    state.limiter.enabled = false;
                    publish();
                }));
        };
        addAndMakeVisible (limiterToggle);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (16);
        auto row = [&area] (juce::Component& label, juce::Component& control, int h = 26)
        {
            auto r = area.removeFromTop (h);
            label.setBounds (r.removeFromLeft (110));
            control.setBounds (r);
            area.removeFromTop (6);
        };
        for (int i = 0; i < 5; ++i) row (labels[i], eqSliders[i]);
        area.removeFromTop (8);
        row (compressorLabel, compressorAmount);
        area.removeFromTop (4);
        limiterToggle.setBounds (area.removeFromTop (26));
    }

private:
    void publish()
    {
        if (onChange) onChange (state);
    }

    otoha::ProcessingState state;
    juce::Label labels[5];
    juce::Slider eqSliders[5] { { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight },
                                { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight },
                                { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight },
                                { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight },
                                { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight } };
    juce::Label compressorLabel;
    juce::Slider compressorAmount { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::ToggleButton limiterToggle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundAdvancedPanel)
};

inline void showAdvancedPanel (const otoha::ProcessingState& initialState,
                               std::function<void (const otoha::ProcessingState&)> onChange)
{
    auto* panel = new SoundAdvancedPanel (initialState);
    panel->onChange = std::move (onChange);
    panel->setSize (460, 320);

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle            = "Advanced";
    options.dialogBackgroundColour = juce::Colour (0xff0a0a0a);
    options.content.setOwned (panel);
    options.useNativeTitleBar      = true;
    options.resizable              = false;
    options.create()->setVisible (true);
}
} // namespace otoha::ui
