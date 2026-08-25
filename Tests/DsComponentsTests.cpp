/*
    DsComponentsTests — headless M18 design-system verification.

    Exercises behaviour (clicks, disabled states, selection, toasts, theme
    swap) and smoke-paints every component into an image so paint code paths
    run without a display.
*/
#include "../Source/UI/Components/DsButton.h"
#include "../Source/UI/Components/DsControls.h"
#include "../Source/UI/Components/DsSurfaces.h"
#include "../Source/UI/Components/DsToast.h"

#include <cstdio>

namespace
{
bool expect (bool condition, const char* message)
{
    if (! condition)
        std::printf ("FAIL: %s\n", message);
    return condition;
}

/** Render a component into an image — exercises the paint path headlessly. */
template <typename Comp>
bool smokePaint (Comp& c, int w = 160, int h = 40)
{
    c.setSize (w, h);
    juce::Image img (juce::Image::ARGB, w, h, true);
    juce::Graphics g (img);
    c.paintEntireComponent (g, true);
    return true;
}

/** triggerClick posts an async command message — deliver it. */
void pumpMessages()
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI guiInit;
    using namespace otoha;
    bool ok = true;

    // --- Button behaviour ----------------------------------------------------
    {
        ds::Button b ("Go", ds::ButtonVariant::primary);
        int clicks = 0;
        b.onClick = [&] { ++clicks; };
        b.triggerClick();
        pumpMessages();
        ok &= expect (clicks == 1, "button click fires onClick");

        b.setEnabled (false);
        b.triggerClick();
        pumpMessages();
        ok &= expect (clicks == 1, "disabled button must not fire onClick");

        ok &= expect (b.getName() == "Go", "button carries its accessible name");

        for (auto v : { ds::ButtonVariant::primary, ds::ButtonVariant::secondary,
                        ds::ButtonVariant::tertiary, ds::ButtonVariant::danger })
        {
            ds::Button p ("Paint", v);
            ok &= expect (smokePaint (p), "button paints without crashing");
            p.setSize (160, ds::buttonHeight (ds::ButtonSize::large));
            ok &= expect (smokePaint (p), "large button paints");
        }
    }

    // --- IconButton ------------------------------------------------------------
    {
        ds::IconButton ib ("Play", ds::icons::play());
        ok &= expect (ib.getName() == "Play", "icon button keeps its accessible name");
        ok &= expect (smokePaint (ib, 44, 44), "icon button paints");

        ds::IconButton disabled ("Undo", ds::icons::undo());
        disabled.setEnabled (false);
        ok &= expect (smokePaint (disabled, 44, 44), "disabled icon button paints");
    }

    // --- ComboBox -----------------------------------------------------------------
    {
        ds::ComboBox cb ("Microphone", "< none >");
        cb.addItem ("A", 1);
        cb.addItem ("B", 2);
        cb.setSelectedId (2, juce::dontSendNotification);
        ok &= expect (cb.getSelectedId() == 2, "combo selection stored");
        ok &= expect (cb.getText() == "B", "combo reports selected text");
        cb.setError (true);
        ok &= expect (smokePaint (cb, 200, theme::Metrics::inputHeight + 8), "combo paints");
    }

    // --- Slider -----------------------------------------------------------------------
    {
        ds::Slider s ("Clarity", true);
        s.setRange (0.0, 100.0, 1.0);
        s.setValue (62.0);
        ok &= expect (s.getValue() == 62.0, "slider value stored");
        s.setValue (-5.0);
        ok &= expect (s.getValue() == 0.0, "slider clamps below range");
        s.setValue (500.0);
        ok &= expect (s.getValue() == 100.0, "slider clamps above range");
        ok &= expect (smokePaint (s, 240, theme::Metrics::rowHeight), "slider paints");
    }

    // --- Toggle --------------------------------------------------------------------------
    {
        ds::Toggle t ("Enhance");
        int changes = 0;
        t.onStateChange = [&] { ++changes; };
        t.setToggleState (true, juce::sendNotificationSync);
        ok &= expect (t.getToggleState(), "toggle turns on");
        ok &= expect (changes > 0, "toggle notifies state change");
        t.setToggleState (false, juce::sendNotificationSync);
        ok &= expect (! t.getToggleState(), "toggle turns off");
        ok &= expect (smokePaint (t, 160, 30), "toggle paints");
    }

    // --- Input -----------------------------------------------------------------------------
    {
        ds::Input in ("Recording name", "Untitled");
        in.setText ("take 1", juce::dontSendNotification);
        ok &= expect (in.getText() == "take 1", "input stores edited text");
        in.setError (true);
        ok &= expect (in.hasError(), "error state set");
        in.setError (false);
        ok &= expect (! in.hasError(), "error state cleared");
        ok &= expect (smokePaint (in, 200, theme::Metrics::inputHeight + 6), "input paints");
    }

    // --- Card --------------------------------------------------------------------------------
    {
        ds::Card c ("Recording card");
        int clicks = 0;
        c.onClick = [&] { ++clicks; };
        c.triggerClick();
        pumpMessages();
        ok &= expect (clicks == 1, "interactive card activates");
        c.setSelected (true);
        ok &= expect (c.isSelected(), "card selection stored");
        ok &= expect (smokePaint (c, 280, 96), "selected card paints");

        ds::Card inert ("Static card", false);
        ok &= expect (smokePaint (inert, 280, 96), "non-interactive card paints");
    }

    // --- Toast ---------------------------------------------------------------------------------
    {
        ds::ToastHost host;
        host.setBounds ({ 0, 0, 400, 300 });
        host.show (ds::ToastHost::Kind::success, "Recording saved");
        host.show (ds::ToastHost::Kind::error, "Couldn't export recording");
        ok &= expect (host.getNumChildComponents() == 2, "toasts stack");

        host.dismissAll();
        ok &= expect (host.getNumChildComponents() == 0, "toasts dismiss");
    }

    // --- Tag / Divider / Section / EmptyState ------------------------------------------------------
    {
        ds::Tag tag ("Lossless", ds::Tag::Variant::success);
        ok &= expect (smokePaint (tag, 90, 22), "tag paints");

        ds::Divider d;
        ok &= expect (smokePaint (d, 320, 1), "divider paints");

        ds::Section section ("Recording", "Microphone, countdown, format");
        ok &= expect (smokePaint (section, 320, 44), "section paints");

        ds::Button action ("Record", ds::ButtonVariant::primary);
        ds::EmptyState empty ({ ds::icons::play(), "No recordings yet.",
                                "Record something to see it here.", &action });
        ok &= expect (smokePaint (empty, 320, 160), "empty state paints");
    }

    // --- Runtime theme swap (M24 prep) ------------------------------------------------------------
    {
        int notifications = 0;
        struct L : juce::ChangeListener
        {
            int* count = nullptr;
            void changeListenerCallback (juce::ChangeBroadcaster*) override { (*count)++; }
        } listener;
        listener.count = &notifications;
        theme::themeChangedBroadcaster().addChangeListener (&listener);

        auto alt = theme::makeDefaultDarkTheme();
        alt.colors.accent = juce::Colours::lime;
        theme::setTheme (alt);
        ok &= expect (theme::current().colors.accent == juce::Colours::lime,
                      "setTheme swaps the active look");
        ok &= expect (theme::colors::accent() == juce::Colours::lime,
                      "token accessors read the new look");
        ok &= expect (notifications >= 1, "theme change broadcasts to listeners");

        ds::Button b ("Recolor", ds::ButtonVariant::primary);
        ok &= expect (smokePaint (b), "component paints after theme swap");

        theme::setTheme (theme::makeDefaultDarkTheme()); // restore
        ok &= expect (theme::current().colors.accent == theme::makeDefaultDarkTheme().colors.accent,
                      "default theme restored");
        theme::themeChangedBroadcaster().removeChangeListener (&listener);
    }

    std::printf (ok ? "ALL DS COMPONENT TESTS PASSED\n" : "DS COMPONENT TESTS FAILED\n");
    return ok ? 0 : 1;
}
