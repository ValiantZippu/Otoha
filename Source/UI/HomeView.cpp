#include "HomeView.h"

#include "OtohaTheme.h"
#include "Components/DsNavigation.h"

#include "../Core/RecordingSupport.h"

/*
    Studio home implementation (M20 → M29 Kaiteyo upgrade).

    Layout hierarchy (top to bottom, matching Kaiteyo DashboardView):
      1. Hero card — primary Start Recording action
      2. Quick actions row — Record, Library, Sound
      3. Stat tiles — Recordings, Total Time, Last Recording
      4. Recent recordings — card grid with "View all" link
      5. Empty state (when no recordings)

    All visuals use OtohaTheme tokens. No fake data. No DSP.
*/

HomeView::HomeView (LibraryService& lib) : library (lib)
{
    // =========================================================================
    // Hero card
    // =========================================================================
    heroCard.setProminent (true);
    otoha::theme::label (heroCard, "Start recording", "Open the Record screen");
    heroCard.onClick = [this] { if (onRecord) onRecord(); };
    addAndMakeVisible (heroCard);

    heroHeading.setFont (otoha::theme::font (otoha::theme::TextSize::heading));
    heroHeading.setColour (juce::Label::textColourId, otoha::theme::colors::textPrimary());
    heroHeading.setJustificationType (juce::Justification::centredLeft);
    heroHeading.setText ("Record something great", juce::dontSendNotification);
    heroHeading.setInterceptsMouseClicks (false, false);
    heroCard.addAndMakeVisible (heroHeading);

    heroSubtext.setFont (otoha::theme::font (otoha::theme::TextSize::body));
    heroSubtext.setColour (juce::Label::textColourId, otoha::theme::colors::textMuted());
    heroSubtext.setJustificationType (juce::Justification::centredLeft);
    heroSubtext.setText ("Clean recording, simple editing, one-tap Enhance.", juce::dontSendNotification);
    heroSubtext.setInterceptsMouseClicks (false, false);
    heroCard.addAndMakeVisible (heroSubtext);

    otoha::theme::label (startRecordBtn, "Start recording", "Navigate to the Record screen");
    startRecordBtn.setIcon (otoha::icons::record());
    startRecordBtn.onClick = [this] { if (onRecord) onRecord(); };
    heroCard.addAndMakeVisible (startRecordBtn);

    // =========================================================================
    // Quick actions
    // =========================================================================
    quickHeader.setFont (otoha::theme::font (otoha::theme::TextSize::caption));
    quickHeader.setColour (juce::Label::textColourId, otoha::theme::colors::textMuted());
    quickHeader.setText ("QUICK ACTIONS", juce::dontSendNotification);
    addAndMakeVisible (quickHeader);

    otoha::theme::label (recordQuickBtn, "Record", "Start a new recording");
    recordQuickBtn.setIcon (otoha::icons::record());
    recordQuickBtn.onClick = [this] { if (onRecord) onRecord(); };
    addAndMakeVisible (recordQuickBtn);

    otoha::theme::label (libraryQuickBtn, "Library", "Browse all recordings");
    libraryQuickBtn.setIcon (otoha::icons::library());
    libraryQuickBtn.onClick = [this] { if (onViewLibrary) onViewLibrary(); };
    addAndMakeVisible (libraryQuickBtn);

    otoha::theme::label (soundQuickBtn, "Sound", "Audio settings");
    soundQuickBtn.setIcon (otoha::icons::sound());
    soundQuickBtn.onClick = [this] { if (onViewSound) onViewSound(); };
    addAndMakeVisible (soundQuickBtn);

    // =========================================================================
    // Stat tiles
    // =========================================================================
    statsHeader.setFont (otoha::theme::font (otoha::theme::TextSize::caption));
    statsHeader.setColour (juce::Label::textColourId, otoha::theme::colors::textMuted());
    statsHeader.setText ("OVERVIEW", juce::dontSendNotification);
    addAndMakeVisible (statsHeader);

    addAndMakeVisible (recordingsStat);
    addAndMakeVisible (totalTimeStat);
    addAndMakeVisible (lastRecordStat);

    // =========================================================================
    // Recent recordings section
    // =========================================================================
    recentHeader.setFont (otoha::theme::font (otoha::theme::TextSize::heading));
    recentHeader.setColour (juce::Label::textColourId, otoha::theme::colors::textPrimary());
    recentHeader.setText ("Recent recordings", juce::dontSendNotification);
    addAndMakeVisible (recentHeader);

    otoha::theme::label (viewAllBtn, "View all", "Open the Library screen");
    viewAllBtn.onClick = [this] { if (onViewLibrary) onViewLibrary(); };
    addAndMakeVisible (viewAllBtn);

    // =========================================================================
    // Empty state
    // =========================================================================
    emptySetup.icon        = otoha::icons::microphone();
    emptySetup.title       = "No recordings yet";
    emptySetup.description = "Start your first recording and it will appear here.";
    emptySetup.action      = &startRecordBtn;  // reuse the hero button
    emptyState = std::make_unique<otoha::ds::EmptyState> (emptySetup);
    addAndMakeVisible (*emptyState);

    refreshRecents();
}

// =========================================================================
// Data refresh
// =========================================================================

void HomeView::refreshRecents()
{
    auto all = library.query ({}, otoha::LibraryFilter::audio,
                              otoha::LibrarySort::newestFirst);

    // --- stat tiles (real data) ------------------------------------------------
    recordingsStat.setValues (juce::String ((int) all.size()));

    double totalSecs = 0.0;
    for (const auto& item : all)
        totalSecs += item.durationSeconds;
    totalTimeStat.setValues (otoha::formatDuration (totalSecs));

    lastRecordStat.setValues (! all.empty()
                                 ? otoha::friendlyRelativeDate (all.front().createdAt)
                                 : juce::String ("—"));

    addAndMakeVisible (recordingsStat);
    addAndMakeVisible (totalTimeStat);
    addAndMakeVisible (lastRecordStat);

    // --- recent recordings (up to 6) -------------------------------------------
    const int maxRecent = juce::jmin (6, (int) all.size());
    std::vector<otoha::MediaItem> recent (all.begin(), all.begin() + maxRecent);

    recents.clear();
    for (const auto& item : recent)
    {
        auto entry = std::make_unique<RecentEntry>();
        entry->item = item;

        const auto name = item.displayName;
        entry->card = std::make_unique<otoha::ds::Card> (name, true);
        otoha::theme::label (*entry->card,
                             name + ", " + otoha::formatDuration (item.durationSeconds)
                                 + ", " + otoha::friendlyRelativeDate (item.createdAt),
                             "Open in editor");
        entry->card->onClick = [this, id = item.id]
        {
            if (onOpenItem)
                onOpenItem (library.get (id));
        };
        addAndMakeVisible (*entry->card);

        entry->name.setFont (otoha::theme::font (otoha::theme::TextSize::bodySmall));
        entry->name.setColour (juce::Label::textColourId, otoha::theme::colors::textPrimary());
        entry->name.setText (name, juce::dontSendNotification);
        entry->name.setJustificationType (juce::Justification::centredLeft);
        entry->name.setMinimumHorizontalScale (0.8f);
        entry->name.setInterceptsMouseClicks (false, false);
        entry->card->addAndMakeVisible (entry->name);

        entry->meta.setFont (otoha::theme::font (otoha::theme::TextSize::caption));
        entry->meta.setColour (juce::Label::textColourId, otoha::theme::colors::textMuted());
        entry->meta.setText (otoha::formatDuration (item.durationSeconds)
                                 + juce::String ("  ·  ")
                                 + otoha::friendlyRelativeDate (item.createdAt),
                             juce::dontSendNotification);
        entry->meta.setJustificationType (juce::Justification::centredRight);
        entry->meta.setInterceptsMouseClicks (false, false);
        entry->card->addAndMakeVisible (entry->meta);

        recents.push_back (std::move (entry));
    }

    // --- visibility toggles ----------------------------------------------------
    const bool hasRecordings = ! recents.empty();
    emptyState->setVisible (! hasRecordings);
    heroCard.setVisible (true);   // hero is always visible
    recentHeader.setVisible (hasRecordings);
    viewAllBtn.setVisible (hasRecordings);

    resized();
}

// =========================================================================
// Paint
// =========================================================================

void HomeView::paint (juce::Graphics& g)
{
    g.fillAll (otoha::theme::colors::background());

    // Subtle accent glow behind the hero (Kaiteyo: accent at low alpha)
    juce::ColourGradient gradient (otoha::theme::colors::accent().withAlpha (0.12f),
                                   (float) getWidth() * 0.2f, 0.0f,
                                   otoha::theme::colors::accent().withAlpha (0.01f),
                                   (float) getWidth() * 0.8f, 180.0f, false);
    g.setGradientFill (gradient);
    g.fillRect (0, 0, getWidth(), 180);
}

// =========================================================================
// Layout (Kaiteyo: Xl padding, Lg gaps, max 720 content width)
// =========================================================================

void HomeView::resized()
{
    auto bounds = getLocalBounds().reduced (otoha::theme::Spacing::xl);
    const int maxW = 720;
    auto content = bounds.withSizeKeepingCentre (
        juce::jmin (maxW, bounds.getWidth()), bounds.getHeight());

    const int gap = otoha::theme::Spacing::lg;
    const int cardH = 100;   // hero card height
    const int actionH = 36;
    const int recentH = 56;

    // --- Hero card -------------------------------------------------------------
    heroCard.setBounds (content.removeFromTop (cardH));
    {
        auto inner = heroCard.getLocalBounds().reduced (otoha::theme::Metrics::cardPadding);
        heroHeading.setBounds (inner.removeFromTop (24));
        inner.removeFromTop (4);
        heroSubtext.setBounds (inner.removeFromTop (18));
        inner.removeFromTop (8);
        startRecordBtn.setBounds (inner.getX(), inner.getBottom() - actionH,
                                  160, actionH);
    }
    content.removeFromTop (gap);

    // --- Quick actions ---------------------------------------------------------
    quickHeader.setBounds (content.removeFromTop (14));
    content.removeFromTop (otoha::theme::Spacing::xs);
    {
        const int btnW = 100;
        const int btnGap = otoha::theme::Spacing::sm;
        recordQuickBtn.setBounds (content.getX(), content.getY(), btnW, actionH);
        libraryQuickBtn.setBounds (content.getX() + btnW + btnGap, content.getY(), btnW, actionH);
        soundQuickBtn.setBounds (content.getX() + 2 * (btnW + btnGap), content.getY(), btnW, actionH);
    }
    content.removeFromTop (actionH + gap);

    // --- Stat tiles ------------------------------------------------------------
    statsHeader.setBounds (content.removeFromTop (14));
    content.removeFromTop (otoha::theme::Spacing::xs);
    {
        const int tileGap = otoha::theme::Spacing::sm;
        const int cols = 3;
        const int tileW = (content.getWidth() - (cols - 1) * tileGap) / cols;
        const int tileH = 72;

        recordingsStat.setBounds (content.getX(), content.getY(), tileW, tileH);
        totalTimeStat.setBounds (content.getX() + tileW + tileGap, content.getY(), tileW, tileH);
        lastRecordStat.setBounds (content.getX() + 2 * (tileW + tileGap), content.getY(), tileW, tileH);
    }
    content.removeFromTop (72 + gap);

    // --- Recent recordings -----------------------------------------------------
    if (recents.empty())
    {
        recentHeader.setVisible (false);
        viewAllBtn.setVisible (false);
        emptyState->setBounds (content.removeFromTop (200));
    }
    else
    {
        emptyState->setVisible (false);
        recentHeader.setVisible (true);
        viewAllBtn.setVisible (true);

        // header row: title on left, "View all" on right
        auto headerRow = content.removeFromTop (24);
        recentHeader.setBounds (headerRow.withWidth (200));
        viewAllBtn.setBounds (headerRow.getRight() - 80, headerRow.getY(), 80, 24);
        content.removeFromTop (otoha::theme::Spacing::sm);

        // responsive grid: 3 cols if wide enough, else 2, else 1
        const int minCardW = 200;
        const int gap2 = otoha::theme::Spacing::sm;
        const int cols = juce::jmax (1, juce::jmin (3, content.getWidth() / minCardW));
        const int colW = (content.getWidth() - (cols - 1) * gap2) / cols;

        int col = 0, rowY = content.getY();
        for (auto& entry : recents)
        {
            if (col == 0 && entry == recents.front())
                rowY = content.getY();

            entry->card->setBounds ({ content.getX() + col * (colW + gap2),
                                      rowY, colW, recentH });

            auto inner = entry->card->getLocalBounds()
                             .reduced (otoha::theme::Metrics::cardPadding - 4);
            entry->meta.setBounds (inner.removeFromBottom (14));
            entry->name.setBounds (inner.withHeight (18));

            ++col;
            if (col >= cols)
            {
                col = 0;
                rowY += recentH + gap2;
            }
        }
    }
}
