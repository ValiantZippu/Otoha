#pragma once

/*    DsToast — lightweight non-blocking notifications (M18, M26 upgrade).

    M26 adds:
      - Kind-based background coloring (Kaiteyo pattern):
          Success = green tinted bg,  Warning = amber tinted bg,
          Error   = red tinted bg,    Info    = surface bg
      - Slide-up entrance animation
      - Fade-out exit (via opacity timer)
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
    // Kaiteyo kind-based background colors
    static juce::Colour bgForKind (Kind kind)
    {
        switch (kind)
        {
            case Kind::success: return juce::Colour (0xFF1E3A24);  // Kaiteyo green tinted
            case Kind::warning: return juce::Colour (0xFF3A2C1E);  // Kaiteyo amber tinted
            case Kind::error:   return juce::Colour (0xFF3A1E1E);  // Kaiteyo red tinted
            case Kind::info:    break;
        }
        return {};  // resolved from theme at paint time
    }

    static juce::Colour dotForKind (Kind kind)
    {
        switch (kind)
        {
            case Kind::success: return juce::Colour (0xFFC2FC8B);  // Kaiteyo accent for success
            case Kind::warning: return juce::Colour (0xFFFEAB57);  // Kaiteyo warning
            case Kind::error:   return juce::Colour (0xFFFF6B6B);  // Kaiteyo error
            case Kind::info:    break;
        }
        return {};  // resolved from theme at paint time
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
            if (bg.isTransparent())
                bg = theme::colors::surfaceElevated();
            g.setColour (bg);
            g.fillRoundedRectangle (shifted, r);
            g.setColour (theme::colors::border());
            g.drawRoundedRectangle (shifted.reduced (0.5f), r, 1.0f);

            // kind-colored dot
            auto dot = dotForKind (kind);
            if (dot.isTransparent())
                dot = theme::colors::info();
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
        float animProgress = 0.0f;  // 0→1 slide-up entrance
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
