#pragma once

/*
    DsCore — shared plumbing for the Otoha design-system components (M18).

    Components consume OtohaTheme tokens only. This header holds the bits
    every component needs: live theme watching (M24 prep), the shared focus
    ring, and a shared chevron affordance.
*/

#include "../OtohaTheme.h"

namespace otoha::ds
{

/** Repaint (and optionally re-apply colours of) a component when the theme changes. */
class ThemeWatcher : private juce::ChangeListener
{
public:
    explicit ThemeWatcher (juce::Component& ownerIn,
                           std::function<void()> onThemeChangedIn = {})
        : owner (ownerIn), onThemeChanged (std::move (onThemeChangedIn))
    {
        theme::themeChangedBroadcaster().addChangeListener (this);
    }

    ~ThemeWatcher() override { theme::themeChangedBroadcaster().removeChangeListener (this); }

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        if (onThemeChanged)
            onThemeChanged();
        owner.repaint();
    }

    juce::Component& owner;
    std::function<void()> onThemeChanged;
};

/** The one focus treatment — every interactive Ds component uses this. */
inline void drawFocusRing (juce::Graphics& g, const juce::Rectangle<float>& bounds, float radius)
{
    g.setColour (theme::colors::focusRing());
    g.drawRoundedRectangle (bounds.reduced (1.5f), radius, 2.0f);
}

/** Shared chevron for combo boxes / similar affordances. */
inline juce::Path chevronDown (juce::Rectangle<float> area)
{
    juce::Path p;
    const auto c = area.getCentre();
    p.startNewSubPath (c.x - area.getWidth() * 0.5f, c.y - area.getHeight() * 0.25f);
    p.lineTo (c.x, c.y + area.getHeight() * 0.25f);
    p.lineTo (c.x + area.getWidth() * 0.5f, c.y - area.getHeight() * 0.25f);
    return p;
}

} // namespace otoha::ds
