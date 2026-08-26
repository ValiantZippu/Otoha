#pragma once

/*    DsMenu — context menu primitives (M26, M34 upgrade).

    DsMenuItem    : single menu entry (label, icon, shortcut, checked, danger)
    DsMenuPanel   : keyboard-navigable popup menu (↑/↓/Enter/Esc)
    DsContextMenu : right-click context menu wrapper (M34: proper popup lifecycle)
    DsMenuOverlay : transient overlay that shows a MenuPanel at a given position
                    with viewport clamping and outside-click/Escape dismiss

    M34 additions:
      - MenuPanel properly fires onDismiss on Escape
      - ContextMenu properly cleans up previous panels (no leak)
      - DsMenuOverlay: positions a MenuPanel at screen coordinates with
        viewport clamping so menus never appear off-screen
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
        setName ("Menu");
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

            if (i == selectedIndex)
            {
                g.setColour (theme::colors::accent().withAlpha (0.14f));
                g.fillRoundedRectangle (row.reduced (4.0f, 1.0f), (float) theme::Radius::small);
            }

            auto content = row.reduced ((float) theme::Spacing::sm, 0.0f);

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

            g.setColour (item.danger ? theme::colors::danger()
                         : (item.enabled ? theme::colors::textPrimary()
                                         : theme::colors::textDisabled()));
            g.setFont (theme::font (theme::TextSize::bodySmall));
            auto labelArea = content;
            if (item.hasCheck || item.shortcutLabel.isNotEmpty())
                labelArea = labelArea.withTrimmedRight (60);
            g.drawText (item.label, labelArea, juce::Justification::centredLeft);

            if (item.hasCheck && item.checked)
            {
                g.setColour (theme::colors::accent());
                g.setFont (theme::font (theme::TextSize::bodySmall));
                g.drawText (L"\u2713", content.getRight() - 50, (int) row.getY(), 20, (int) row.getHeight(),
                            juce::Justification::centredRight);
            }

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

    int getPreferredWidth() const
    {
        int maxW = 120;
        for (const auto& item : menuItems)
        {
            int w = (int) item.label.length() * 8 + 40;
            if (item.shortcutLabel.isNotEmpty()) w += 60;
            maxW = juce::jmax (maxW, w);
        }
        return juce::jmin (maxW, 300);
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
// DsMenuOverlay (M34) — transient overlay for showing menus as popups
// ---------------------------------------------------------------------------

/** A full-window transparent overlay that shows a MenuPanel at a viewport-clamped
    position. Clicking outside the menu or pressing Escape dismisses it.

    Usage: auto* overlay = new DsMenuOverlay(parent, items, screenPos);
           parent->addAndMakeVisible(overlay);
           // overlay self-destructs on dismiss
*/
class DsMenuOverlay : public juce::Component
{
public:
    DsMenuOverlay (const juce::Array<MenuItem>& items,
                   juce::Point<int> positionInParent)
        : panel (items, [this] { dismiss(); })
    {
        setInterceptsMouseClicks (true, false);  // catch clicks on the background only

        // Viewport clamp the position
        const int prefW = panel.getPreferredWidth();
        const int prefH = panel.getPreferredHeight();

        int x = positionInParent.x;
        int y = positionInParent.y;

        // Clamp to our bounds (set by parent in resized())
        x = juce::jmax (0, x);
        y = juce::jmax (0, y);

        addAndMakeVisible (panel);
        panel.setBounds (x, y, prefW, prefH);
        panel.toFront (true);
        panel.grabKeyboardFocus();
    }

    void resized() override
    {
        // Re-clamp panel position when overlay resizes (e.g. window resize)
        auto pb = panel.getBounds();
        const int maxX = juce::jmax (0, getWidth() - pb.getWidth());
        const int maxY = juce::jmax (0, getHeight() - pb.getHeight());
        panel.setBounds (juce::jlimit (0, maxX, pb.getX()),
                         juce::jlimit (0, maxY, pb.getY()),
                         pb.getWidth(), pb.getHeight());
    }

    void mouseUp (const juce::MouseEvent&) override { dismiss(); }

private:
    void dismiss()
    {
        if (auto* p = getParentComponent())
            p->removeChildComponent (this);
        delete this;
    }

    MenuPanel panel;
};

// ---------------------------------------------------------------------------
// showMenuPopup (M34) — convenience: show a menu at a screen position
// ---------------------------------------------------------------------------

/** Show a menu as a popup overlay on `parent` at the given position.
    The popup is viewport-clamped and self-destructs on dismiss. */
inline void showMenuPopup (
    juce::Component* parent,
    const juce::Array<MenuItem>& items,
    juce::Point<int> positionInParent)
{
    if (items.isEmpty() || parent == nullptr) return;

    auto* overlay = new DsMenuOverlay (items, positionInParent);
    parent->addAndMakeVisible (overlay);
    overlay->setBounds (parent->getLocalBounds());
    overlay->toFront (true);
}

// ---------------------------------------------------------------------------
// ContextMenu (right-click wrapper, M34: proper popup lifecycle)
// ---------------------------------------------------------------------------

/** Shows a popup menu at the mouse position on right-click.
    M34: uses DsMenuOverlay for proper lifecycle management. */
class ContextMenu : public juce::Component
{
public:
    ContextMenu() { setInterceptsMouseClicks (false, false); }

    void setItems (juce::Array<MenuItem> items) { menuItems = std::move (items); }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! e.mods.isPopupMenu()) return;
        if (menuItems.isEmpty()) return;

        showMenuPopup (getParentComponent(), menuItems, e.getPosition());
    }

private:
    juce::Array<MenuItem> menuItems;
};

} // namespace otoha::ds
