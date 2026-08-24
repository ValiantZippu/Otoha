#include "HomeView.h"

#include "OtohaTheme.h"

#include "../Core/RecordingSupport.h"

#include <algorithm>

/*
    Home implementation. Same AMOLED + restrained sakura language as Sound and
    the onboarding screen, so Studio and Sound read as one product (#2).
    M14: all styling flows from otoha::theme — this file is the reference.
*/
HomeView::HomeView (LibraryService& lib) : library (lib)
{
    brand.setFont (otoha::theme::font (otoha::theme::TextSize::display));
    brand.setColour (juce::Label::textColourId, otoha::theme::textPrimary());
    brand.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (brand);

    subtitle.setFont (otoha::theme::font (otoha::theme::TextSize::body));
    subtitle.setColour (juce::Label::textColourId, otoha::theme::textMuted());
    subtitle.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (subtitle);

    // The one dominant action on this screen (#5): clear states, accessible name,
    // and a tooltip. JUCE supplies hover/pressed/down visuals from the colours.
    otoha::theme::stylePrimaryButton (recordButton);
    recordButton.setButtonText ("●  Record");
    otoha::theme::label (recordButton, "Record",
                         "Start recording with the selected microphone");
    recordButton.onClick = [this] { if (onRecord) onRecord(); };
    addAndMakeVisible (recordButton);

    recentHeader.setFont (otoha::theme::font (otoha::theme::TextSize::section));
    recentHeader.setColour (juce::Label::textColourId, otoha::theme::textPrimary());
    recentHeader.setText ("Recent", juce::dontSendNotification);
    addAndMakeVisible (recentHeader);

    emptyHint.setJustificationType (juce::Justification::centred);
    emptyHint.setColour (juce::Label::textColourId, otoha::theme::textMuted());
    emptyHint.setText ("No recordings yet.\nPress Record to make your first one.",
                       juce::dontSendNotification);
    addAndMakeVisible (emptyHint);

    otoha::theme::styleCardButton (viewLibraryButton);
    viewLibraryButton.setButtonText ("View library →");
    otoha::theme::label (viewLibraryButton, "View library", "Open your recordings");
    viewLibraryButton.onClick = [this] { if (onViewLibrary) onViewLibrary(); };
    addAndMakeVisible (viewLibraryButton);

    // #4: a quiet time-of-day greeting — friendly without being chatty.
    const auto hour = juce::Time::getCurrentTime().getHours();
    subtitle.setText (hour < 12 ? "Good morning"
                    : hour < 18 ? "Good afternoon"
                                : "Good evening",
                      juce::dontSendNotification);

    refreshRecents();
}

void HomeView::refreshRecents()
{
    auto newest = library.query ({}, otoha::LibraryFilter::audio,
                                 otoha::LibrarySort::newestFirst);
    if ((int) newest.size() > 5)
        newest.resize (5);   // Recent means recent — the Library holds everything

    recents.clear();
    for (const auto& item : newest)
    {
        RecentRow row;
        row.item = item;

        row.openButton = std::make_unique<juce::TextButton> (item.displayName);
        otoha::theme::styleCardButton (*row.openButton);
        row.openButton->setTooltip ("Open in editor");
        row.openButton->onClick = [this, id = item.id]
        {
            if (onOpenItem)
                onOpenItem (library.get (id));   // fresh lookup: survives renames
        };
        addAndMakeVisible (*row.openButton);

        row.duration.setFont (otoha::theme::font (otoha::theme::TextSize::caption));
        row.duration.setText (otoha::formatDuration (item.durationSeconds),
                              juce::dontSendNotification);
        row.duration.setColour (juce::Label::textColourId, otoha::theme::textMuted());
        row.duration.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (row.duration);

        recents.push_back (std::move (row));
    }

    emptyHint.setVisible (recents.empty());
    resized();
}

void HomeView::paint (juce::Graphics& g)
{
    g.fillAll (otoha::theme::background());

    juce::ColourGradient gradient (juce::Colour (0x30ff9ecf), (float) getWidth() * 0.25f, 0.0f,
                                   juce::Colour (0x08ff9ecf), (float) getWidth() * 0.75f, 160.0f, false);
    g.setGradientFill (gradient);
    g.fillRect (0, 0, getWidth(), 160);
}

void HomeView::resized()
{
    auto bounds = getLocalBounds().reduced (otoha::theme::edgePadding);
    const int centreW = juce::jmin (460, bounds.getWidth());

    brand.setBounds    (bounds.removeFromTop (52));
    subtitle.setBounds (bounds.removeFromTop (22));
    bounds.removeFromTop (18);

    auto centre = bounds.withSizeKeepingCentre (centreW, bounds.getHeight());
    recordButton.setBounds (centre.removeFromTop (52).withSizeKeepingCentre (220, 48));
    centre.removeFromTop (28);

    recentHeader.setBounds (centre.removeFromTop (26));
    centre.removeFromTop (6);

    if (recents.empty())
    {
        emptyHint.setBounds (centre.removeFromTop (56));
    }
    else
    {
        for (auto& row : recents)
        {
            auto r = centre.removeFromTop (40);
            r.removeFromLeft (8);
            row.duration.setBounds (r.removeFromRight (64));
            row.openButton->setBounds (r.withTrimmedRight (8));
            centre.removeFromTop (4);
        }
    }

    centre.removeFromTop (14);
    viewLibraryButton.setBounds (centre.removeFromTop (38).withSizeKeepingCentre (180, 34));
}
