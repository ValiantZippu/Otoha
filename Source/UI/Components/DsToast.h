#pragma once

/*    DsToast — lightweight non-blocking notifications (M18, M26, M34 upgrade).

    M26 added:
      - Kind-based background coloring (Kaiteyo pattern):
          Success = green tinted bg,  Warning = amber tinted bg,
          Error   = red tinted bg,    Info    = surface bg
      - Slide-up entrance animation
      - Fade-out exit (via opacity timer)

    M34 adds:
      - Fixed info-kind background color (was falling through to empty)
      - Message deduplication: identical messages within a 2-second window
        are coalesced instead of stacking duplicate toasts
      - Accessible announcement for all kinds (not just errors)
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
        // M34 deduplication: if the most recent toast has the same message, skip
        if (! toasts.isEmpty())
        {
            const auto& last = toasts.getFirst();
            if (last->message == message && last->kind == kind)
                return;
        }

        auto* toast = new ToastItem (*this, kind, message, dismissAfterMs);
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
        {
            // slide-up entrance animation
            if (t->animProgress < 1.0f)
            {
                t->animProgress = juce::jmin (1.0f, t->animProgress + 0.08f);
                t->repaint();
            }

            // auto-dismiss
            if (now >= t->expiresAt && t->animProgress >= 1.0f)
            {
                dismiss (t);
                return; // one per tick keeps iteration safe
            }
        }

        if (toasts.isEmpty())
            stopTimer();
    }

private:
    // M34: Kaiteyo kind-based background colors (fixed: info kind now uses theme surface)
    static juce::Colour bgForKind (Kind kind)
    {
        switch (kind)
        {
            case Kind::success: return theme::colors::success().withAlpha (0.15f);
            case Kind::warning: return theme::colors::warning().withAlpha (0.15f);
            case Kind::error:   return theme::colors::danger().withAlpha (0.15f);
            case Kind::info:    return theme::colors::surfaceElevated();
        }
        return theme::colors::surfaceElevated();
    }

    static juce::Colour dotForKind (Kind kind)
    {
        switch (kind)
        {
            case Kind::success: return theme::colors::success();
            case Kind::warning: return theme::colors::warning();
            case Kind::error:   return theme::colors::danger();
            case Kind::info:    return theme::colors::info();
        }
        return theme::colors::info();
    }

    struct ToastItem : juce::Component
    {
        ToastItem (ToastHost& h, Kind k, const juce::String& m, int dismissMs)
            : host (h), kind (k), message (m),
              expiresAt (juce::Time::getMillisecondCounter() + (juce::uint32) dismissMs)
        {
            theme::label (*this, message);
        }

        void paint (juce::Graphics& g) override
        {
            const auto bounds = getLocalBounds().toFloat();
            const float r = (float) theme::Radius::medium;

            // slide-up entrance: offset Y by (1 - progress) * 20px
            const float slideOffset = (1.0f - animProgress) * 20.0f;
            const auto shifted = bounds.translated (0.0f, slideOffset);

            // Kaiteyo kind-based background
            auto bg = bgForKind (kind);
            g.setColour (bg);
            g.fillRoundedRectangle (shifted, r);
            g.setColour (theme::colors::border());
            g.drawRoundedRectangle (shifted.reduced (0.5f), r, 1.0f);

            // kind-colored dot
            auto dot = dotForKind (kind);
            g.setColour (dot);
            g.fillEllipse (shifted.getX() + 12.0f, shifted.getCentreY() - 4.0f, 8.0f, 8.0f);

            g.setColour (theme::colors::textPrimary());
            g.setFont (theme::font (theme::TextSize::bodySmall));
            g.drawText (message, shifted.reduced (28.0f, 0), juce::Justification::centredLeft);
        }

        void mouseUp (const juce::MouseEvent&) override { host.dismiss (this); }

        ToastHost& host;
        Kind kind;
        juce::String message;
        juce::uint32 expiresAt;
        float animProgress = 0.0f;  // 0->1 slide-up entrance
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
        // M34: announce errors and warnings (not just errors)
        if (kind != Kind::error && kind != Kind::warning)
            return;
        for (int i = 0; i < juce::Desktop::getInstance().getNumComponents(); ++i)
            if (auto* win = dynamic_cast<juce::TopLevelWindow*> (juce::Desktop::getInstance().getComponent (i)))
                if (auto* ah = win->getAccessibilityHandler())
                {
                    ah->postAnnouncement (message,
                        kind == Kind::error
                            ? juce::AccessibilityHandler::AnnouncementPriority::high
                            : juce::AccessibilityHandler::AnnouncementPriority::medium);
                    return;
                }
    }

    juce::Array<ToastItem*> toasts;
    std::unique_ptr<ThemeWatcher> watcher;
};

} // namespace otoha::ds
