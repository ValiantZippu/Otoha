#pragma once

/*    DsButton — the Otoha design-system buttons (M18, M26 upgrade).

    Button       : label button — 5 variants x 3 sizes, optional vector icon.
    IconButton   : compact square button for a single vector icon.
    TextButton   : ghost text-only button with hover tint.

    M26 adds:
      - accentTint variant (accent at 16% alpha background)
      - hover scale animation (spring: 0.97 pressed → 1.02 hover → 1.0 rest)
      - ButtonRow for grouping
*/

#include "DsCore.h"
#include <cmath>

namespace otoha::ds
{

enum class ButtonVariant { primary, secondary, tertiary, danger, accentTint };
enum class ButtonSize    { small, medium, large };

/** Button heights come from theme metrics — screens never invent heights. */
inline int buttonHeight (ButtonSize s)
{
    switch (s)
    {
        case ButtonSize::small:  return theme::Metrics::buttonHeight - 8;  // 28
        case ButtonSize::large:  return theme::Metrics::touchTargetMin;    // 44
        case ButtonSize::medium: break;
    }
    return theme::Metrics::buttonHeight;                                   // 36
}

// ---------------------------------------------------------------------------
// Hover scale animation helper (Kaiteyo spring pattern)
// ---------------------------------------------------------------------------

/** Computes a scale factor from hover/press state using a simple spring model.
    Returns 0.97 when pressed, 1.02 when hovered, 1.0 at rest. */
inline float hoverScale (bool hovered, bool pressed)
{
    if (pressed)  return 0.97f;
    if (hovered)  return 1.02f;
    return 1.0f;
}

// ---------------------------------------------------------------------------
// Button
// ---------------------------------------------------------------------------

class Button : public juce::Button
{
public:
    Button (const juce::String& label, ButtonVariant v = ButtonVariant::secondary,
            ButtonSize s = ButtonSize::medium)
        : juce::Button (label), variant (v), size (s)
    {
        theme::label (*this, label);
        setWantsKeyboardFocus (true);
        watcher = std::make_unique<ThemeWatcher> (*this);
    }

    /** Optional vector icon drawn before the label. */
    void setIcon (juce::Path iconPath)            { icon = std::move (iconPath); repaint(); }
    void setIconSize (float px)                   { iconPx = px; repaint(); }

    void setVariant (ButtonVariant v)             { variant = v; repaint(); }
    void setSize_ (ButtonSize s)                  { size = s; repaint(); }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const float r = (float) theme::Radius::medium;
        juce::Colour fill, text;

        if (! isEnabled())
        {
            fill = theme::colors::surface();
            text = theme::colors::textDisabled();
        }
        else switch (variant)
        {
            case ButtonVariant::primary:
                fill = down ? theme::colors::accentPressed()
                     : over ? theme::colors::accentHover()
                            : theme::colors::accentSoft();
                text = theme::colors::accent();
                break;
            case ButtonVariant::secondary:
                fill = down ? theme::colors::surfacePressed()
                     : over ? theme::colors::surfaceHover()
                            : theme::colors::surfaceElevated();
                text = theme::colors::textPrimary();
                break;
            case ButtonVariant::tertiary:
                fill = juce::Colours::transparentBlack;
                text = down ? theme::colors::textSecondary()
                     : over ? theme::colors::textPrimary()
                            : theme::colors::textSecondary();
                break;
            case ButtonVariant::danger:
                fill = down ? theme::colors::danger().darker (0.25f)
                     : over ? theme::colors::danger().brighter (0.15f)
                            : theme::colors::danger().withAlpha (0.16f);
                text = theme::colors::danger();
                break;
            case ButtonVariant::accentTint:
                // Kaiteyo: accent.primary at 16% alpha, darken on hover/press
                fill = down ? theme::colors::accent().withAlpha (0.34f)
                     : over ? theme::colors::accent().withAlpha (0.26f)
                            : theme::colors::accent().withAlpha (0.16f);
                text = theme::colors::accent();
                break;
        }

        // --- hover scale animation (Kaiteyo spring) ---
        const float sc = hoverScale (over, down);
        if (std::abs (sc - 1.0f) > 0.001f)
        {
            const auto centre = bounds.getCentre();
            g.addTransform (juce::AffineTransform::translation (-centre.x, -centre.y)
                                .scaled (sc)
                                .translated (centre.x, centre.y));
        }

        g.setColour (fill);
        g.fillRoundedRectangle (bounds, r);

        if (variant == ButtonVariant::secondary && isEnabled())
        {
            g.setColour (down ? theme::colors::border().brighter (0.15f)
                              : theme::colors::border());
            g.drawRoundedRectangle (bounds.reduced (0.5f), r, 1.0f);
        }

        if (hasKeyboardFocus (true))
            drawFocusRing (g, bounds, r);

        // content: [icon] label
        g.setColour (text);
        g.setFont (theme::font (theme::TextSize::button));
        auto content = bounds;
        if (! icon.isEmpty())
        {
            const float iconBox = iconPx;
            auto iconArea = content.removeFromLeft (iconBox + theme::Spacing::sm)
                                   .withSizeKeepingCentre (iconBox, iconBox);
            if (getButtonText().isEmpty())
                iconArea = content.withSizeKeepingCentre (iconBox, iconBox);
            g.fillPath (icon, icon.getTransformToScaleToFit (iconArea, true));
        }
        if (getButtonText().isNotEmpty())
            g.drawText (getButtonText(), content, juce::Justification::centred);
    }

private:
    ButtonVariant variant;
    ButtonSize size;
    juce::Path icon;
    float iconPx = (float) theme::Metrics::iconSize;
    std::unique_ptr<ThemeWatcher> watcher;
};

// ---------------------------------------------------------------------------
// IconButton
// ---------------------------------------------------------------------------

/** Compact square button for a single vector icon (play/pause/close/undo…).
    The accessible name is mandatory — pass it or the build asserts. */
class IconButton : public juce::Button
{
public:
    IconButton (const juce::String& accessibleName, juce::Path iconPath,
                ButtonSize s = ButtonSize::medium)
        : juce::Button (accessibleName), icon (std::move (iconPath)), size (s)
    {
        jassert (accessibleName.isNotEmpty()); // icon-only controls need a name
        theme::label (*this, accessibleName);
        setWantsKeyboardFocus (true);
        watcher = std::make_unique<ThemeWatcher> (*this);
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const float r = (float) theme::Radius::medium;

        if (isEnabled())
        {
            if (down)      { g.setColour (theme::colors::surfacePressed()); g.fillRoundedRectangle (bounds, r); }
            else if (over) { g.setColour (theme::colors::surfaceHover());   g.fillRoundedRectangle (bounds, r); }
        }

        if (hasKeyboardFocus (true))
            drawFocusRing (g, bounds, r);

        const auto box = bounds.withSizeKeepingCentre ((float) theme::Metrics::iconSize,
                                                       (float) theme::Metrics::iconSize);
        g.setColour (isEnabled() ? theme::colors::textPrimary() : theme::colors::textDisabled());
        g.fillPath (icon, icon.getTransformToScaleToFit (box, true));
    }

private:
    juce::Path icon;
    ButtonSize size;
    std::unique_ptr<ThemeWatcher> watcher;
};

// ---------------------------------------------------------------------------
// TextButton (ghost text-only with hover tint — Kaiteyo DsTextButton)
// ---------------------------------------------------------------------------

/** Ghost text-only button. Used for secondary actions like "Cancel", "View all". */
class TextButton : public juce::Button
{
public:
    TextButton (const juce::String& label)
        : juce::Button (label)
    {
        theme::label (*this, label);
        setWantsKeyboardFocus (true);
        watcher = std::make_unique<ThemeWatcher> (*this);
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const float r = (float) theme::Radius::small;

        juce::Colour bg;
        if (! isEnabled())
            bg = juce::Colours::transparentBlack;
        else if (down)
            bg = theme::colors::accent().withAlpha (0.10f);
        else if (over)
            bg = theme::colors::accent().withAlpha (0.10f);
        else
            bg = juce::Colours::transparentBlack;

        g.setColour (bg);
        g.fillRoundedRectangle (bounds, r);

        if (hasKeyboardFocus (true))
            drawFocusRing (g, bounds, r);

        g.setColour (isEnabled() ? theme::colors::accent() : theme::colors::textMuted());
        g.setFont (theme::font (theme::TextSize::bodySmall));
        g.drawText (getButtonText(), bounds, juce::Justification::centred);
    }

private:
    std::unique_ptr<ThemeWatcher> watcher;
};

// ---------------------------------------------------------------------------
// ButtonRow (horizontal group of buttons — Kaiteyo DsButtonRow)
// ---------------------------------------------------------------------------

/** Container that lays out child buttons horizontally with consistent spacing. */
class ButtonRow : public juce::Component
{
public:
    ButtonRow() { setInterceptsMouseClicks (false, false); }

    void resized() override
    {
        auto area = getLocalBounds();
        const int gap = theme::Spacing::sm;
        const int count = getNumChildComponents();
        if (count == 0) return;

        const int totalGap = (count - 1) * gap;
        const int buttonW = (area.getWidth() - totalGap) / count;

        for (int i = 0; i < count; ++i)
        {
            auto* child = getChildComponent (i);
            if (child != nullptr)
                child->setBounds (area.getX() + i * (buttonW + gap), area.getY(),
                                  buttonW, area.getHeight());
        }
    }
};

// ---------------------------------------------------------------------------
// Convenience vector icons (simple strokes; no raster assets)
// ---------------------------------------------------------------------------

namespace icons
{
    inline juce::Path play()
    {
        juce::Path p; p.addTriangle (0.0f, 0.0f, 1.0f, 0.5f, 0.0f, 1.0f); return p;
    }
    inline juce::Path pause()
    {
        juce::Path p; p.addRectangle (0.0f, 0.0f, 0.32f, 1.0f); p.addRectangle (0.68f, 0.0f, 0.32f, 1.0f); return p;
    }
    inline juce::Path close()
    {
        juce::Path p; p.startNewSubPath (0, 0); p.lineTo (1, 1);
        p.startNewSubPath (1, 0); p.lineTo (0, 1); return p;
    }
    inline juce::Path undo()
    {
        juce::Path p;
        p.addArc (0.1f, 0.1f, 0.8f, 0.8f, juce::MathConstants<float>::halfPi * 0.5f,
                  juce::MathConstants<float>::twoPi);
        p.lineTo (0.5f, 0.0f); p.closeSubPath();
        return p;
    }
    inline juce::Path plus()
    {
        juce::Path p; p.startNewSubPath (0.5f, 0); p.lineTo (0.5f, 1);
        p.startNewSubPath (0, 0.5f); p.lineTo (1, 0.5f); return p;
    }
} // namespace icons

} // namespace otoha::ds
