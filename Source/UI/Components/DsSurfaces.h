#pragma once

/*    DsSurfaces — the quiet building blocks (M18, M26 upgrade).

    M26 additions:
      - Card hover: accent top-line on hover (2dp accent bar at top)
      - DsListItem: leading + title + subtitle + trailing slot
      - DsStatTile: label + big value + optional delta
      - DsProgressBar: fraction-based, accent-colored fill
      - DsBadge: pill with tinted background
      - DsSkeleton: pulsing loading placeholder
*/

#include "DsCore.h"

namespace otoha::ds
{

// ---------------------------------------------------------------------------
// Card (upgraded with accent top-line on hover — Kaiteyo DsCards.kt pattern)
// ---------------------------------------------------------------------------

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

    /** Prominent cards (e.g. a hero action) tint toward the accent. */
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

        // Kaiteyo: accent top-line on hover (2dp accent bar at top)
        if (interactive && isEnabled() && over && ! down)
        {
            g.setColour (theme::colors::accent());
            g.fillRect (bounds.getX(), bounds.getY(), bounds.getWidth(), 2.0f);
        }

        if (interactive && hasKeyboardFocus (true))
            drawFocusRing (g, bounds, r);
    }

    void clicked (const juce::ModifierKeys&) override
    {
        if (! interactive)
            setToggleState (false, juce::dontSendNotification);
    }

private:
    bool interactive;
    bool selected = false;
    bool prominent = false;
    std::unique_ptr<ThemeWatcher> watcher;
};

// ---------------------------------------------------------------------------
// ListItem (Kaiteyo DsListItem pattern)
// ---------------------------------------------------------------------------

/** Generic list item row with leading/trailing slots. */
class ListItem : public juce::Component
{
public:
    ListItem (const juce::String& titleText, const juce::String& subtitleText = {})
        : titleText_ (titleText), subtitleText_ (subtitleText)
    {
        setInterceptsMouseClicks (false, true);
        watcher = std::make_unique<ThemeWatcher> (*this);
    }

    void setLeading (std::unique_ptr<juce::Component> c)
    {
        leading = std::move (c);
        if (leading) addAndMakeVisible (*leading);
        resized();
    }

    void setTrailing (std::unique_ptr<juce::Component> c)
    {
        trailing = std::move (c);
        if (trailing) addAndMakeVisible (*trailing);
        resized();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();
        const int leftPad = leading ? 40 : theme::Spacing::lg;
        const int rightPad = trailing ? 40 : theme::Spacing::lg;

        g.setColour (theme::colors::textPrimary());
        g.setFont (theme::font (theme::TextSize::body));
        g.drawText (titleText_,
                    bounds.withTrimmedLeft (leftPad).withTrimmedRight (rightPad)
                          .removeFromTop (bounds.getHeight() / 2),
                    juce::Justification::centredLeft);

        if (subtitleText_.isNotEmpty())
        {
            g.setColour (theme::colors::textMuted());
            g.setFont (theme::font (theme::TextSize::caption));
            g.drawText (subtitleText_,
                        bounds.withTrimmedLeft (leftPad).withTrimmedRight (rightPad)
                              .withTop (bounds.getHeight() / 2),
                        juce::Justification::centredLeft);
        }
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced (theme::Spacing::md, theme::Spacing::sm);

        if (leading)
        {
            const int s = 20;
            leading->setBounds (bounds.getX(), bounds.getCentreY() - s / 2, s, s);
        }
        if (trailing)
        {
            const int s = 18;
            trailing->setBounds (bounds.getRight() - s, bounds.getCentreY() - s / 2, s, s);
        }
    }

private:
    juce::String titleText_, subtitleText_;
    std::unique_ptr<juce::Component> leading, trailing;
    std::unique_ptr<ThemeWatcher> watcher;
};

// ---------------------------------------------------------------------------
// StatTile (Kaiteyo DsStatTile pattern)
// ---------------------------------------------------------------------------

/** Stat tile: label + big value + optional delta. */
class StatTile : public juce::Component
{
public:
    StatTile (const juce::String& label, const juce::String& value,
              const juce::String& deltaText = {}, bool deltaPositive = true)
        : label_ (label), value_ (value), deltaText_ (deltaText), deltaPositive_ (deltaPositive)
    {
        setInterceptsMouseClicks (false, false);
        watcher = std::make_unique<ThemeWatcher> (*this);
    }

    void setValues (const juce::String& newValue,
                    const juce::String& newDelta = {}, bool newDeltaPositive = true)
    {
        value_ = newValue;
        deltaText_ = newDelta;
        deltaPositive_ = newDeltaPositive;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const float r = (float) theme::Radius::large;

        g.setColour (theme::colors::surface());
        g.fillRoundedRectangle (bounds, r);

        auto area = bounds.reduced (theme::Spacing::lg);

        g.setColour (theme::colors::textMuted());
        g.setFont (theme::font (theme::TextSize::caption));
        g.drawText (label_.toUpperCase(), area.removeFromTop (14),
                    juce::Justification::centredLeft);

        area.removeFromTop (theme::Spacing::xs);

        g.setColour (theme::colors::textPrimary());
        g.setFont (theme::font (theme::TextSize::heading));
        g.drawText (value_, area.removeFromTop (24),
                    juce::Justification::centredLeft);

        if (deltaText_.isNotEmpty())
        {
            area.removeFromTop (theme::Spacing::xs);
            g.setColour (deltaPositive_ ? theme::colors::success() : theme::colors::danger());
            g.setFont (theme::font (theme::TextSize::caption));
            g.drawText (deltaText_, area.removeFromTop (14),
                        juce::Justification::centredLeft);
        }
    }

private:
    juce::String label_, value_, deltaText_;
    bool deltaPositive_;
    std::unique_ptr<ThemeWatcher> watcher;
};

// ---------------------------------------------------------------------------
// ProgressBar (Kaiteyo DsProgressBar pattern)
// ---------------------------------------------------------------------------

/** Inline progress bar. Fraction 0.0–1.0, accent-colored fill. */
class ProgressBar : public juce::Component
{
public:
    ProgressBar (float initialFraction = 0.0f) : fraction (initialFraction)
    {
        setInterceptsMouseClicks (false, false);
        watcher = std::make_unique<ThemeWatcher> (*this);
    }

    void setFraction (float f) { fraction = juce::jlimit (0.0f, 1.0f, f); repaint(); }
    float getFraction() const  { return fraction; }

    void setBarColour (juce::Colour c) { barColour = c; repaint(); }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const float h = (float) getHeight();
        const float r = h * 0.5f;

        // track
        g.setColour (theme::colors::surfaceHover());
        g.fillRoundedRectangle (bounds, r);

        // fill
        const auto filled = bounds.withWidth (bounds.getWidth() * fraction);
        g.setColour (barColour.isTransparent() ? theme::colors::accent() : barColour);
        g.fillRoundedRectangle (filled, r);
    }

private:
    float fraction = 0.0f;
    juce::Colour barColour;
    std::unique_ptr<ThemeWatcher> watcher;
};

// ---------------------------------------------------------------------------
// Badge (Kaiteyo DsBadge pattern)
// ---------------------------------------------------------------------------

/** Small pill badge with tinted background. */
class Badge : public juce::Component
{
public:
    Badge (const juce::String& text, juce::Colour tint = {})
        : text_ (text), tint_ (tint)
    {
        setInterceptsMouseClicks (false, false);
        watcher = std::make_unique<ThemeWatcher> (*this);
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const float r = (float) theme::Radius::pill;

        const juce::Colour tint = tint_.isTransparent() ? theme::colors::accent() : tint_;

        g.setColour (tint.withAlpha (0.16f));
        g.fillRoundedRectangle (bounds, r);

        g.setColour (tint);
        g.setFont (theme::font (theme::TextSize::caption));
        g.drawText (text_, bounds.reduced (theme::Spacing::sm, 2),
                    juce::Justification::centred);
    }

private:
    juce::String text_;
    juce::Colour tint_;
    std::unique_ptr<ThemeWatcher> watcher;
};

// ---------------------------------------------------------------------------
// Skeleton (Kaiteyo DsSkeleton pattern — pulsing loading placeholder)
// ---------------------------------------------------------------------------

/** Animated loading placeholder. Pulses between surface tones. */
class Skeleton : public juce::Component,
                 private juce::Timer
{
public:
    Skeleton (int w = 120, int h = 12) : targetW (w), targetH (h)
    {
        setSize (w, h);
        setInterceptsMouseClicks (false, false);
        watcher = std::make_unique<ThemeWatcher> (*this);
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const float r = (float) theme::Radius::small;

        const float alpha = 0.35f + 0.4f * std::sin (phase);
        g.setColour (theme::colors::surfaceHover().withAlpha (alpha));
        g.fillRoundedRectangle (bounds, r);
    }

    void timerCallback() override
    {
        phase += 0.12f;
        if (phase > juce::MathConstants<float>::twoPi)
            phase -= juce::MathConstants<float>::twoPi;
        repaint();
    }

private:
    int targetW, targetH;
    float phase = 0.0f;
    std::unique_ptr<ThemeWatcher> watcher;
};

// ---------------------------------------------------------------------------
// Tag (semantic variant)
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Divider
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Section header (Kaiteyo DsSectionHeader pattern)
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// EmptyState
// ---------------------------------------------------------------------------

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
