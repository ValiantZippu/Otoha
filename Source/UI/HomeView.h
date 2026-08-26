#pragma once

#include "../Library/LibraryService.h"
#include "Components/DsButton.h"
#include "Components/DsCore.h"
#include "Components/DsSurfaces.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

/*
    HomeView — Otoha Studio home (M20 → M29 Kaiteyo upgrade).

    Answers three questions immediately:
      What can I do?        -> one dominant Start Recording action
      What did I work on?   -> Recent recordings (real library data)
      Where do I go next?   -> Quick actions + View all

    M29 upgrade: Kaiteyo-aligned dashboard layout with:
      - Hero card with heading + supporting text + primary action
      - Quick actions row (Record, Library, Sound)
      - Stat tiles (Recordings, Total Time)
      - Recent recordings section with "View all" link
      - Proper section headers (DsSection)
      - Empty state with action button
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

    // --- Hero card (Kaiteyo: ContinueHero / WelcomeHero pattern) ----------------
    otoha::ds::Card heroCard { "Start recording", true };
    juce::Label heroHeading;      // "Record something great"
    juce::Label heroSubtext;     // "Clean recording, simple editing, one-tap Enhance."
    otoha::ds::Button startRecordBtn { "Start recording", otoha::ds::ButtonVariant::primary,
                                       otoha::ds::ButtonSize::large };

    // --- Quick actions (Kaiteyo: horizontal action row) -------------------------
    juce::Label quickHeader;      // "Quick actions"
    otoha::ds::Button recordQuickBtn  { "Record",  otoha::ds::ButtonVariant::accentTint,
                                        otoha::ds::ButtonSize::small };
    otoha::ds::Button libraryQuickBtn { "Library", otoha::ds::ButtonVariant::secondary,
                                        otoha::ds::ButtonSize::small };
    otoha::ds::Button soundQuickBtn   { "Sound",   otoha::ds::ButtonVariant::secondary,
                                        otoha::ds::ButtonSize::small };

    // --- Stat tiles (Kaiteyo: DsStatTile row) ----------------------------------
    juce::Label statsHeader;      // "Overview"
    otoha::ds::StatTile recordingsStat { "Recordings", "0" };
    otoha::ds::StatTile totalTimeStat  { "Total time", "0:00" };
    otoha::ds::StatTile lastRecordStat { "Last recording", "—" };

    // --- Recent recordings section ----------------------------------------------
    juce::Label recentHeader;     // "Recent recordings"
    otoha::ds::TextButton viewAllBtn { "View all" };

    struct RecentEntry
    {
        otoha::MediaItem item;
        std::unique_ptr<otoha::ds::Card> card;
        juce::Label name;
        juce::Label meta;      // duration · date
    };
    std::vector<std::unique_ptr<RecentEntry>> recents;

    // --- Empty state ------------------------------------------------------------
    otoha::ds::EmptyState::Setup emptySetup;
    std::unique_ptr<otoha::ds::EmptyState> emptyState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HomeView)
};
