#include "OnboardingView.h"

#include "../Dsp/Presets.h"

/*
    Onboarding implementation. AMOLED base + restrained sakura accent, matching
    the Sound view's visual language. Defaults are the conservative ones from
    the release spec (#5): Natural preset, Enhance on at 100%, System Default.
*/
OnboardingView::OnboardingView()
{
    title.setFont (juce::FontOptions (44.0f, juce::Font::bold));
    title.setJustificationType (juce::Justification::centred);
    title.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (title);

    tagline.setFont (juce::FontOptions (18.0f));
    tagline.setJustificationType (juce::Justification::centred);
    tagline.setColour (juce::Label::textColourId, juce::Colour (0xffd8c7ce));
    addAndMakeVisible (tagline);

    getStartedButton.onClick = [this] { revealSetup(); };
    addAndMakeVisible (getStartedButton);

    // --- setup rows (hidden until Get Started) --------------------------------
    for (auto* l : { &outputLabel, &enhanceLabel, &presetLabel })
    {
        l->setColour (juce::Label::textColourId, juce::Colours::white);
        addChildComponent (*l);
    }

    outputCombo.addItem ("System Default", 1);
    outputCombo.setSelectedItemIndex (0, juce::dontSendNotification);
    outputCombo.setColour (juce::ComboBox::textColourId, juce::Colours::white);
    outputCombo.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff141414));
    addChildComponent (outputCombo);

    enhanceToggle.setClickingTogglesState (true);
    enhanceToggle.setToggleState (true, juce::dontSendNotification);   // default ON
    enhanceToggle.setColour (juce::ToggleButton::textColourId, juce::Colours::white);
    addChildComponent (enhanceToggle);

    const auto presets = otoha::allDspPresets();
    for (int i = 0; i < presets.size(); ++i)
        if (presets.getReference (i) != otoha::DspPreset::off)
            presetCombo.addItem (otoha::presetToString (presets.getReference (i)), i + 1);
    presetCombo.setSelectedItemIndex (1, juce::dontSendNotification);   // Natural
    presetCombo.setColour (juce::ComboBox::textColourId, juce::Colours::white);
    presetCombo.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff141414));
    addChildComponent (presetCombo);

    hintLabel.setText ("You can change everything later under Advanced.",
                       juce::dontSendNotification);
    hintLabel.setJustificationType (juce::Justification::centred);
    hintLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8a7a82));
    addChildComponent (hintLabel);

    doneButton.onClick = [this]
    {
        if (onFinished)
            onFinished (enhanceToggle.getToggleState(),
                        presetCombo.getText());
    };
    addChildComponent (doneButton);
}

void OnboardingView::revealSetup()
{
    setupRevealed = true;
    getStartedButton.setVisible (false);
    for (auto* c : { (juce::Component*) &outputLabel, &outputCombo,
                     &enhanceLabel, &enhanceToggle,
                     &presetLabel, &presetCombo, &hintLabel, &doneButton })
        c->setVisible (true);
    resized();
}

void OnboardingView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff000000));

    juce::ColourGradient gradient (juce::Colour (0x38ff9ecf), (float) getWidth() * 0.15f, (float) getHeight() * 0.2f,
                                   juce::Colour (0x08ff9ecf), (float) getWidth() * 0.85f, (float) getHeight() * 0.75f, false);
    g.setGradientFill (gradient);
    g.fillRect (getLocalBounds());
}

void OnboardingView::resized()
{
    auto bounds = getLocalBounds();
    const int centreW = juce::jmin (420, bounds.getWidth() - 48);

    auto centre = bounds.withSizeKeepingCentre (centreW, bounds.getHeight() - 64);

    if (! setupRevealed)
    {
        title.setBounds   (centre.removeFromTop (64));
        tagline.setBounds (centre.removeFromTop (32));
        centre.removeFromTop (28);
        getStartedButton.setBounds (centre.removeFromTop (44).withSizeKeepingCentre (180, 40));
        return;
    }

    title.setBounds   (centre.removeFromTop (56));
    tagline.setBounds (centre.removeFromTop (26));
    centre.removeFromTop (24);

    const int rowH = 34, gap = 10;
    auto placeRow = [&centre, rowH] (juce::Component& label, juce::Component& control)
    {
        auto row = centre.removeFromTop (rowH);
        label.setBounds (row.removeFromLeft (80));
        control.setBounds (row);
    };

    placeRow (outputLabel, outputCombo);  centre.removeFromTop (gap);
    placeRow (enhanceLabel, enhanceToggle); centre.removeFromTop (gap);
    placeRow (presetLabel, presetCombo);  centre.removeFromTop (gap + 6);

    hintLabel.setBounds (centre.removeFromTop (22));
    centre.removeFromTop (12);
    doneButton.setBounds (centre.removeFromTop (40).withSizeKeepingCentre (140, 36));
}
