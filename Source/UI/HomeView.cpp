#include "HomeView.h"

#include "OtohaTheme.h"
#include "Components/DsNavigation.h"

#include "../Core/RecordingSupport.h"

/*
    Studio home implementation (M20). Everything consumes OtohaTheme tokens and
    the M18 component kit — no local styling, no fake data. Hierarchy:
    Record (primary) → Recent (secondary) → Quick actions (tertiary).
*/
HomeView::HomeView (LibraryService& lib) : library (lib)
{
    // --- header ---------------------------------------------------------------
    greeting.setFont (otoha::theme::font (otoha::theme::TextSize::display));
    greeting.setColour (juce::Label::textColourId, otoha::theme::colors::textPrimary());
    greeting.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (greeting);

    tagline.setFont (otoha::theme::font (otoha::theme::TextSize::body));
    tagline.setColour (juce::Label::textColourId, otoha::theme::colors::textMuted());
    tagline.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (tagline);

    // Quiet time-of-day greeting; neutral fallback is fine without any account system.
    const auto hour = juce::Time::getCurrentTime().getHours();
    greeting.setText (hour < 12 ? "Good morning"
                    : hour < 18 ? "Good afternoon"
                                : "Good evening",
                      juce::dontSendNotification);
    tagline.setText ("Ready when you are.", juce::dontSendNotification);

    // --- primary Record action --------------------------------------------------
    recordCard.setProminent (true);
    otoha::theme::label (recordCard, "Record", "Start a new recording");
    recordCard.onClick = [this] { if (onRecord) onRecord(); };
    addAndMakeVisible (recordCard);

    recordTitle.setFont (otoha::theme::font (otoha::theme::TextSize::title));
    recordTitle.setColour (juce::Label::textColourId, otoha::theme::colors::accent());
    recordTitle.setJustificationType (juce::Justification::centredLeft);
    recordTitle.setInterceptsMouseClicks (false, false);
    recordTitle.setText ("Record", juce::dontSendNotification);
    recordCard.addAndMakeVisible (recordTitle);

    recordHint.setFont (otoha::theme::font (otoha::theme::TextSize::caption));
    recordHint.setColour (juce::Label::textColourId, otoha::theme::colors::textMuted());
    recordHint.setJustificationType (juce::Justification::centredLeft);
    recordHint.setInterceptsMouseClicks (false, false);
    recordHint.setText ("Tap to start a new recording", juce::dontSendNotification);
    recordCard.addAndMakeVisible (recordHint);

    // --- Recent section ----------------------------------------------------------
    recentHeader.setFont (otoha::theme::font (otoha::theme::TextSize::heading));
    recentHeader.setColour (juce::Label::textColourId, otoha::theme::colors::textPrimary());
    addAndMakeVisible (recentHeader);

    // --- empty state --------------------------------------------------------------
    otoha::ds::EmptyState::Setup emptySetup;
    emptySetup.icon        = otoha::icons::microphone();
    emptySetup.title       = "No recordings yet.";
    emptySetup.description = "Record something and it will appear here.";
    emptyState = std::make_unique<otoha::ds::EmptyState> (emptySetup);
    addAndMakeVisible (*emptyState);

    // --- quick actions ---------------------------------------------------------------
    quickHeader.setFont (otoha::theme::font (otoha::theme::TextSize::heading));
    quickHeader.setColour (juce::Label::textColourId, otoha::theme::colors::textPrimary());
    addAndMakeVisible (quickHeader);

    otoha::theme::label (libraryCard, "Library", "Browse all recordings");
    libraryCard.onClick = [this] { if (onViewLibrary) onViewLibrary(); };
    addAndMakeVisible (libraryCard);
    libraryTitle.setFont (otoha::theme::font (otoha::theme::TextSize::bodySmall));
    libraryTitle.setColour (juce::Label::textColourId, otoha::theme::colors::textPrimary());
    libraryTitle.setInterceptsMouseClicks (false, false);
    libraryCard.addAndMakeVisible (libraryTitle);
    libraryHint.setFont (otoha::theme::font (otoha::theme::TextSize::caption));
    libraryHint.setColour (juce::Label::textColourId, otoha::theme::colors::textMuted());
    libraryHint.setInterceptsMouseClicks (false, false);
    libraryCard.addAndMakeVisible (libraryHint);

    otoha::theme::label (soundCard, "Sound", "Microphone and audio settings");
    soundCard.onClick = [this] { if (onViewSound) onViewSound(); };
    addAndMakeVisible (soundCard);
    soundTitle.setFont (otoha::theme::font (otoha::theme::TextSize::bodySmall));
    soundTitle.setColour (juce::Label::textColourId, otoha::theme::colors::textPrimary());
    soundTitle.setInterceptsMouseClicks (false, false);
    soundCard.addAndMakeVisible (soundTitle);
    soundHint.setFont (otoha::theme::font (otoha::theme::TextSize::caption));
    soundHint.setColour (juce::Label::textColourId, otoha::theme::colors::textMuted());
    soundHint.setInterceptsMouseClicks (false, false);
    soundCard.addAndMakeVisible (soundHint);

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
        auto rc = std::make_unique<RecentCard>();
        rc->item = item;

        const auto name = item.displayName;
        rc->card = std::make_unique<otoha::ds::Card> (name, true);
        otoha::theme::label (*rc->card,
                             name + ", " + otoha::formatDuration (item.durationSeconds)
                                 + ", " + otoha::friendlyRelativeDate (item.createdAt),
                             "Open in editor");
        rc->card->onClick = [this, id = item.id]
        {
            if (onOpenItem)
                onOpenItem (library.get (id));   // fresh lookup: survives renames
        };
        addAndMakeVisible (*rc->card);

        rc->name.setFont (otoha::theme::font (otoha::theme::TextSize::bodySmall));
        rc->name.setColour (juce::Label::textColourId, otoha::theme::colors::textPrimary());
        rc->name.setText (name, juce::dontSendNotification);
        rc->name.setJustificationType (juce::Justification::centredLeft);
        rc->name.setMinimumHorizontalScale (0.8f);
        rc->name.setInterceptsMouseClicks (false, false);
        rc->card->addAndMakeVisible (rc->name);

        rc->meta.setFont (otoha::theme::font (otoha::theme::TextSize::caption));
        rc->meta.setColour (juce::Label::textColourId, otoha::theme::colors::textMuted());
        rc->meta.setText (otoha::formatDuration (item.durationSeconds) + juce::String ("  ·  ")
                              + otoha::friendlyRelativeDate (item.createdAt),
                          juce::dontSendNotification);
        rc->meta.setJustificationType (juce::Justification::centredRight);
        rc->meta.setInterceptsMouseClicks (false, false);
        rc->card->addAndMakeVisible (rc->meta);

        recents.push_back (std::move (rc));
    }

    emptyState->setVisible (recents.empty());
    resized();
}

void HomeView::paint (juce::Graphics& g)
{
    g.fillAll (otoha::theme::colors::background());

    // Quiet accent glow behind the hero — derived from the active accent token.
    juce::ColourGradient gradient (otoha::theme::colors::accent().withAlpha (0.16f),
                                   (float) getWidth() * 0.25f, 0.0f,
                                   otoha::theme::colors::accent().withAlpha (0.02f),
                                   (float) getWidth() * 0.75f, 200.0f, false);
    g.setGradientFill (gradient);
    g.fillRect (0, 0, getWidth(), 200);
}

void HomeView::resized()
{
    auto bounds = getLocalBounds().reduced (otoha::theme::Spacing::xl);
    const int maxContentW = 720;                       // comfortable reading width
    auto content = bounds.withSizeKeepingCentre (
        juce::jmin (maxContentW, bounds.getWidth()), bounds.getHeight());

    const int cardH = otoha::theme::Metrics::touchTargetMin + 24;

    // header
    greeting.setBounds (content.removeFromTop (44));
    tagline.setBounds  (content.removeFromTop (22));
    content.removeFromTop (otoha::theme::Spacing::lg);

    // primary Record card
    {
        auto r = content.removeFromTop (cardH + 20);
        recordCard.setBounds (r);
        auto inner = recordCard.getLocalBounds().reduced (otoha::theme::Metrics::cardPadding);
        recordTitle.setBounds (inner.removeFromTop (26));
        recordHint.setBounds  (inner.removeFromTop (18).withTrimmedLeft (2));
    }
    content.removeFromTop (otoha::theme::Spacing::xl);

    // Recent section
    if (! recents.empty())
    {
        recentHeader.setBounds (content.removeFromTop (24));
        content.removeFromTop (otoha::theme::Spacing::sm);

        const int cols = juce::jmax (1, juce::jmin (3, content.getWidth() / 220));
        const int gap  = otoha::theme::Spacing::sm;
        const int colW = (content.getWidth() - (cols - 1) * gap) / cols;
        int col = 0, rowY = 0;
        for (auto& rc : recents)
        {
            if (col == 0)
                rowY = content.removeFromTop (cardH).getY();
            rc->card->setBounds ({ content.getX() + col * (colW + gap), rowY, colW, cardH });
            auto inner = rc->card->getLocalBounds().reduced (otoha::theme::Metrics::cardPadding - 4);
            rc->meta.setBounds (inner.removeFromBottom (16));
            rc->name.setBounds (inner.withHeight (20));
            col = (col + 1) % cols;
            if (col == 0)
                content.removeFromTop (gap);   // row gap after a full row
        }
        content.removeFromTop (otoha::theme::Spacing::lg);
    }
    else
    {
        recentHeader.setVisible (false);
        emptyState->setBounds (content.removeFromTop (180));
        content.removeFromTop (otoha::theme::Spacing::lg);
    }
    if (recents.empty())
        recentHeader.setVisible (false);
    else
        recentHeader.setVisible (true);

    // Quick actions
    quickHeader.setBounds (content.removeFromTop (24));
    content.removeFromTop (otoha::theme::Spacing::sm);
    {
        const int halfGap = otoha::theme::Spacing::sm / 2;
        auto left  = content.removeFromLeft ((content.getWidth() - otoha::theme::Spacing::sm) / 2);
        content.removeFromLeft (otoha::theme::Spacing::sm);
        auto right = content;
        libraryCard.setBounds (left.removeFromTop (64));
        soundCard.setBounds  (right.removeFromTop (64));

        auto styleInner = [] (juce::Label& title, juce::Label& hint, juce::Rectangle<int> area)
        {
            title.setBounds (area.removeFromTop (20));
            hint.setBounds (area.removeFromTop (16));
        };
        styleInner (libraryTitle, libraryHint,
                    libraryCard.getLocalBounds().reduced (otoha::theme::Spacing::md)
                               .withTrimmedTop (6));
        styleInner (soundTitle, soundHint,
                    soundCard.getLocalBounds().reduced (otoha::theme::Spacing::md)
                             .withTrimmedTop (6));
    }
}
