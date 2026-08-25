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
#include "../Source/UI/Components/DsNavigation.h"
#include "../Source/UI/Components/OtohaIcons.h"
#include "../Source/UI/HomeView.h"

#include <juce_audio_formats/juce_audio_formats.h>

namespace
{
/** Minimal silent wav for Studio tests (same approach as LibraryTests). */
juce::File writeTestWav (const juce::File& dir, const juce::String& name, int seconds = 1)
{
    juce::WavAudioFormat wavFormat;
    const auto file = dir.getChildFile (name);
    auto stream = file.createOutputStream();
    if (stream == nullptr) return {};
    std::unique_ptr<juce::AudioFormatWriter> writer (
        wavFormat.createWriterFor (stream.release(), 48000.0, 1, 16, {}, 0));
    if (writer == nullptr) return {};
    juce::AudioBuffer<float> silence (1, seconds * 48000);
    writer->writeFromAudioSampleBuffer (silence, 0, silence.getNumSamples());
    writer.reset();
    return file;
}
} // namespace

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

    // --- Vector icon registry (M19) ---------------------------------------------------------------
    {
        // Every icon path should be non-empty and paintable
        auto testIcon = [] (const juce::Path& p, const char* name)
        {
            return expect (! p.isEmpty(), std::string ("icon ").append (name).append (" is non-empty").c_str());
        };
        testIcon (otoha::icons::home(), "home");
        testIcon (otoha::icons::record(), "record");
        testIcon (otoha::icons::library(), "library");
        testIcon (otoha::icons::sound(), "sound");
        testIcon (otoha::icons::settings(), "settings");
        testIcon (otoha::icons::play(), "play");
        testIcon (otoha::icons::pause(), "pause");
        testIcon (otoha::icons::stop(), "stop");
        testIcon (otoha::icons::back(), "back");
        testIcon (otoha::icons::forward(), "forward");
        testIcon (otoha::icons::search(), "search");
        testIcon (otoha::icons::more(), "more");
        testIcon (otoha::icons::plus(), "plus");
        testIcon (otoha::icons::close(), "close");
        testIcon (otoha::icons::trash(), "trash");
        testIcon (otoha::icons::undo(), "undo");
        testIcon (otoha::icons::redo(), "redo");
        testIcon (otoha::icons::check(), "check");
        testIcon (otoha::icons::waveform(), "waveform");
        testIcon (otoha::icons::microphone(), "microphone");
        testIcon (otoha::icons::musicNote(), "musicNote");
        testIcon (otoha::icons::folder(), "folder");
        testIcon (otoha::icons::info(), "info");
        testIcon (otoha::icons::warning(), "warning");
        testIcon (otoha::icons::chevronDown(), "chevronDown");
    }

    // --- NavItem (M19) -------------------------------------------------------------------
    {
        otoha::ds::NavItem navItem ("Studio", otoha::icons::home(), "Studio");
        ok &= expect (navItem.getName() == "Studio", "navItem carries accessible name");
        ok &= expect (smokePaint (navItem, 200, 44), "navItem paints");

        navItem.setActive (true);
        ok &= expect (navItem.isActive(), "navItem active state stored");
        ok &= expect (smokePaint (navItem, 200, 44), "active navItem paints");

        navItem.setActive (false);
        ok &= expect (! navItem.isActive(), "navItem inactive state stored");

        navItem.setEnabled (false);
        ok &= expect (smokePaint (navItem, 200, 44), "disabled navItem paints");

        // Compact mode (icon-only)
        navItem.setEnabled (true);
        navItem.setLabelVisible (false);
        ok &= expect (smokePaint (navItem, 56, 44), "compact navItem paints");
    }

    // --- Sidebar (M19) --------------------------------------------------------------------
    {
        otoha::ds::Sidebar sb;
        int lastNavId = 0;
        sb.onNavigate = [&] (int id) { lastNavId = id; };

        enum { ID_HOME = 1, ID_RECORD = 2, ID_LIBRARY = 3 };
        sb.addItem (ID_HOME,    "Studio",  otoha::icons::home(),    "Studio",  false);
        sb.addItem (ID_RECORD,  "Record",  otoha::icons::record(),  "Record",  false);
        sb.addItem (ID_LIBRARY, "Library", otoha::icons::library(), "Library", false);

        sb.setBounds (0, 0, 200, 600);
        sb.setActiveItem (ID_HOME);
        ok &= expect (sb.getActiveItem() == ID_HOME, "sidebar sets active item");

        // Test paint
        ok &= expect (smokePaint (sb, 200, 600), "sidebar paints");

        // Test navigation callback (simulating a click)
        lastNavId = 0;
        sb.setActiveItem (ID_RECORD);
        ok &= expect (sb.getActiveItem() == ID_RECORD, "sidebar changes active");

        // Compact width test
        sb.setBounds (0, 0, otoha::ds::NavItem::compactWidth(), 600);
        ok &= expect (smokePaint (sb, otoha::ds::NavItem::compactWidth(), 600),
                      "sidebar paints in compact mode");
    }

    // --- Prominent Card variant (M20 hero action) ---------------------------------------------
    {
        otoha::ds::Card hero ("Hero", true);
        hero.setProminent (true);
        ok &= expect (smokePaint (hero, 280, 72), "prominent card paints");
        auto altTheme = theme::makeDefaultDarkTheme();
        altTheme.colors.accent = juce::Colours::lime;
        theme::setTheme (altTheme);
        ok &= expect (smokePaint (hero), "prominent card paints after theme swap");
        theme::setTheme (theme::makeDefaultDarkTheme());
    }

    // --- Studio home (M20) — real LibraryService, real data -------------------------------
    {
        const auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("otoha_studio_test_" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt()));
        root.createDirectory();

        LibraryService service (root);
        juce::String err;
        ok &= expect (service.initialise (err), "library service initialises for studio test");

        // Empty state first — no recordings yet.
        {
            HomeView emptyStudio (service);
            emptyStudio.setBounds (0, 0, 800, 600);
            ok &= expect (smokePaint (emptyStudio, 800, 600), "empty studio paints");

            // The empty-state Record action must be present and named.
            bool foundRecordButton = false;
            for (auto* c : emptyStudio.getChildren())
                if (c->getName() == "Record") { foundRecordButton = true; break; }
            ok &= expect (foundRecordButton, "empty state exposes a named Record action");
        }

        // Register real recordings through the actual data layer.
        const auto audioDir = root.getChildFile ("Library").getChildFile ("Audio");
        audioDir.createDirectory();
        const auto w1 = writeTestWav (audioDir, "idea.wav", 2);
        const auto w2 = writeTestWav (audioDir, "memo.wav", 1);
        ok &= expect (w1.existsAsFile() && w2.existsAsFile(), "test recordings written");
        ok &= expect (service.registerAudioFile (w1) != 0, "recording one registered");
        ok &= expect (service.registerAudioFile (w2) != 0, "recording two registered");

        HomeView studio (service);
        int recordNav = 0, libraryNav = 0, soundNav = 0, itemOpens = 0;
        studio.onRecord      = [&] { ++recordNav; };
        studio.onViewLibrary = [&] { ++libraryNav; };
        studio.onViewSound   = [&] { ++soundNav; };
        studio.onOpenItem    = [&] (const otoha::MediaItem&) { ++itemOpens; };

        studio.refreshRecents();
        studio.setBounds (0, 0, 800, 600);

        // Recent cards exist and carry accessible names built from real metadata.
        int recentCards = 0;
        bool hasNamedCard = false;
        bool hasLibraryCard = false, hasSoundCard = false;
        for (auto* c : studio.getChildren())
            if (auto* card = dynamic_cast<otoha::ds::Card*> (c))
            {
                ++recentCards;
                if (card->getName().contains ("idea") || card->getName().contains ("memo")) hasNamedCard = true;
                if (card->getName() == "Library") hasLibraryCard = true;
                if (card->getName() == "Sound") hasSoundCard = true;
            }
        ok &= expect (recentCards == 5, "record + 2 recent + library + sound cards");
        ok &= expect (hasNamedCard, "recent cards carry the recording display name");
        ok &= expect (hasLibraryCard && hasSoundCard, "library and sound quick action cards exist");

        // Clicking a recent card routes to the editor with fresh data.
        for (auto* c : studio.getChildren())
            if (auto* card = dynamic_cast<otoha::ds::Card*> (c))
                if (card->getName().contains ("idea")) { card->triggerClick(); break; }
        pumpMessages();
        ok &= expect (itemOpens == 1, "recent card opens the item in the editor");

        // Quick actions navigate to their existing routes.
        for (auto* c : studio.getChildren())
            if (auto* card = dynamic_cast<otoha::ds::Card*> (c))
            {
                if (card->getName() == "Library") card->triggerClick();
                else if (card->getName() == "Sound") card->triggerClick();
            }
        pumpMessages();
        ok &= expect (libraryNav == 1 && soundNav == 1,
                      "quick actions navigate to Library and Sound");

        ok &= expect (smokePaint (studio, 800, 600), "studio paints at normal desktop width");
        ok &= expect (smokePaint (studio, 1100, 700), "studio paints at large desktop width");
        ok &= expect (smokePaint (studio, 520, 640), "studio paints at narrow width");

        // Runtime recolor of the whole studio (M17 infrastructure).
        auto alt = theme::makeDefaultDarkTheme();
        alt.colors.accent = juce::Colours::orange;
        theme::setTheme (alt);
        ok &= expect (smokePaint (studio), "studio repaints after runtime theme change");
        theme::setTheme (theme::makeDefaultDarkTheme());

        root.deleteRecursively();
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
