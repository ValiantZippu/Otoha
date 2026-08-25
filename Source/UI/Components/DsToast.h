#pragma once

/*
    DsToast — lightweight non-blocking notifications (M18).

    Add one ToastHost as an overlay over the window content; call show().
    Toasts stack from the bottom, auto-dismiss, dismiss on click, and never
    cover primary controls (host is click-transparent except on toasts).
    Errors are announced via the accessibility system where supported.
*/

#include "DsCore.h"

namespace otoha::ds
{

class ToastHost : public juce::Component,
                  private juce::Timer
{
public:
    enum class Kind { info, success, warning, error };

    explicit ToastHost() { setInterceptsMouseClicks (true, true); watcher = std::make_unique<ThemeWatcher> (*this); }

    void show (Kind kind, const juce::String& message, int dismissAfterMs = 3500)
    {
        auto* toast = new ToastItem (*this, kind, message);
        addAndMakeVisible (*toast);
        toasts.insert (0, toast); // newest on top of the stack
        layout();
        startTimer (100);

        announce (kind, message);
    }

    /** Test hook — immediately dismiss everything. */
    void dismissAll()
    {
        while (toasts.size() > 0)
            dismiss (toasts.getFirst());
    }

    void paint (juce::Graphics& g) override { juce::ignoreUnused (g); }

    void resized() override { layout(); }

    void timerCallback() override
    {
        const auto now = juce::Time::getMillisecondCounter();
        for (auto* t : toasts)
            if (now >= t->expiresAt)
            {
                dismiss (t);
                return; // one per tick keeps iteration safe
            }
        if (toasts.isEmpty())
            stopTimer();
    }

private:
    struct ToastItem : juce::Component
    {
        ToastItem (ToastHost& h, Kind k, const juce::String& m)
            : host (h), kind (k), message (m),
              expiresAt (juce::Time::getMillisecondCounter() + (juce::uint32) 3500)
        {
            theme::label (*this, message);
        }

        void paint (juce::Graphics& g) override
        {
            const auto bounds = getLocalBounds().toFloat();
            const float r = (float) theme::Radius::medium;

            g.setColour (theme::colors::surfaceElevated());
            g.fillRoundedRectangle (bounds, r);
            g.setColour (theme::colors::border());
            g.drawRoundedRectangle (bounds.reduced (0.5f), r, 1.0f);

            juce::Colour dot;
            switch (kind)
            {
                case Kind::success: dot = theme::colors::success(); break;
                case Kind::warning: dot = theme::colors::warning(); break;
                case Kind::error:   dot = theme::colors::danger();  break;
                case Kind::info:    dot = theme::colors::info();    break;
            }
            g.setColour (dot);
            g.fillEllipse (12.0f, bounds.getCentreY() - 4.0f, 8.0f, 8.0f);

            g.setColour (theme::colors::textPrimary());
            g.setFont (theme::font (theme::TextSize::bodySmall));
            g.drawText (message, bounds.reduced (28.0f, 0), juce::Justification::centredLeft);
        }

        void mouseUp (const juce::MouseEvent&) override { host.dismiss (this); }

        ToastHost& host;
        Kind kind;
        juce::String message;
        juce::uint32 expiresAt;
    };

    void dismiss (ToastItem* t)
    {
        toasts.removeAllInstancesOf (t);
        removeChildComponent (t);
        delete t;
        layout();
        if (toasts.isEmpty())
            stopTimer();
    }

    void layout()
    {
        auto y = getHeight() - theme::Spacing::xl;
        for (auto* t : toasts)
        {
            const int w = juce::jmin (360, getWidth() - 2 * theme::Spacing::xl);
            const int h = 44;
            t->setBounds ((getWidth() - w) / 2, y - h, w, h);
            y -= h + theme::Spacing::sm;
        }
    }

    static void announce (Kind kind, const juce::String& message)
    {
        if (kind != Kind::error)
            return; // errors are the must-not-be-missed case
        // Best-effort accessibility announcement; never blocks.
        for (int i = 0; i < juce::Desktop::getInstance().getNumComponents(); ++i)
            if (auto* win = dynamic_cast<juce::TopLevelWindow*> (juce::Desktop::getInstance().getComponent (i)))
                if (auto* ah = win->getAccessibilityHandler())
                {
                    ah->postAnnouncement (message, juce::AccessibilityHandler::AnnouncementPriority::high);
                    return;
                }
    }

    juce::Array<ToastItem*> toasts;
    std::unique_ptr<ThemeWatcher> watcher;
};

} // namespace otoha::ds
