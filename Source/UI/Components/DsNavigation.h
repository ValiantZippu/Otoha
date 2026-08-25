#pragma once

/*
    DsNavigation — the floating sidebar shell (M19).

    Provides:
      Sidebar      : the floating vertical panel (brand + nav items).
      NavItem      : a single icon+label navigation entry.

    The sidebar consumes OtohaTheme tokens exclusively and sits above the
    content background rather than becoming a full-height slab.  Responsive
    behaviour collapses labels below a width breakpoint.

    M20+ screens should use Sidebar for navigation; they never build their
    own sidebar/navigation widget.
*/

#include "DsCore.h"
#include "OtohaIcons.h"

namespace otoha::ds
{

// ---------------------------------------------------------------------------
// Navigation item
// ---------------------------------------------------------------------------

/** A single icon+label entry in the sidebar. */
class NavItem : public juce::Component
{
public:
    using Callback = std::function<void()>;

    NavItem (const juce::String& label,
             juce::Path iconPath,
             const juce::String& accessibleName = {},
             Callback onSelect = {})
        : label_ (label),
          icon (std::move (iconPath)),
          onSelectCallback (std::move (onSelect))
    {
        theme::label (*this, accessibleName.isNotEmpty() ? accessibleName : label);
        setWantsKeyboardFocus (true);
        setMouseClickGrabsKeyboardFocus (true);
        watcher = std::make_unique<ThemeWatcher> (*this);
    }

    /** Set the callback fired when the item is activated. */
    void onSelect (Callback cb) { onSelectCallback = std::move (cb); }

    void setActive (bool shouldBeActive)
    {
        if (active == shouldBeActive) return;
        active = shouldBeActive;
        repaint();
    }

    bool isActive() const { return active; }

    void setLabelVisible (bool shouldShowLabels) { labelsVisible = shouldShowLabels; }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const float r = (float) theme::Radius::medium;

        // background states
        if (isEnabled())
        {
            if (isMouseButtonDown())
            {
                g.setColour (theme::colors::surfacePressed());
                g.fillRoundedRectangle (bounds, r);
            }
            else if (isMouseOver() && ! active)
            {
                g.setColour (theme::colors::surfaceHover());
                g.fillRoundedRectangle (bounds, r);
            }
            else if (active)
            {
                g.setColour (theme::colors::accentSoft());
                g.fillRoundedRectangle (bounds, r);

                // accent indicator bar on the left
                g.setColour (theme::colors::accent());
                g.fillRect (bounds.getX(), bounds.getY() + 6.0f,
                            3.0f, bounds.getHeight() - 12.0f);
            }
        }

        // icon
        const float iconBox = labelsVisible ? (float) theme::Metrics::iconSize
                                            : (float) theme::Metrics::touchTargetMin;
        const auto iconArea = juce::Rectangle<float> (iconBox, iconBox)
            .withCentre ({ labelsVisible ? bounds.getX() + 22.0f : bounds.getCentreX(),
                           bounds.getCentreY() - (labelsVisible ? 4.0f : 0.0f) });

        g.setColour (active   ? theme::colors::accent()
                     : isEnabled() ? theme::colors::textPrimary()
                                   : theme::colors::textDisabled());
        g.fillPath (icon, icon.getTransformToScaleToFit (iconArea, true));

        // label
        if (labelsVisible && label_.isNotEmpty())
        {
            g.setColour (active   ? theme::colors::accent()
                         : isEnabled() ? theme::colors::textPrimary()
                                       : theme::colors::textDisabled());
            g.setFont (theme::font (theme::TextSize::caption));
            auto labelArea = bounds.withLeft (iconArea.getRight() + 8.0f);
            g.drawText (label_, labelArea, juce::Justification::centredLeft, false);
        }

        // focus ring
        if (hasKeyboardFocus (true))
            drawFocusRing (g, bounds, r);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (isMouseOver() && isEnabled() && onSelectCallback)
            onSelectCallback();
    }

    /** Trigger the selection callback programmatically (used by Sidebar keyboard nav). */
    void triggerSelection()
    {
        if (isEnabled() && onSelectCallback)
            onSelectCallback();
    }

    /** Preferred width when labels are visible. */
    static int fullWidth() { return 200; }
    /** Width when collapsed to icon-only. */
    static int compactWidth() { return theme::Metrics::touchTargetMin + theme::Spacing::xs; }

private:
    juce::String label_;
    juce::Path icon;
    bool active = false;
    bool labelsVisible = true;
    Callback onSelectCallback;
    std::unique_ptr<ThemeWatcher> watcher;
};

// ---------------------------------------------------------------------------
// Sidebar
// ---------------------------------------------------------------------------

/**
    The floating sidebar panel.

    Contains:
      - Brand mark (top)
      - Primary nav items (Studio, Record, Library, Sound)
      - Spacer
      - Secondary nav items (Settings)

    The sidebar floats above the content background using M17 surface/border/shadow
    tokens.  It collapses to icon-only mode below a width threshold.
*/
class Sidebar : public juce::Component,
                private juce::ChangeListener
{
public:
    using NavigateCallback = std::function<void (int)>;

    Sidebar()
    {
        theme::themeChangedBroadcaster().addChangeListener (this);
        setWantsKeyboardFocus (false);
        setOpaque (true);
        setName ("Otoha Navigation");
    }

    ~Sidebar() override
    {
        theme::themeChangedBroadcaster().removeChangeListener (this);
    }

    // --- item management --------------------------------------------------------

    NavItem& addItem (int id, const juce::String& label, juce::Path icon,
                      const juce::String& accessibleName = {},
                      bool isSecondary = false)
    {
        auto* item = new NavItem (label, std::move (icon), accessibleName);
        item->onSelect ([this, id] { if (onNavigate) onNavigate (id); });
        addAndMakeVisible (*item);
        ItemEntry entry;
        entry.id = id;
        entry.item = item;
        entry.secondary = isSecondary;
        items.push_back (std::move (entry));
        return *item;
    }

    void setActiveItem (int id)
    {
        for (auto& entry : items)
            entry.item->setActive (entry.id == id);
        activeId = id;
    }

    int getActiveItem() const { return activeId; }

    // --- layout ----------------------------------------------------------------

    void resized() override
    {
        const bool wide = getWidth() > NavItem::compactWidth() + 12;
        for (auto& entry : items)
            entry.item->setLabelVisible (wide);

        auto bounds = getLocalBounds();

        // brand area
        brandArea = bounds.removeFromTop (theme::Spacing::xxl + theme::Spacing::sm);
        bounds.removeFromTop (theme::Spacing::sm);

        // primary items
        const int itemH = theme::Metrics::touchTargetMin;
        for (auto& entry : items)
        {
            if (! entry.secondary)
            {
                entry.item->setBounds (bounds.removeFromTop (itemH)
                                            .reduced (theme::Spacing::xs, 2));
            }
        }

        // push secondary items to the bottom
        int secondaryH = 0;
        for (const auto& entry : items)
            if (entry.secondary)
                secondaryH += itemH + 4;

        if (secondaryH > 0 && bounds.getHeight() > secondaryH)
            bounds.removeFromBottom (bounds.getBottom() - bounds.getY() - secondaryH
                                     - theme::Spacing::lg);

        for (auto& entry : items)
        {
            if (entry.secondary)
            {
                entry.item->setBounds (bounds.removeFromTop (itemH)
                                            .reduced (theme::Spacing::xs, 2));
            }
        }
    }

    // --- painting --------------------------------------------------------------

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const float r = (float) theme::Radius::large;

        // floating panel background
        g.setColour (theme::colors::surface());
        g.fillRoundedRectangle (bounds, r);

        // subtle border
        g.setColour (theme::colors::borderSubtle());
        g.drawRoundedRectangle (bounds.reduced (0.5f), r, 1.0f);

        // brand mark
        const bool wide = getWidth() > NavItem::compactWidth() + 12;
        auto brandBounds = brandArea.toFloat().reduced (theme::Spacing::sm);

        // "O" brand mark
        g.setColour (theme::colors::accent());
        g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
        if (wide)
        {
            g.drawText ("Otoha", brandBounds.withLeft (theme::Spacing::md),
                        juce::Justification::centredLeft, false);
        }
        else
        {
            g.drawText ("O", brandBounds, juce::Justification::centred, false);
        }
    }



    // --- keyboard --------------------------------------------------------------

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::upKey || key == juce::KeyPress::downKey)
        {
            const int dir = (key == juce::KeyPress::downKey) ? 1 : -1;
            int idx = findActiveIndex();
            for (int attempts = 0; attempts < (int) items.size(); ++attempts)
            {
                idx = (idx + dir + (int) items.size()) % (int) items.size();
                if (items[idx].item->isEnabled())
                {
                    setActiveItem (items[idx].id);
                    items[idx].item->grabKeyboardFocus();
                    return true;
                }
            }
        }

        if (key == juce::KeyPress::returnKey || key == juce::KeyPress::spaceKey)
        {
            for (auto& entry : items)
            {
                if (entry.item->hasKeyboardFocus (true) && entry.item->isEnabled())
                {
                    entry.item->triggerSelection();
                    return true;
                }
            }
        }

        return false;
    }

    // --- theme update ----------------------------------------------------------

    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        repaint();
    }

    // --- public data -----------------------------------------------------------

    NavigateCallback onNavigate;

private:
    struct ItemEntry
    {
        int id = 0;
        NavItem* item = nullptr;
        bool secondary = false;
    };

    int findActiveIndex() const
    {
        for (int i = 0; i < (int) items.size(); ++i)
            if (items[i].id == activeId)
                return i;
        return 0;
    }

    std::vector<ItemEntry> items;
    int activeId = 0;
    juce::Rectangle<int> brandArea;

    std::unique_ptr<ThemeWatcher> watcher;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Sidebar)
};

} // namespace otoha::ds
