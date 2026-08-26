#pragma once

/*  DsToolbar — Top-bar and toolbar-divider for the workspace shell (M28).

    Matches the Kaiteyo DsToolbar / DsTopBar visual language:

      ┌───────────────────────────────────────────────────────┐
      │ Title                                   Search   ⋯    │
      │ Subtitle                                                │
      └───────────────────────────────────────────────────────┘
      ──────────────────────────────────────────────────────────

    Components:
      DsToolbar       — title + optional subtitle + trailing action area
      DsSearchTrigger — compact "Search or jump to…" pill with Ctrl+K badge
      DsToolbarDivider — subtle 1 dp line below the toolbar
*/

#include "DsCore.h"
#include "OtohaIcons.h"

namespace otoha::ds
{

// ---------------------------------------------------------------------------
// Search trigger (Kaiteyo: elevated surface pill with icon + text + shortcut badge)
// ---------------------------------------------------------------------------

class SearchTrigger : public juce::Component
{
public:
    using Callback = std::function<void()>;

    SearchTrigger()
    {
        setWantsKeyboardFocus (true);
        setMouseClickGrabsKeyboardFocus (true);
        theme::label (*this, "Search or jump to");
        watcher = std::make_unique<ThemeWatcher> (*this);
    }

    void onClicked (Callback cb) { callback = std::move (cb); }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const float r = (float) theme::Radius::medium;

        // background
        juce::Colour bg;
        if (isMouseButtonDown())
            bg = theme::colors::surfacePressed();
        else if (isMouseOver())
            bg = theme::colors::surfaceHover();
        else
            bg = theme::colors::surfaceElevated();

        g.setColour (bg);
        g.fillRoundedRectangle (bounds, r);

        // border
        g.setColour (theme::colors::border().withAlpha (0.3f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), r, 1.0f);

        // search icon
        const float iconSize = 14.0f;
        const float iconY = bounds.getCentreY() - iconSize / 2.0f;
        juce::Rectangle<float> iconArea (iconSize, iconSize);
        iconArea.setPosition (bounds.getX() + 10.0f, iconY);

        g.setColour (theme::colors::textMuted());
        g.fillPath (otoha::icons::search(),
                    otoha::icons::search().getTransformToScaleToFit (iconArea, true));

        // "Search or jump to…" text
        g.setColour (theme::colors::textMuted());
        g.setFont (theme::font (theme::TextSize::body));
        g.drawText ("Search or jump to…",
                    iconArea.translated (iconSize + 8.0f, 0.0f)
                           .withWidth (bounds.getWidth() - iconSize - 80.0f),
                    juce::Justification::centredLeft, false);

        // Ctrl+K badge
        const float badgeW = 36.0f;
        const float badgeH = 16.0f;
        auto badge = juce::Rectangle<float> (badgeW, badgeH)
            .withCentre ({ bounds.getRight() - 14.0f, bounds.getCentreY() });

        g.setColour (theme::colors::surfaceSubtle());
        g.fillRoundedRectangle (badge, (float) theme::Radius::small);
        g.setColour (theme::colors::textMuted());
        g.setFont (theme::font (theme::TextSize::caption));
        g.drawText ("Ctrl+K", badge, juce::Justification::centred, false);

        // focus ring
        if (hasKeyboardFocus (true))
            drawFocusRing (g, bounds, r);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (isMouseOver() && callback)
            callback();
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::returnKey || key == juce::KeyPress::spaceKey)
        {
            if (callback) callback();
            return true;
        }
        return false;
    }

    static int preferredWidth() { return 240; }

private:
    Callback callback;
    std::unique_ptr<ThemeWatcher> watcher;
};

// ---------------------------------------------------------------------------
// Toolbar (Kaiteyo DsToolbar: title + subtitle + trailing actions)
// ---------------------------------------------------------------------------

class Toolbar : public juce::Component
{
public:
    Toolbar()
    {
        watcher = std::make_unique<ThemeWatcher> (*this);
        setOpaque (true);
    }

    void setTitle (const juce::String& t)  { titleText = t; repaint(); }
    void setSubtitle (const juce::String& s) { subtitleText = s; repaint(); }

    /** Add a component to the trailing actions area (e.g. sort button, settings). */
    void addAction (juce::Component& action)
    {
        addAndMakeVisible (action);
        actions.add (&action);
        resized();
    }

    void removeAllActions()
    {
        for (auto* a : actions)
            removeChildComponent (a);
        actions.clear();
        resized();
    }

    // --- search trigger integration -------------------------------------------

    void setSearchTrigger (SearchTrigger* trigger)
    {
        searchTrigger = trigger;
        if (trigger != nullptr)
            addAndMakeVisible (*trigger);
        resized();
    }

    // --- layout --------------------------------------------------------------

    void resized() override
    {
        auto bounds = getLocalBounds();
        auto content = bounds.reduced ((int) theme::Spacing::lg,
                                       (int) theme::Spacing::md);

        // action buttons on the right
        const int actionSize = theme::Metrics::touchTargetMin;
        for (int i = actions.size() - 1; i >= 0; --i)
        {
            if (auto* comp = actions.getUnchecked (i))
                comp->setBounds (content.removeFromRight (actionSize)
                                     .reduced (0, (content.getHeight() - actionSize) / 2));
            content.removeFromRight ((int) theme::Spacing::xs);
        }

        // search trigger
        if (searchTrigger != nullptr)
        {
            const int searchW = SearchTrigger::preferredWidth();
            searchTrigger->setBounds (content.removeFromRight (searchW)
                                           .reduced (0, (content.getHeight() - 36) / 2));
            content.removeFromRight ((int) theme::Spacing::sm);
        }

        // title + subtitle on the left
        titleBounds = content;
    }

    // --- painting ------------------------------------------------------------

    void paint (juce::Graphics& g) override
    {
        g.setColour (theme::colors::background());
        g.fillRect (getLocalBounds());

        const auto& content = titleBounds.toFloat();

        // title
        g.setColour (theme::colors::textPrimary());
        g.setFont (theme::font (theme::TextSize::title, true));
        g.drawText (titleText,
                    content.withHeight (22.0f),
                    juce::Justification::centredLeft, false);

        // subtitle (optional)
        if (subtitleText.isNotEmpty())
        {
            g.setColour (theme::colors::textMuted());
            g.setFont (theme::font (theme::TextSize::caption));
            g.drawText (subtitleText,
                        content.withTop (content.getY() + 20.0f).withHeight (16.0f),
                        juce::Justification::centredLeft, false);
        }
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        // subtle bottom divider (Kaiteyo: border at 0.4 alpha)
        auto divBounds = getLocalBounds().toFloat();
        g.setColour (theme::colors::border().withAlpha (0.4f));
        g.fillRect (divBounds.getX(), divBounds.getBottom() - 1.0f,
                     divBounds.getWidth(), 1.0f);
    }

    static int preferredHeight()
    {
        return (int) (theme::Spacing::md * 2 + theme::Spacing::lg)
             + (int) theme::font (theme::TextSize::title).getHeight()
             + 4;
    }

private:
    juce::String titleText { "Studio" };
    juce::String subtitleText;
    juce::Rectangle<int> titleBounds;
    juce::OwnedArray<juce::Component> actions;
    SearchTrigger* searchTrigger = nullptr;
    std::unique_ptr<ThemeWatcher> watcher;
};

// ---------------------------------------------------------------------------
// Toolbar Divider (standalone, reusable)
// ---------------------------------------------------------------------------

class ToolbarDivider : public juce::Component
{
public:
    ToolbarDivider()
    {
        setOpaque (true);
        setAccessible (false);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (theme::colors::border().withAlpha (0.4f));
        g.fillRect (0, 0, getWidth(), 1);
    }

    static int preferredHeight() { return 1; }
};

} // namespace otoha::ds
