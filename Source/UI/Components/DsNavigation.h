#pragma once

/*    DsNavigation — the floating sidebar shell (M19, M27 upgrade).

    M27 adds:
      - Vector logo mark (waveform icon) at top
      - "Audio Studio" subtitle in expanded mode
      - Compact/expanded toggle button
      - Overflow menu button at bottom (⋯)
      - Tooltips via accessibility labels in compact mode
      - Improved spacing matching Kaiteyo DsNavRail pattern

    The sidebar consumes OtohaTheme tokens exclusively and sits above the
    content background as a floating island.
*/

#include "DsCore.h"
#include "OtohaIcons.h"

namespace otoha::ds
{

// ---------------------------------------------------------------------------
// Navigation item (M27: improved with tooltip support)
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

        // background states (Kaiteyo DsNavItem)
        if (isEnabled())
        {
            if (isMouseButtonDown())
            {
                g.setColour (theme::colors::surfacePressed());
                g.fillRoundedRectangle (bounds, r);
            }
            else if (isMouseOver() && ! active)
            {
                g.setColour (theme::colors::surfaceHover().withAlpha (0.6f));
                g.fillRoundedRectangle (bounds, r);
            }
            else if (active)
            {
                // accent tinted background (Kaiteyo: accent at 16% alpha)
                g.setColour (theme::colors::accent().withAlpha (0.16f));
                g.fillRoundedRectangle (bounds, r);

                // accent indicator bar (3dp wide, 16dp tall, 2dp radius)
                g.setColour (theme::colors::accent());
                g.fillRoundedRectangle (bounds.getX(), bounds.getY() + 6.0f,
                                        3.0f, bounds.getHeight() - 12.0f, 2.0f);
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

        // label (Kaiteyo: Body = 14sp, SemiBold when selected)
        if (labelsVisible && label_.isNotEmpty())
        {
            g.setColour (active   ? theme::colors::accent()
                         : isEnabled() ? theme::colors::textSecondary()
                                       : theme::colors::textDisabled());
            g.setFont (theme::font (theme::TextSize::body, active));
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

    void triggerSelection()
    {
        if (isEnabled() && onSelectCallback)
            onSelectCallback();
    }

    static int fullWidth() { return 200; }
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
// Sidebar (M27: floating island with logo, toggle, overflow)
// ---------------------------------------------------------------------------

/**
    The floating sidebar panel.

    Structure:
      - Logo mark + "Otoha" brand (top)
      - Primary nav items (Studio, Record, Library)
      - Spacer
      - Secondary nav items (Settings)
      - Overflow menu button (bottom)

    The sidebar floats above the content background using M17 surface/border/shadow
    tokens. It supports expanded (icon + label) and compact (icon only) modes.
*/
class Sidebar : public juce::Component,
                private juce::ChangeListener
{
public:
    using NavigateCallback = std::function<void (int)>;
    using OverflowCallback = std::function<void()>;

    Sidebar()
    {
        theme::themeChangedBroadcaster().addChangeListener (this);
        setWantsKeyboardFocus (false);
        setOpaque (true);
        setName ("Otoha Navigation");

        // compact/expanded toggle button
        expandToggle = std::make_unique<IconButton> (
            "Toggle sidebar", otoha::icons::chevronDown());
        expandToggle->setWantsKeyboardFocus (true);
        addAndMakeVisible (*expandToggle);
        expandToggle->onClick = [this]
        {
            expanded = ! expanded;
            resized();
            repaint();
            if (onExpandToggle) onExpandToggle (expanded);
        };

        // overflow menu button
        overflowBtn = std::make_unique<IconButton> (
            "More options", otoha::icons::more());
        overflowBtn->setWantsKeyboardFocus (true);
        addAndMakeVisible (*overflowBtn);
        overflowBtn->onClick = [this]
        {
            if (onOverflow) onOverflow();
        };

        // setBounds AFTER children are created so resized() can safely lay them out
        setBounds (0, 0, theme::Metrics::sidebarWidth, 600);
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

    bool isExpanded() const { return expanded; }

    void setExpanded (bool shouldBeExpanded)
    {
        if (expanded == shouldBeExpanded) return;
        expanded = shouldBeExpanded;
        resized();
        repaint();
    }

    // --- layout ----------------------------------------------------------------

    void resized() override
    {
        const bool wide = expanded && getWidth() > NavItem::compactWidth() + 12;
        for (auto& entry : items)
            entry.item->setLabelVisible (wide);

        auto bounds = getLocalBounds();

        // brand area (logo + text)
        const int brandH = wide ? (theme::Spacing::xxl + theme::Spacing::sm)
                                : (theme::Metrics::touchTargetMin);
        brandArea = bounds.removeFromTop (brandH);
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

        // spacer before secondary items
        if (bounds.getHeight() > theme::Spacing::lg * 2 + theme::Metrics::touchTargetMin * 2)
            bounds.removeFromBottom (theme::Spacing::lg);

        // overflow button at bottom
        const int overflowH = theme::Metrics::touchTargetMin;
        if (overflowBtn != nullptr)
            overflowBtn->setBounds (bounds.removeFromBottom (overflowH)
                                        .reduced (theme::Spacing::xs, 2));

        // toggle button above overflow
        if (expandToggle != nullptr)
            expandToggle->setBounds (bounds.removeFromBottom (overflowH)
                                         .reduced (theme::Spacing::xs, 2));

        // secondary items (Settings)
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
        const float r = (float) theme::Radius::xl;   // Kaiteyo SidebarRadius = 24dp

        // accent-tinted shadow behind the floating island
        g.setColour (theme::colors::accent().withAlpha (0.22f));
        g.fillRoundedRectangle (bounds.expanded (0.0f, 4.0f).translated (0.0f, 4.0f), r);

        // floating panel background
        g.setColour (theme::colors::surfaceElevated());
        g.fillRoundedRectangle (bounds, r);

        // subtle border at 0.3 alpha (Kaiteyo default)
        g.setColour (theme::colors::border().withAlpha (0.3f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), r, 1.0f);

        // brand area
        const bool wide = expanded && getWidth() > NavItem::compactWidth() + 12;
        auto brandBounds = brandArea.toFloat().reduced (theme::Spacing::sm);

        if (wide)
        {
            // expanded: logo icon + "Otoha" title + "Audio Studio" subtitle
            const float logoSize = 24.0f;
            auto logoArea = juce::Rectangle<float> (logoSize, logoSize)
                .withCentre ({ brandBounds.getX() + logoSize / 2.0f + 4.0f,
                               brandBounds.getCentreY() });

            // draw waveform logo icon
            g.setColour (theme::colors::accent());
            g.fillPath (otoha::icons::waveform(),
                        otoha::icons::waveform().getTransformToScaleToFit (logoArea, true));

            // "Otoha" title
            g.setColour (theme::colors::textPrimary());
            g.setFont (theme::font (theme::TextSize::title));
            g.drawText ("Otoha",
                        juce::Rectangle<float> (logoArea.getRight() + 8.0f,
                                                 brandBounds.getY(),
                                                 brandBounds.getWidth() - logoSize - 16.0f,
                                                 20.0f),
                        juce::Justification::centredLeft, false);

            // "Audio Studio" subtitle
            g.setColour (theme::colors::accent());
            g.setFont (theme::font (theme::TextSize::caption));
            g.drawText ("Audio Studio",
                        juce::Rectangle<float> (logoArea.getRight() + 8.0f,
                                                 brandBounds.getY() + 18.0f,
                                                 brandBounds.getWidth() - logoSize - 16.0f,
                                                 14.0f),
                        juce::Justification::centredLeft, false);
        }
        else
        {
            // compact: just the waveform logo icon centered
            const float logoSize = 20.0f;
            auto logoArea = juce::Rectangle<float> (logoSize, logoSize)
                .withCentre (brandBounds.getCentre());

            g.setColour (theme::colors::accent());
            g.fillPath (otoha::icons::waveform(),
                        otoha::icons::waveform().getTransformToScaleToFit (logoArea, true));
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
    OverflowCallback onOverflow;
    std::function<void (bool)> onExpandToggle;

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
    bool expanded = true;
    juce::Rectangle<int> brandArea;

    std::unique_ptr<IconButton> expandToggle;
    std::unique_ptr<IconButton> overflowBtn;

    std::unique_ptr<ThemeWatcher> watcher;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Sidebar)
};

} // namespace otoha::ds
