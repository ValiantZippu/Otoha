#include "HomeView.h"

#include "../Core/RecordingSupport.h"

#include <algorithm>

/*
    Home implementation. Same AMOLED + restrained sakura language as Sound and
    the onboarding screen, so Studio and Sound read as one product (#2).
*/
HomeView::HomeView (LibraryService& lib) : library (lib)
{
    brand.setFont (juce::FontOptions (34.0f, juce::Font::bold));
    brand.setColour (juce::Label::textColourId, juce::Colours::white);
    brand.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (brand);

    subtitle.setFont (juce::FontOptions (15.0f));
    subtitle.setColour (juce::Label::textColourId, juce::Colour (0xff8a7a82));
    subtitle.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (subtitle);

    recordButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a1620));
    recordButton.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffff9ecf));
    recordButton.onClick = [this] { if (onRecord) onRecord(); };
    addAndMakeVisible (recordButton);

    recentHeader.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    recentHeader.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (recentHeader);

    emptyHint.setJustificationType (juce::Justification::centred);
    emptyHint.setColour (juce::Label::textColourId, juce::Colour (0xff8a7a82));
    addAndMakeVisible (emptyHint);

    viewLibraryButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff141414));
    viewLibraryButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    viewLibraryButton.onClick = [this] { if (onViewLibrary) onViewLibrary(); };
    addAndMakeVisible (viewLibraryButton);

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
        row.openButton->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff141414));
        row.openButton->setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        row.openButton->setTooltip ("Open in editor");
        row.openButton->onClick = [this, id = item.id]
        {
            if (onOpenItem)
                onOpenItem (library.get (id));   // fresh lookup: survives renames
        };
        addAndMakeVisible (*row.openButton);

        row.duration.setText (otoha::formatDuration (item.durationSeconds),
                              juce::dontSendNotification);
        row.duration.setColour (juce::Label::textColourId, juce::Colour (0xff8a7a82));
        row.duration.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (row.duration);

        recents.push_back (std::move (row));
    }

    emptyHint.setVisible (recents.empty());
    resized();
}

void HomeView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff000000));

    juce::ColourGradient gradient (juce::Colour (0x30ff9ecf), (float) getWidth() * 0.25f, 0.0f,
                                   juce::Colour (0x08ff9ecf), (float) getWidth() * 0.75f, 160.0f, false);
    g.setGradientFill (gradient);
    g.fillRect (0, 0, getWidth(), 160);
}

void HomeView::resized()
{
    auto bounds = getLocalBounds().reduced (24);
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
