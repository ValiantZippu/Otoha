#pragma once

/*
    DsButton — the Otoha design-system buttons (M18).

    Button     : label button — 4 variants x 3 sizes, optional vector icon.
    IconButton : compact square button for a single vector icon.

    Both consume OtohaTheme tokens only, share the one focus treatment
    (DsCore::drawFocusRing), and support Normal/Hover/Pressed/Focused/Disabled.
    States derive from the active accent — components never assume a hue (M24).
*/

#include "DsCore.h"

namespace otoha::ds
{

enum class ButtonVariant { primary, secondary, tertiary, danger };
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

/** Convenience vector icons (simple strokes; no raster assets). */
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
