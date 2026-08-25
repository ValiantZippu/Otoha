#pragma once

/*    DsMenu — context menu primitives (M26).

    DsMenuItem    : single menu entry (label, icon, shortcut, checked, danger)
    DsMenuPanel   : keyboard-navigable popup menu (↑/↓/Enter/Esc)
    DsContextMenu : right-click context menu wrapper
*/

#include "DsCore.h"
#include "OtohaIcons.h"
#include "DsButton.h"

namespace otoha::ds
{

// ---------------------------------------------------------------------------
// MenuItem data
// ---------------------------------------------------------------------------

struct MenuItem
{
    juce::String label;
    juce::Path icon;                     // optional (unit square path)
    juce::String shortcutLabel;          // e.g. "Ctrl+Z"
    bool checked = false;                // checkmark if non-null concept
    bool hasCheck = false;               // whether to show a check column
    bool danger = false;                 // red-tinted
    bool enabled = true;
    std::function<void()> onAction;
};

// ---------------------------------------------------------------------------
// MenuPanel (keyboard-navigable popup — Kaiteyo DsMenuPanel)
// ---------------------------------------------------------------------------

class MenuPanel : public juce::Component,
                  private juce::KeyListener
{
public:
    MenuPanel (const juce::Array<MenuItem>& items, std::function<void()> onDismissIn)
        : menuItems (items), onDismiss (std::move (onDismissIn))
    {
        setSelectedIndex (firstEnabledIndex());
        addKeyListener (this);
        setWantsKeyboardFocus (true);
        grabKeyboardFocus();
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const float r = (float) theme::Radius::medium;

        g.setColour (theme::colors::surfaceElevated());
        g.fillRoundedRectangle (bounds, r);
        g.setColour (theme::colors::border());
        g.drawRoundedRectangle (bounds.reduced (0.5f), r, 1.0f);

        const int itemH = 32;
        const int pad = theme::Spacing::xs;

        for (int i = 0; i < menuItems.size(); ++i)
        {
            const auto& item = menuItems.getReference (i);
            auto row = juce::Rectangle<float> (0.0f, (float) (pad + i * itemH),
                                                bounds.getWidth(), (float) itemH);

            // selected highlight
            if (i == selectedIndex)
            {
                g.setColour (theme::colors::accent().withAlpha (0.14f));
                g.fillRoundedRectangle (row.reduced (4.0f, 1.0f), (float) theme::Radius::small);
            }

            auto content = row.reduced ((float) theme::Spacing::sm, 0.0f);

            // icon
            if (! item.icon.isEmpty())
            {
                const float iconBox = 16.0f;
                auto iconArea = juce::Rectangle<float> (iconBox, iconBox)
                    .withCentre ({ content.getX() + iconBox / 2.0f + 2.0f, row.getCentreY() });
                g.setColour (item.danger ? theme::colors::danger()
                                         : (item.enabled ? theme::colors::textSecondary()
                                                         : theme::colors::textDisabled()));
                g.fillPath (item.icon, item.icon.getTransformToScaleToFit (iconArea, true));
                content = content.withTrimmedLeft ((int) iconBox + theme::Spacing::sm);
            }

            // label
            g.setColour (item.danger ? theme::colors::danger()
                         : (item.enabled ? theme::colors::textPrimary()
                                         : theme::colors::textDisabled()));
            g.setFont (theme::font (theme::TextSize::bodySmall));
            auto labelArea = content;
            if (item.hasCheck || item.shortcutLabel.isNotEmpty())
                labelArea = labelArea.withTrimmedRight (60);
            g.drawText (item.label, labelArea, juce::Justification::centredLeft);

            // checkmark
            if (item.hasCheck && item.checked)
            {
                g.setColour (theme::colors::accent());
                g.setFont (theme::font (theme::TextSize::bodySmall));
                g.drawText ("✓", content.getRight() - 50, (int) row.getY(), 20, (int) row.getHeight(),
                            juce::Justification::centredRight);
            }

            // shortcut label
            if (item.shortcutLabel.isNotEmpty())
            {
                g.setColour (theme::colors::textMuted());
                g.setFont (theme::font (theme::TextSize::caption));
                g.drawText (item.shortcutLabel,
                            juce::Rectangle<int> ((int) (content.getRight() - 50), (int) row.getY(), 50, (int) row.getHeight()),
                            juce::Justification::centredRight);
            }
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        const int idx = indexAt (e.getPosition());
        if (idx >= 0 && idx < menuItems.size())
        {
            const auto& item = menuItems.getReference (idx);
            if (item.enabled && item.onAction)
            {
                if (onDismiss) onDismiss();
                item.onAction();
            }
        }
    }

    bool keyPressed (const juce::KeyPress& key, juce::Component*) override
    {
        if (key == juce::KeyPress::upKey)
        {
            moveSelection (-1);
            return true;
        }
        if (key == juce::KeyPress::downKey)
        {
            moveSelection (1);
            return true;
        }
        if (key == juce::KeyPress::returnKey || key == juce::KeyPress::spaceKey)
        {
            if (selectedIndex >= 0 && selectedIndex < menuItems.size())
            {
                const auto& item = menuItems.getReference (selectedIndex);
                if (item.enabled && item.onAction)
                {
                    if (onDismiss) onDismiss();
                    item.onAction();
                }
            }
            return true;
        }
        if (key == juce::KeyPress::escapeKey)
        {
            if (onDismiss) onDismiss();
            return true;
        }
        return false;
    }

    int getPreferredHeight() const
    {
        return (int) menuItems.size() * 32 + theme::Spacing::xs * 2;
    }

private:
    int indexAt (const juce::Point<int>& pos) const
    {
        const int pad = theme::Spacing::xs;
        const int itemH = 32;
        int idx = (pos.y - pad) / itemH;
        return (idx >= 0 && idx < menuItems.size()) ? idx : -1;
    }

    void moveSelection (int dir)
    {
        int idx = selectedIndex;
        for (int attempts = 0; attempts < menuItems.size(); ++attempts)
        {
            idx = (idx + dir + menuItems.size()) % menuItems.size();
            if (menuItems.getReference (idx).enabled)
            {
                setSelectedIndex (idx);
                repaint();
                return;
            }
        }
    }

    void setSelectedIndex (int idx) { selectedIndex = idx; }

    int firstEnabledIndex() const
    {
        for (int i = 0; i < menuItems.size(); ++i)
            if (menuItems.getReference (i).enabled) return i;
        return 0;
    }

    const juce::Array<MenuItem>& menuItems;
    int selectedIndex = 0;
    std::function<void()> onDismiss;
};

// ---------------------------------------------------------------------------
// ContextMenu (right-click wrapper)
// ---------------------------------------------------------------------------

/** Shows a popup menu at the mouse position on right-click. */
class ContextMenu : public juce::Component
{
public:
    ContextMenu() { setInterceptsMouseClicks (false, false); }

    void setItems (juce::Array<MenuItem> items) { menuItems = std::move (items); }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! e.mods.isPopupMenu()) return;
        if (menuItems.isEmpty()) return;

        auto* panel = new MenuPanel (menuItems, [] {});

        // Show as a simple overlay for now — context menus can be enhanced later.
        addAndMakeVisible (panel);
        panel->setBounds (getLocalBounds());
        panel->toFront (true);
    }

private:
    juce::Array<MenuItem> menuItems;
};

} // namespace otoha::ds
