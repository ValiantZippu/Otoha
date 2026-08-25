#pragma once

/*
    DsControls — themed input controls (M18): ComboBox, Slider, Toggle, Input.

    All consume OtohaTheme tokens, share the one focus ring, recolor live via
    ThemeWatcher, and keep stock JUCE keyboard/mouse behaviour.
*/

#include "DsCore.h"

namespace otoha::ds
{

/** Themed dropdown. Keyboard (arrows/Enter/Escape) comes from juce::ComboBox. */
class ComboBox : public juce::ComboBox
{
public:
    ComboBox (const juce::String& accessibleName, const juce::String& placeholder = {})
        : placeholderText (placeholder)
    {
        theme::label (*this, accessibleName);
        setWantsKeyboardFocus (true);
        watcher = std::make_unique<ThemeWatcher> (*this, [this] { applyColours(); });
        applyColours();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        const float r = (float) theme::Radius::medium;

        g.setColour (isEnabled() ? theme::colors::surface() : theme::colors::surface().darker (0.3f));
        g.fillRoundedRectangle (bounds, r);

        g.setColour (error ? theme::colors::danger() : theme::colors::border());
        g.drawRoundedRectangle (bounds.reduced (0.5f), r, 1.0f);

        if (hasKeyboardFocus (true))
            drawFocusRing (g, bounds, r);

        const auto text = getText();
        g.setColour (text.isNotEmpty() ? theme::colors::textPrimary() : theme::colors::textMuted());
        g.setFont (theme::font (theme::TextSize::bodySmall));
        g.drawText (text.isNotEmpty() ? text : placeholderText,
                    bounds.reduced (theme::Spacing::md, 0),
                    juce::Justification::centredLeft);

        auto chevronArea = bounds.removeFromRight (24.0f).withSizeKeepingCentre (10.0f, 6.0f);
        g.setColour (isEnabled() ? theme::colors::textSecondary() : theme::colors::textDisabled());
        g.fillPath (chevronDown (chevronArea),
                    chevronDown (chevronArea).getTransformToScaleToFit (chevronArea, true));
    }

    /** Optional error state — the screen owns validation messaging. */
    void setError (bool shouldShowError) { error = shouldShowError; repaint(); }

private:
    void applyColours()
    {
        setColour (juce::ComboBox::backgroundColourId, theme::colors::surface());
        setColour (juce::ComboBox::textColourId,       theme::colors::textPrimary());
        setColour (juce::ComboBox::outlineColourId,    theme::colors::border());
        setColour (juce::ComboBox::buttonColourId,     theme::colors::surfaceElevated());
    }

    juce::String placeholderText;
    bool error = false;
    std::unique_ptr<ThemeWatcher> watcher;
};

/** Themed linear slider with an optional value readout. Keyboard arrows and
    Home/End come from juce::Slider. */
class Slider : public juce::Slider
{
public:
    Slider (const juce::String& accessibleName, bool showValue = false)
        : juce::Slider (juce::Slider::LinearHorizontal, juce::Slider::NoTextBox)
    {
        theme::label (*this, accessibleName);
        setWantsKeyboardFocus (true);
        if (showValue)
        {
            valueLabel.setFont (theme::font (theme::TextSize::caption));
            valueLabel.setColour (juce::Label::textColourId, theme::colors::textSecondary());
            valueLabel.setJustificationType (juce::Justification::centredRight);
            addAndMakeVisible (valueLabel);
            onValueChange = [this] { valueLabel.setText (formattedValue(), juce::dontSendNotification); };
        }
        watcher = std::make_unique<ThemeWatcher> (*this, [this]
        {
            valueLabel.setColour (juce::Label::textColourId, theme::colors::textSecondary());
            valueLabel.setText (formattedValue(), juce::dontSendNotification);
        });
    }

    void paint (juce::Graphics& g) override
    {
        auto track = getLocalBounds().toFloat();
        if (valueLabel.isVisible())
            track.removeFromRight (valueWidth);
        track = track.withSizeKeepingCentre (track.getWidth(), 4.0f);

        const float r = track.getHeight() * 0.5f;
        const double norm = juce::jlimit (0.0, 1.0,
            (getValue() - getMinimum()) / juce::jmax (1.0e-9, getMaximum() - getMinimum()));

        g.setColour (isEnabled() ? theme::colors::surfaceHover() : theme::colors::surface());
        g.fillRoundedRectangle (track, r);

        const auto filled = track.withWidth ((float) (track.getWidth() * norm));
        g.setColour (isEnabled() ? theme::colors::accent() : theme::colors::textDisabled());
        g.fillRoundedRectangle (filled, r);

        if (hasKeyboardFocus (true))
            drawFocusRing (g, getLocalBounds().toFloat(), (float) theme::Radius::small);

        const auto thumb = juce::Rectangle<float> (14.0f, 14.0f)
            .withCentre ({ filled.getRight(), track.getCentreY() });
        g.setColour (theme::colors::textPrimary());
        g.fillEllipse (thumb);
    }

    void resized() override
    {
        if (valueLabel.isVisible())
            valueLabel.setBounds (getLocalBounds().removeFromRight (valueWidth).reduced (4, 0));
    }

private:
    juce::String formattedValue() const
    {
        const double v = getValue();
        return std::abs (v - (double) (int) v) < 1.0e-6
            ? juce::String ((int) v) : juce::String (v, 2);
    }

    juce::Label valueLabel;
    int valueWidth = 48;
    std::unique_ptr<ThemeWatcher> watcher;
};

/** Themed on/off switch. On/off is conveyed by the knob position, not colour alone. */
class Toggle : public juce::ToggleButton
{
public:
    explicit Toggle (const juce::String& accessibleName)
    {
        theme::label (*this, accessibleName);
        setWantsKeyboardFocus (true);
        watcher = std::make_unique<ThemeWatcher> (*this);
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto trackArea = getLocalBounds().toFloat();
        juce::String text = getButtonText();
        if (text.isNotEmpty())
            trackArea = trackArea.removeFromRight (switchWidth);

        const float h = 22.0f;
        const auto track = trackArea.withSizeKeepingCentre (switchWidth, h);
        const float r = h * 0.5f;

        juce::Colour fill;
        if (! isEnabled())          fill = theme::colors::surface();
        else if (down)              fill = theme::colors::surfacePressed();
        else if (over)              fill = getToggleState() ? theme::colors::accentHover()
                                                           : theme::colors::surfaceHover();
        else                        fill = getToggleState() ? theme::colors::accent()
                                                           : theme::colors::surfaceHover();

        g.setColour (fill);
        g.fillRoundedRectangle (track, r);
        g.setColour (theme::colors::border());
        g.drawRoundedRectangle (track.reduced (0.5f), r, 1.0f);

        const float knob = h - 6.0f;
        const float x = getToggleState() ? track.getRight() - knob - 3.0f
                                         : track.getX() + 3.0f;
        auto knobBounds = juce::Rectangle<float> (knob, knob).withPosition (x, track.getY() + 3.0f);
        g.setColour (isEnabled() ? theme::colors::textPrimary() : theme::colors::textDisabled());
        g.fillEllipse (knobBounds);

        if (hasKeyboardFocus (true))
            drawFocusRing (g, getLocalBounds().toFloat(), r);

        if (text.isNotEmpty())
        {
            g.setColour (isEnabled() ? theme::colors::textPrimary() : theme::colors::textDisabled());
            g.setFont (theme::font (theme::TextSize::bodySmall));
            g.drawText (text, getLocalBounds().toFloat()
                              .withTrimmedRight (switchWidth + theme::Spacing::sm),
                        juce::Justification::centredLeft);
        }
    }

private:
    static constexpr float switchWidth = 40.0f;
    std::unique_ptr<ThemeWatcher> watcher;
};

/** Themed single-line text editor with an optional error state.
    The screen provides any validation message; the component stays dumb. */
class Input : public juce::TextEditor
{
public:
    Input (const juce::String& accessibleName, const juce::String& placeholder = {})
    {
        theme::label (*this, accessibleName);
        applyColours();
        setFont (theme::font (theme::TextSize::bodySmall));
        setIndents (theme::Spacing::md, 0);
        if (placeholder.isNotEmpty())
            setTextToShowWhenEmpty (placeholder, theme::colors::textMuted());
        watcher = std::make_unique<ThemeWatcher> (*this, [this] { applyColours(); });
    }

    void setError (bool shouldShowError)
    {
        error = shouldShowError;
        setColour (juce::TextEditor::outlineColourId,
                   error ? theme::colors::danger() : theme::colors::border());
        repaint();
    }

    bool hasError() const { return error; }

private:
    void applyColours()
    {
        setColour (juce::TextEditor::backgroundColourId,       theme::colors::surface());
        setColour (juce::TextEditor::textColourId,             theme::colors::textPrimary());
        setColour (juce::TextEditor::highlightColourId,        theme::colors::selection());
        setColour (juce::TextEditor::focusedOutlineColourId,   theme::colors::focusRing());
        setColour (juce::TextEditor::outlineColourId,
                   error ? theme::colors::danger() : theme::colors::border());
    }

    bool error = false;
    std::unique_ptr<ThemeWatcher> watcher;
};

} // namespace otoha::ds
