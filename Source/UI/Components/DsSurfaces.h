#pragma once

/*
    DsSurfaces — the quiet building blocks (M18): Card, Tag, Divider, Section,
    EmptyState. All consume OtohaTheme tokens only.
*/

#include "DsCore.h"

namespace otoha::ds
{

/** Surface card. Interactive mode adds hover/pressed/focus/selected states
    (never hover-only — touch has no hover). Children go in contentBounds(). */
class Card : public juce::Button
{
public:
    explicit Card (const juce::String& accessibleName, bool interactive = true)
        : juce::Button (accessibleName), interactive (interactive)
    {
        theme::label (*this, accessibleName);
        if (interactive)
            setWantsKeyboardFocus (true);
        watcher = std::make_unique<ThemeWatcher> (*this);
    }

    void setSelected (bool shouldBeSelected) { selected = shouldBeSelected; repaint(); }
    bool isSelected() const                  { return selected; }

    /** Prominent cards (e.g. a hero action) tint toward the accent. Still just tokens. */
    void setProminent (bool shouldBeProminent) { prominent = shouldBeProminent; repaint(); }

    /** Where callers place children. */
    juce::Rectangle<int> contentBounds() const
    {
        return getLocalBounds().reduced (theme::Metrics::cardPadding);
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const float r = (float) theme::Radius::large;

        juce::Colour fill = theme::colors::surface();
        if (interactive && isEnabled())
        {
            if (down) fill = theme::colors::surfacePressed();
            else if (over) fill = theme::colors::surfaceHover();
            else if (prominent) fill = theme::colors::accentSoft();
        }
        else if (! isEnabled())
            fill = theme::colors::surface().darker (0.3f);

        g.setColour (fill);
        g.fillRoundedRectangle (bounds, r);

        const bool accentEdge = selected || (prominent && isEnabled());
        g.setColour (accentEdge ? theme::colors::accent() : theme::colors::borderSubtle());
        g.drawRoundedRectangle (bounds.reduced (0.5f), r, accentEdge ? 1.5f : 1.0f);

        if (interactive && hasKeyboardFocus (true))
            drawFocusRing (g, bounds, r);
    }

    void clicked (const juce::ModifierKeys&) override
    {
        if (! interactive)
            setToggleState (false, juce::dontSendNotification); // inert card ignores clicks
    }

private:
    bool interactive;
    bool selected = false;
    bool prominent = false;
    std::unique_ptr<ThemeWatcher> watcher;
};

/** Compact semantic tag (Enhanced / Lossless / Voice…). Not a button. */
class Tag : public juce::Component
{
public:
    enum class Variant { neutral, accent, success, warning, danger };

    Tag (const juce::String& text, Variant v = Variant::neutral)
        : text_ (text), variant (v)
    {
        theme::label (*this, text);
        watcher = std::make_unique<ThemeWatcher> (*this);
    }

    void paint (juce::Graphics& g) override
    {
        juce::Colour bg, fg;
        switch (variant)
        {
            case Variant::accent:  bg = theme::colors::accentSoft();  fg = theme::colors::accent();  break;
            case Variant::success: bg = theme::colors::success().withAlpha (0.16f); fg = theme::colors::success(); break;
            case Variant::warning: bg = theme::colors::warning().withAlpha (0.16f); fg = theme::colors::warning(); break;
            case Variant::danger:  bg = theme::colors::danger().withAlpha (0.16f);  fg = theme::colors::danger();  break;
            case Variant::neutral: break;
        }
        if (variant == Variant::neutral)
        {
            bg = theme::colors::surfaceHover();
            fg = theme::colors::textSecondary();
        }

        const auto bounds = getLocalBounds().toFloat();
        g.setColour (bg);
        g.fillRoundedRectangle (bounds, (float) theme::Radius::small);
        g.setColour (fg);
        g.setFont (theme::font (theme::TextSize::caption));
        g.drawText (text_, bounds.reduced (8.0f, 0), juce::Justification::centred);
    }

private:
    juce::String text_;
    Variant variant;
    std::unique_ptr<ThemeWatcher> watcher;
};

/** Subtle horizontal rule. Use sparingly — whitespace separates normally. */
class Divider : public juce::Component
{
public:
    Divider() { setInterceptsMouseClicks (false, false); watcher = std::make_unique<ThemeWatcher> (*this); }

    void paint (juce::Graphics& g) override
    {
        g.setColour (theme::colors::borderSubtle());
        g.fillRect (getLocalBounds().withSizeKeepingCentre (getWidth(), 1));
    }

private:
    std::unique_ptr<ThemeWatcher> watcher;
};

/** Section header: title + optional description + optional trailing widget. */
class Section : public juce::Component
{
public:
    Section (const juce::String& title, const juce::String& description = {})
        : title_ (title), description_ (description)
    {
        setInterceptsMouseClicks (false, true);
        watcher = std::make_unique<ThemeWatcher> (*this);
    }

    void setTrailing (std::unique_ptr<juce::Component> c)
    {
        trailing = std::move (c);
        if (trailing)
            addAndMakeVisible (*trailing);
        resized();
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (theme::colors::textPrimary());
        g.setFont (theme::font (theme::TextSize::heading));
        g.drawText (title_, getLocalBounds().removeFromTop (titleHeight),
                    juce::Justification::centredLeft);

        if (description_.isNotEmpty())
        {
            g.setColour (theme::colors::textMuted());
            g.setFont (theme::font (theme::TextSize::caption));
            g.drawText (description_, getLocalBounds()
                          .withTrimmedTop (titleHeight).removeFromTop (descHeight),
                        juce::Justification::centredLeft);
        }
    }

    void resized() override
    {
        if (trailing)
            trailing->setBounds (getLocalBounds().removeFromRight (trailing->getWidth())
                                             .withSizeKeepingCentre (trailing->getWidth(),
                                                                     trailing->getHeight()));
    }

private:
    static constexpr int titleHeight = 22;
    static constexpr int descHeight = 18;
    juce::String title_, description_;
    std::unique_ptr<juce::Component> trailing;
    std::unique_ptr<ThemeWatcher> watcher;
};

/** Empty-state pattern: icon / title / description / action — all optional. */
class EmptyState : public juce::Component
{
public:
    struct Setup
    {
        juce::Path icon;                 // optional (unit square path)
        juce::String title, description; // optional
        juce::Button* action = nullptr;  // optional; caller retains ownership
    };

    EmptyState (Setup s) : setup (std::move (s))
    {
        setInterceptsMouseClicks (false, true);
        if (setup.action)
            addAndMakeVisible (*setup.action);
        watcher = std::make_unique<ThemeWatcher> (*this);
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        auto y = bounds.getY();

        if (! setup.icon.isEmpty())
        {
            const float s = 40.0f;
            auto iconArea = juce::Rectangle<float> (s, s).withCentre ({ bounds.getCentreX(), y + s });
            g.setColour (theme::colors::textMuted());
            g.fillPath (setup.icon, setup.icon.getTransformToScaleToFit (iconArea, true));
            y += s + theme::Spacing::md;
        }

        if (setup.title.isNotEmpty())
        {
            g.setColour (theme::colors::textPrimary());
            g.setFont (theme::font (theme::TextSize::title));
            g.drawText (setup.title,
                        juce::Rectangle<int> (bounds.getX(), (int) y, bounds.getWidth(), 30),
                        juce::Justification::centred);
            y += 30 + theme::Spacing::sm;
        }

        if (setup.description.isNotEmpty())
        {
            g.setColour (theme::colors::textMuted());
            g.setFont (theme::font (theme::TextSize::body));
            g.drawFittedText (setup.description,
                              juce::Rectangle<int> (bounds.getX(), (int) y, bounds.getWidth(), 44),
                              juce::Justification::centred, 2);
        }
    }

    void resized() override
    {
        if (setup.action)
        {
            const int w = 160, h = buttonHeight (ButtonSize::medium);
            setup.action->setBounds (getLocalBounds().withSizeKeepingCentre (w, h)
                                        .withBottom (getBottom() - theme::Spacing::xl));
        }
    }

private:
    Setup setup;
    std::unique_ptr<ThemeWatcher> watcher;
};

} // namespace otoha::ds
