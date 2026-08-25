#pragma once

#include "../Library/LibraryService.h"
#include "Components/DsButton.h"
#include "Components/DsCore.h"
#include "Components/DsSurfaces.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

/*
    HomeView — Otoha Studio home (M20).

    Answers three questions immediately:
      What can I do?        -> one dominant Record action
      What did I work on?   -> Recent (real library data, newest first)
      Where do I go next?   -> Quick actions (Library, Sound)

    Built from the M18 component kit (ds::Card / ds::EmptyState / ds::Button);
    all visuals come from OtohaTheme tokens. Pure navigation — no DSP, playback,
    or fake statistics live here. The Record action routes to the existing
    Record screen (M21 owns its redesign).
*/
class HomeView : public juce::Component
{
public:
    std::function<void()> onRecord;                            // -> Record screen
    std::function<void()> onViewLibrary;                       // -> Library screen
    std::function<void()> onViewSound;                         // -> Sound screen
    std::function<void (const otoha::MediaItem&)> onOpenItem;  // -> Editor

    explicit HomeView (LibraryService& library);

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Re-reads the newest recordings (called when the shell shows this view). */
    void refreshRecents();

private:
    void rebuildRecents();

    LibraryService& library;

    // Header
    juce::Label greeting;
    juce::Label tagline;

    // Primary action — a large interactive card, unmistakably first.
    otoha::ds::Card recordCard { "Record", true };
    juce::Label recordTitle  { {}, "Record" };
    juce::Label recordHint   { {}, "Start a new recording" };

    // Recent section
    juce::Label recentHeader { {}, "Recent" };

    struct RecentCard
    {
        otoha::MediaItem item;
        std::unique_ptr<otoha::ds::Card> card;
        juce::Label name;
        juce::Label meta;      // duration · friendly date
    };
    std::vector<std::unique_ptr<RecentCard>> recents;

    // Empty state (M18 pattern): icon + title + description.
    std::unique_ptr<otoha::ds::EmptyState> emptyState;

    // Quick actions
    juce::Label quickHeader { {}, "Quick actions" };
    otoha::ds::Card libraryCard { "Library", true };
    juce::Label libraryTitle { {}, "Library" };
    juce::Label libraryHint  { {}, "Browse all recordings" };
    otoha::ds::Card soundCard { "Sound", true };
    juce::Label soundTitle { {}, "Sound" };
    juce::Label soundHint  { {}, "Microphone & audio settings" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HomeView)
};
