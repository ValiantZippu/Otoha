#pragma once

/*    DsControls — themed input controls (M18, M26 upgrade).

    M26 additions:
      - Bottom-border focus indicator (2dp accent line on focus)
      - Leading icon support for ComboBox and Input
      - DsSearchField (search icon + text + clear button)
      - DsSelect (styled dropdown with keyboard nav)
      - DsTextField (multi-line text area)
*/

#include "DsCore.h"
#include "OtohaIcons.h"

namespace otoha::ds
{

// ---------------------------------------------------------------------------
// ComboBox (upgraded with leading icon + bottom-border focus)
// ---------------------------------------------------------------------------

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

    void setLeadingIcon (juce::Path iconPath) { leadingIcon = std::move (iconPath); repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        const float r = (float) theme::Radius::medium;

        g.setColour (isEnabled() ? theme::colors::surface() : theme::colors::surface().darker (0.3f));
        g.fillRoundedRectangle (bounds, r);

        g.setColour (error ? theme::colors::danger() : theme::colors::border());
        g.drawRoundedRectangle (bounds.reduced (0.5f), r, 1.0f);

        // bottom-border focus indicator (Kaiteyo pattern)
        if (hasKeyboardFocus (true))
        {
            const float focusY = bounds.getBottom() - 2.0f;
            g.setColour (theme::colors::accent());
            g.fillRect (bounds.getX() + r, focusY, bounds.getWidth() - r * 2.0f, 2.0f);
        }

        auto textArea = bounds.reduced (theme::Spacing::md, 0);

        // leading icon
        if (! leadingIcon.isEmpty())
        {
            const float iconBox = 16.0f;
            auto iconArea = juce::Rectangle<float> (iconBox, iconBox)
                .withCentre ({ textArea.getX() + iconBox / 2.0f + 2.0f, bounds.getCentreY() });
            g.setColour (isEnabled() ? theme::colors::textSecondary() : theme::colors::textDisabled());
            g.fillPath (leadingIcon, leadingIcon.getTransformToScaleToFit (iconArea, true));
            textArea = textArea.withTrimmedLeft ((int) iconBox + theme::Spacing::sm);
        }

        const auto text = getText();
        g.setColour (text.isNotEmpty() ? theme::colors::textPrimary() : theme::colors::textMuted());
        g.setFont (theme::font (theme::TextSize::bodySmall));
        g.drawText (text.isNotEmpty() ? text : placeholderText, textArea,
                    juce::Justification::centredLeft);

        auto chevronArea = bounds.removeFromRight (24.0f).withSizeKeepingCentre (10.0f, 6.0f);
        g.setColour (isEnabled() ? theme::colors::textSecondary() : theme::colors::textDisabled());
        g.fillPath (chevronDown (chevronArea),
                    chevronDown (chevronArea).getTransformToScaleToFit (chevronArea, true));
    }

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
    juce::Path leadingIcon;
    bool error = false;
    std::unique_ptr<ThemeWatcher> watcher;
};

// ---------------------------------------------------------------------------
// Slider (with bottom-border focus + optional value readout)
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Toggle (on/off switch)
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Input (single-line text editor with optional leading icon + bottom-border focus)
// ---------------------------------------------------------------------------

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

    void setLeadingIcon (juce::Path iconPath) { leadingIcon = std::move (iconPath); repaint(); }

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

    juce::Path leadingIcon;
    bool error = false;
    std::unique_ptr<ThemeWatcher> watcher;
};

// ---------------------------------------------------------------------------
// SearchField (search icon + text + clear button — Kaiteyo DsSearchField)
// ---------------------------------------------------------------------------

/** Search field with clear button — the canonical library/global input. */
class SearchField : public juce::Component,
                    private juce::TextEditor::Listener
{
public:
    SearchField (const juce::String& placeholderText = "Search…")
        : placeholder (placeholderText)
    {
        editor.setTextToShowWhenEmpty (placeholder, theme::colors::textMuted());
        editor.setFont (theme::font (theme::TextSize::bodySmall));
        editor.setColour (juce::TextEditor::backgroundColourId, theme::colors::surfaceHover());
        editor.setColour (juce::TextEditor::textColourId, theme::colors::textPrimary());
        editor.setColour (juce::TextEditor::highlightColourId, theme::colors::selection());
        editor.setColour (juce::TextEditor::outlineColourId, theme::colors::border());
        editor.setColour (juce::TextEditor::focusedOutlineColourId, theme::colors::accent());
        editor.addListener (this);
        addAndMakeVisible (editor);
        watcher = std::make_unique<ThemeWatcher> (*this, [this]
        {
            editor.setColour (juce::TextEditor::backgroundColourId, theme::colors::surfaceHover());
            editor.setColour (juce::TextEditor::textColourId, theme::colors::textPrimary());
            editor.setColour (juce::TextEditor::highlightColourId, theme::colors::selection());
            editor.setColour (juce::TextEditor::outlineColourId, theme::colors::border());
            editor.setColour (juce::TextEditor::focusedOutlineColourId, theme::colors::accent());
            clearVisible = editor.getText().isNotEmpty();
            repaint();
        });
    }

    ~SearchField() override { editor.removeListener (this); }

    juce::String getText() const       { return editor.getText(); }
    void setText (const juce::String& t) { editor.setText (t); }

    void focusEditor() { editor.grabKeyboardFocus(); }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const float r = (float) theme::Radius::medium;

        g.setColour (theme::colors::surfaceHover());
        g.fillRoundedRectangle (bounds, r);

        // search icon
        const float iconSize = 16.0f;
        auto iconArea = juce::Rectangle<float> (iconSize, iconSize)
            .withCentre ({ bounds.getX() + theme::Spacing::md + iconSize / 2.0f,
                           bounds.getCentreY() });
        g.setColour (theme::colors::textMuted());
        g.fillPath (otoha::icons::search(),
                    otoha::icons::search().getTransformToScaleToFit (iconArea, true));
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        const int iconSpace = theme::Spacing::md + 16 + theme::Spacing::sm;
        editor.setBounds (bounds.withTrimmedLeft (iconSpace));
    }

private:
    void textEditorTextChanged (juce::TextEditor&) override
    {
        clearVisible = editor.getText().isNotEmpty();
        repaint();
    }

    juce::TextEditor editor;
    juce::String placeholder;
    bool clearVisible = false;
    std::unique_ptr<ThemeWatcher> watcher;
};

// ---------------------------------------------------------------------------
// Select (styled dropdown — Kaiteyo DsSelect pattern)
// ---------------------------------------------------------------------------

/** Styled dropdown with keyboard navigation. Wraps juce::ComboBox with DS theming. */
class Select : public juce::ComboBox
{
public:
    Select (const juce::String& accessibleName)
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

        g.setColour (isEnabled() ? theme::colors::surfaceElevated() : theme::colors::surface().darker (0.3f));
        g.fillRoundedRectangle (bounds, r);

        g.setColour (theme::colors::border());
        g.drawRoundedRectangle (bounds.reduced (0.5f), r, 1.0f);

        if (hasKeyboardFocus (true))
            drawFocusRing (g, bounds, r);

        const auto text = getText();
        g.setColour (text.isNotEmpty() ? theme::colors::textPrimary() : theme::colors::textMuted());
        g.setFont (theme::font (theme::TextSize::bodySmall));
        g.drawText (text.isNotEmpty() ? text : "Choose…",
                    bounds.reduced (theme::Spacing::md, 0),
                    juce::Justification::centredLeft);

        auto chevronArea = bounds.removeFromRight (24.0f).withSizeKeepingCentre (10.0f, 6.0f);
        g.setColour (theme::colors::textSecondary());
        g.fillPath (chevronDown (chevronArea),
                    chevronDown (chevronArea).getTransformToScaleToFit (chevronArea, true));
    }

private:
    void applyColours()
    {
        setColour (juce::ComboBox::backgroundColourId, theme::colors::surfaceElevated());
        setColour (juce::ComboBox::textColourId,       theme::colors::textPrimary());
        setColour (juce::ComboBox::outlineColourId,    theme::colors::border());
    }

    std::unique_ptr<ThemeWatcher> watcher;
};

// ---------------------------------------------------------------------------
// TextField (multi-line text area — Kaiteyo DsTextArea)
// ---------------------------------------------------------------------------

/** Multi-line themed text editor. */
class TextField : public juce::TextEditor
{
public:
    TextField (const juce::String& accessibleName, const juce::String& placeholder = {})
    {
        theme::label (*this, accessibleName);
        setMultiLine (true, true);
        setReturnKeyStartsNewLine (true);
        applyColours();
        setFont (theme::font (theme::TextSize::bodySmall));
        setIndents (theme::Spacing::md, theme::Spacing::sm);
        if (placeholder.isNotEmpty())
            setTextToShowWhenEmpty (placeholder, theme::colors::textMuted());
        watcher = std::make_unique<ThemeWatcher> (*this, [this] { applyColours(); });
    }

private:
    void applyColours()
    {
        setColour (juce::TextEditor::backgroundColourId,       theme::colors::surface());
        setColour (juce::TextEditor::textColourId,             theme::colors::textPrimary());
        setColour (juce::TextEditor::highlightColourId,        theme::colors::selection());
        setColour (juce::TextEditor::focusedOutlineColourId,   theme::colors::focusRing());
        setColour (juce::TextEditor::outlineColourId,          theme::colors::border());
    }

    std::unique_ptr<ThemeWatcher> watcher;
};

} // namespace otoha::ds
