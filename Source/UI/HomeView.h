#pragma once

#include "../Library/LibraryService.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

/*
    HomeView — Otoha's Studio landing screen (Milestone 11 #2/#3).

        OTOHA
        [ ●  Record ]
        Recent
          Interview      02:31
          Idea           00:48
          Voice memo     01:12
        [ View Library ]

    Information hierarchy over decoration: the two things a user does most —
    record something, or get back into the last thing they recorded — are one
    click away from launch. Pure navigation; no DSP, no playback lives here.
*/
class HomeView : public juce::Component
{
public:
    std::function<void()> onRecord;                       // -> Recording screen
    std::function<void()> onViewLibrary;                  // -> Library screen
    std::function<void (const otoha::MediaItem&)> onOpenItem;   // -> Editor

    explicit HomeView (LibraryService& library);

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Re-reads the newest recordings (called when the shell shows this view). */
    void refreshRecents();

private:
    void rebuildRecentRows();

    LibraryService& library;

    juce::Label brand { {}, "OTOHA" };
    juce::Label subtitle { {}, "Studio" };
    juce::TextButton recordButton { "●  Record" };
    juce::Label recentHeader { {}, "Recent" };
    juce::Label emptyHint { {}, "No recordings yet.\nYour first recording will appear here." };

    struct RecentRow
    {
        otoha::MediaItem item;
        std::unique_ptr<juce::TextButton> openButton;
        juce::Label duration;
    };
    std::vector<RecentRow> recents;

    juce::TextButton viewLibraryButton { "View Library" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HomeView)
};
