#include "EnhancePanel.h"

#include <cmath>

namespace
{
juce::Slider* makeVerticalSlider (juce::Slider& s, double min, double max, double def)
{
    s.setRange (min, max, 0.01);
    s.setValue (def, juce::dontSendNotification);
    s.setSliderStyle (juce::Slider::LinearHorizontal);
    s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
    return &s;
}
} // namespace

EnhancePanel::EnhancePanel (otoha::ProcessingState& stateRef, std::function<void()> onChanged)
    : state (stateRef), onChange (std::move (onChanged))
{
    addAndMakeVisible (enableToggle);
    enableToggle.onClick = [this]
    {
        state.enabled = enableToggle.getToggleState();
        changed();
    };

    const auto presets = otoha::allDspPresets();
    for (int i = 0; i < presets.size(); ++i)
        presetCombo.addItem (otoha::presetToString (presets.getReference (i)), i + 1);
    addAndMakeVisible (presetCombo);
    presetCombo.onChange = [this, presets]
    {
        const auto idx = juce::jlimit (0, presets.size() - 1, presetCombo.getSelectedItemIndex());
        applyPreset (presets.getReference (idx));
    };

    for (auto* b : { &abOriginal, &abEnhanced })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (77);
        addAndMakeVisible (*b);
        b->onClick = [this] { changed(); };   // A/B: bypass override only
    }
    abEnhanced.setToggleState (true, juce::dontSendNotification);

    addAndMakeVisible (resetButton);
    resetButton.onClick = [this] { applyPreset (otoha::DspPreset::off); };

    // --- basic controls -------------------------------------------------------
    addAndMakeVisible (*makeVerticalSlider (bassSlider, -12.0, 12.0, 0.0));
    addAndMakeVisible (*makeVerticalSlider (midSlider, -12.0, 12.0, 0.0));
    addAndMakeVisible (*makeVerticalSlider (trebleSlider, -12.0, 12.0, 0.0));
    bassSlider.onValueChange   = [this] { state.eq.gainsDb[0] = (float) bassSlider.getValue(); changed(); };
    midSlider.onValueChange    = [this] { state.eq.gainsDb[2] = (float) midSlider.getValue(); changed(); };
    trebleSlider.onValueChange = [this] { state.eq.gainsDb[4] = (float) trebleSlider.getValue(); changed(); };

    addAndMakeVisible (*makeVerticalSlider (compressionSlider, 0.0, 1.0, 0.0));
    compressionSlider.onValueChange = [this]
    {
        // One "Amount" knob maps onto gentle threshold/ratio pairs.
        const float amount = (float) compressionSlider.getValue();
        state.compressor.enabled = amount > 0.01f;
        state.compressor.thresholdDb = juce::jmap (amount, -6.0f, -30.0f);
        state.compressor.ratio       = juce::jmap (amount, 1.2f, 3.5f);
        changed();
    };

    noiseCombo.addItem ("Off", 1);
    noiseCombo.addItem ("Gentle", 2);
    noiseCombo.addItem ("Strong", 3);
    addAndMakeVisible (noiseCombo);
    noiseCombo.onChange = [this]
    {
        switch (noiseCombo.getSelectedItemIndex())
        {
            case 2:
                state.noiseReduction.mode = otoha::NoiseReductionMode::gentle;
                state.noiseReduction.strength = std::max (state.noiseReduction.strength, 0.5f);
                break;
            case 3:
                state.noiseReduction.mode = otoha::NoiseReductionMode::strong;
                state.noiseReduction.strength = std::max (state.noiseReduction.strength, 0.75f);
                break;
            default:
                state.noiseReduction.mode = otoha::NoiseReductionMode::off;
                break;
        }
        changed();
    };

    addAndMakeVisible (limiterToggle);
    limiterToggle.onClick = [this]
    {
        state.limiter.enabled = limiterToggle.getToggleState();
        changed();
    };

    // --- advanced section -------------------------------------------------------
    addAndMakeVisible (advancedToggle);
    advancedToggle.onClick = [this] { advancedArea.setVisible (advancedToggle.getToggleState()); resized(); };
    addChildComponent (advancedArea);

    auto adv = [&] (juce::Slider& s, double lo, double hi)
    {
        makeVerticalSlider (s, lo, hi, s.getValue());
        s.setSliderStyle (juce::Slider::LinearBar);
        advancedArea.addAndMakeVisible (s);
        s.onValueChange = [this] { changed(); };
    };

    adv (eqLowMid, -12, 12);      eqLowMid.onValueChange  = [this] { state.eq.gainsDb[1] = (float) eqLowMid.getValue(); };
    adv (eqHighMid, -12, 12);     eqHighMid.onValueChange = [this] { state.eq.gainsDb[3] = (float) eqHighMid.getValue(); };
    adv (compThreshold, -40, 0);  compThreshold.onValueChange = [this] { state.compressor.thresholdDb = (float) compThreshold.getValue(); };
    adv (compRatio, 1.2, 8);      compRatio.onValueChange     = [this] { state.compressor.ratio = (float) compRatio.getValue(); };
    adv (compAttack, 1, 100);     compAttack.onValueChange    = [this] { state.compressor.attackMs = (float) compAttack.getValue(); };
    adv (compRelease, 50, 600);   compRelease.onValueChange   = [this] { state.compressor.releaseMs = (float) compRelease.getValue(); };
    adv (compMakeup, 0, 12);      compMakeup.onValueChange    = [this] { state.compressor.makeupGainDb = (float) compMakeup.getValue(); };
    adv (limCeiling, -6, 0);      limCeiling.onValueChange    = [this] { state.limiter.ceilingDb = (float) limCeiling.getValue(); };
    adv (nrStrength, 0, 1);       nrStrength.onValueChange    = [this] { state.noiseReduction.strength = (float) nrStrength.getValue(); };

    startTimerHz (4);
}

void EnhancePanel::applyPreset (otoha::DspPreset p)
{
    const bool abEnhancedWasSelected = abEnhanced.getToggleState();
    state = otoha::presetToState (p);

    enableToggle.setToggleState (state.enabled, juce::dontSendNotification);
    limiterToggle.setToggleState (state.limiter.enabled, juce::dontSendNotification);
    bassSlider.setValue (state.eq.gainsDb[0], juce::dontSendNotification);
    midSlider.setValue (state.eq.gainsDb[2], juce::dontSendNotification);
    trebleSlider.setValue (state.eq.gainsDb[4], juce::dontSendNotification);
    compressionSlider.setValue (
        juce::jmap (juce::jlimit (-30.0f, -6.0f, state.compressor.thresholdDb), -30.0f, -6.0f, 0.0f, 1.0f),
        juce::dontSendNotification);
    noiseCombo.setSelectedItemIndex (
        state.noiseReduction.mode == otoha::NoiseReductionMode::gentle ? 1
      : state.noiseReduction.mode == otoha::NoiseReductionMode::strong ? 2 : 0,
        juce::dontSendNotification);

    eqLowMid.setValue (state.eq.gainsDb[1], juce::dontSendNotification);
    eqHighMid.setValue (state.eq.gainsDb[3], juce::dontSendNotification);
    compThreshold.setValue (state.compressor.thresholdDb, juce::dontSendNotification);
    compRatio.setValue (state.compressor.ratio, juce::dontSendNotification);
    compAttack.setValue (state.compressor.attackMs, juce::dontSendNotification);
    compRelease.setValue (state.compressor.releaseMs, juce::dontSendNotification);
    compMakeup.setValue (state.compressor.makeupGainDb, juce::dontSendNotification);
    limCeiling.setValue (state.limiter.ceilingDb, juce::dontSendNotification);
    nrStrength.setValue (state.noiseReduction.strength, juce::dontSendNotification);

    abOriginal.setVisible (state.enabled);
    abEnhanced.setVisible (state.enabled);
    if (state.enabled && ! abEnhancedWasSelected)
        ; // keep whatever A/B side the listener chose

    changed();
}

void EnhancePanel::changed()
{
    // Push to the audio path immediately; the editor decides what "preview"
    // means (live chain update — no re-render).
    if (onChange) onChange();

    // Preset-modified marker ("Voice*").
    const auto selected = (otoha::DspPreset) juce::jmax (0, presetCombo.getSelectedItemIndex());
    const int nameIndex = presetCombo.getSelectedItemIndex();
    if (nameIndex > 0 && otoha::stateDiffersFromPreset (state, selected))
        presetCombo.setItemText (nameIndex + 1,
                                 otoha::presetToString (selected) + " *");
    else if (nameIndex >= 0)
        presetCombo.setItemText (nameIndex + 1, otoha::presetToString (selected));

    repaint();
}

juce::String EnhancePanel::presetLabel() const
{
    return presetCombo.getText();
}

void EnhancePanel::timerCallback()
{
    // Cheap refresh of the * marker in case state changed elsewhere.
    const int idx = presetCombo.getSelectedItemIndex();
    if (idx <= 0) return;
    const auto p = (otoha::DspPreset) idx;
    const bool differs = otoha::stateDiffersFromPreset (state, p);
    const juce::String wanted = otoha::presetToString (p) + (differs ? " *" : "");
    if (presetCombo.getItemText (idx) != wanted)
        presetCombo.setItemText (idx + 1, wanted);
}

void EnhancePanel::paint (juce::Graphics& g)
{
    g.setColour (findColour (juce::ResizableWindow::backgroundColourId).contrasting (0.04f));
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (4.0f), 10.0f);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (4.0f), 10.0f, 1.0f);

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    g.drawText ("Enhance", getLocalBounds().removeFromTop (26).reduced (10, 0),
                juce::Justification::centredLeft);

    g.setColour (juce::Colours::grey);
    g.setFont (juce::FontOptions (12.0f));
    auto labelAt = [this] (const juce::Rectangle<int>& r, const juce::String& text)
    {
        g.drawText (text, r.removeFromLeft (110), juce::Justification::centredLeft);
    };

    auto area = getLocalBounds().reduced (10);
    area.removeFromTop (28);
    labelAt (area.removeFromTop (24), "Bass");
    labelAt (area.removeFromTop (24), "Mids");
    labelAt (area.removeFromTop (24), "Treble");
    labelAt (area.removeFromTop (24), "Compression");
    labelAt (area.removeFromTop (24), "Noise reduction");
}

void EnhancePanel::resized()
{
    auto area = getLocalBounds().reduced (10);
    area.removeFromTop (28);

    auto controlsRow = area.removeFromTop (26);
    enableToggle.setBounds (controlsRow.removeFromLeft (90));
    presetCombo.setBounds (controlsRow.removeFromRight (150));
    resetButton.setBounds (controlsRow.removeFromRight (64));
    controlsRow.removeFromRight (8);
    abOriginal.setBounds (controlsRow.removeFromRight (84));
    abEnhanced.setBounds (controlsRow.removeFromRight (92));

    auto place = [&area] (juce::Component& c, int h = 24)
    {
        auto row = area.removeFromTop (h);
        c.setBounds (row.withTrimmedLeft (116));
    };

    place (bassSlider);
    place (midSlider);
    place (trebleSlider);
    place (compressionSlider);
    place (noiseCombo);
    place (limiterToggle);
    place (advancedToggle);

    if (advancedArea.isVisible())
        advancedArea.setBounds (area);
}
