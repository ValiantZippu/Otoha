#include "SettingsView.h"
#include "OtohaTheme.h"
#include "Components/DsButton.h"
#include "Components/DsControls.h"
#include "Components/DsSurfaces.h"
#include "Components/DsResponsive.h"

using namespace otoha::theme;

// =============================================================================
// CategoryItem — rail row with vector icon + label
// =============================================================================
SettingsView::CategoryItem::CategoryItem (const juce::String& l, juce::Path icon, std::function<void()> cb)
    : label (l), iconPath (std::move (icon)), onSelect (std::move (cb))
{
    setMouseClickGrabsKeyboardFocus (true);
    setHelpText (l);
}

juce::Font SettingsView::CategoryItem::getLabelFont() const
{
    return font (selected ? TextSize::body : TextSize::bodySmall);
}

void SettingsView::CategoryItem::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const float r = (float) Radius::medium;

    // Background
    if (selected)
    {
        g.setColour (colors::accentSoft());
        g.fillRoundedRectangle (bounds, r);
    }
    else if (isMouseOver())
    {
        g.setColour (colors::surfaceHover());
        g.fillRoundedRectangle (bounds, r);
    }

    // Accent left indicator
    if (selected)
    {
        g.setColour (colors::accent());
        g.fillRoundedRectangle (bounds.getX(), bounds.getY() + 4.0f, 3.0f, bounds.getHeight() - 8.0f, 1.5f);
    }

    // Icon
    const float iconSize = 16.0f;
    auto iconArea = juce::Rectangle<float> (iconSize, iconSize)
        .withCentre ({ bounds.getX() + Spacing::lg + iconSize / 2.0f, bounds.getCentreY() });
    g.setColour (selected ? colors::accent() : colors::textSecondary());
    if (! iconPath.isEmpty())
        g.fillPath (iconPath, iconPath.getTransformToScaleToFit (iconArea, true));

    // Label
    g.setColour (selected ? colors::textPrimary() : colors::textSecondary());
    g.setFont (getLabelFont());
    g.drawText (label, bounds.withTrimmedLeft (Spacing::lg + iconSize + Spacing::md)
                              .withTrimmedRight (Spacing::md),
                juce::Justification::centredLeft);
}

void SettingsView::CategoryItem::mouseUp (const juce::MouseEvent&)
{
    if (onSelect) onSelect();
}

// =============================================================================
// AccentSwatch — preserved from M24
// =============================================================================
SettingsView::AccentSwatch::AccentSwatch (const AccentEntry& e, std::function<void()> onSelected)
    : entry (e), onSelect (std::move (onSelected))
{
    setHelpText (e.name);
    setMouseClickGrabsKeyboardFocus (true);
}

void SettingsView::AccentSwatch::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const float r = bounds.getWidth() * 0.5f;

    if (selected)
    {
        g.setColour (entry.base);
        g.drawRoundedRectangle (bounds.reduced (1.5f), r, 2.5f);
    }
    else
    {
        g.setColour (colors::border());
        g.drawRoundedRectangle (bounds.reduced (1.5f), r, 1.0f);
    }

    g.setColour (entry.base);
    g.fillEllipse (bounds.reduced (4.0f));

    if (selected)
    {
        g.setColour (entry.contrast);
        juce::Path check;
        const float cx = bounds.getCentreX();
        const float cy = bounds.getCentreY();
        const float s = bounds.getWidth() * 0.15f;
        check.startNewSubPath (cx - s * 1.2f, cy);
        check.lineTo (cx - s * 0.3f, cy + s * 1.0f);
        check.lineTo (cx + s * 1.2f, cy - s * 0.8f);
        g.strokePath (check, juce::PathStrokeType (2.0f));
    }
}

void SettingsView::AccentSwatch::mouseUp (const juce::MouseEvent&)
{
    if (onSelect) onSelect();
}

// =============================================================================
// SettingsView
// =============================================================================
SettingsView::SettingsView (otoha::AppSettings& s) : settings (s)
{
    setOpaque (true);

    // --- Header ---
    headerTitle.setText ("Settings", juce::dontSendNotification);
    headerTitle.setFont (font (TextSize::title));
    headerTitle.setColour (juce::Label::textColourId, colors::textPrimary());
    addAndMakeVisible (headerTitle);

    // --- Category rail ---
    searchField = std::make_unique<otoha::ds::SearchField> ("Search settings...");
    addAndMakeVisible (*searchField);

    // Category items with vector icons
    struct CatDef { const char* label; juce::Path (*iconFunc)(); Category cat; };
    CatDef defs[] = {
        { "General",    otoha::icons::settings, Category::general    },
        { "Appearance", otoha::icons::settings, Category::appearance },
        { "About",      otoha::icons::settings, Category::about      },
    };

    for (auto& d : defs)
    {
        auto* item = new CategoryItem (d.label, d.iconFunc(), [this, c = d.cat] { selectCategory (c); });
        addAndMakeVisible (item);
        categories.emplace_back (item);
    }

    // --- General panel ---
    generalHeader.setText ("General", juce::dontSendNotification);
    generalHeader.setFont (font (TextSize::heading));
    generalHeader.setColour (juce::Label::textColourId, colors::textPrimary());
    addAndMakeVisible (generalHeader);

    generalDesc.setText ("Recording defaults", juce::dontSendNotification);
    generalDesc.setFont (font (TextSize::caption));
    generalDesc.setColour (juce::Label::textColourId, colors::textMuted());
    addAndMakeVisible (generalDesc);

    countdownLabel.setText ("Countdown", juce::dontSendNotification);
    countdownLabel.setFont (font (TextSize::body));
    countdownLabel.setColour (juce::Label::textColourId, colors::textPrimary());
    addAndMakeVisible (countdownLabel);

    countdownDesc.setText ("Delay before recording begins", juce::dontSendNotification);
    countdownDesc.setFont (font (TextSize::caption));
    countdownDesc.setColour (juce::Label::textColourId, colors::textMuted());
    addAndMakeVisible (countdownDesc);

    countdownCombo = std::make_unique<otoha::ds::ComboBox> ("Recording countdown");
    countdownCombo->addItem ("Off", 1);
    countdownCombo->addItem ("3 seconds", 2);
    countdownCombo->addItem ("5 seconds", 3);
    countdownCombo->addItem ("10 seconds", 4);
    {
        int idx = 2; // default 3 seconds
        if (settings.countdownSeconds == 0) idx = 1;
        else if (settings.countdownSeconds == 5) idx = 3;
        else if (settings.countdownSeconds == 10) idx = 4;
        countdownCombo->setSelectedItemIndex (idx - 1, juce::dontSendNotification);
    }
    countdownCombo->onChange = [this]
    {
        const int idx = countdownCombo->getSelectedItemIndex();
        static const int seconds[] = { 0, 3, 5, 10 };
        settings.countdownSeconds = seconds[juce::jlimit (0, 3, idx)];
        saveSettings();
    };
    addAndMakeVisible (*countdownCombo);

    // --- Appearance panel ---
    appearanceHeader.setText ("Appearance", juce::dontSendNotification);
    appearanceHeader.setFont (font (TextSize::heading));
    appearanceHeader.setColour (juce::Label::textColourId, colors::textPrimary());
    addAndMakeVisible (appearanceHeader);

    modeLabel.setText ("Mode", juce::dontSendNotification);
    modeLabel.setFont (font (TextSize::bodySmall));
    modeLabel.setColour (juce::Label::textColourId, colors::textSecondary());
    addAndMakeVisible (modeLabel);

    modeCombo = std::make_unique<otoha::ds::ComboBox> ("Appearance mode");
    modeCombo->addItem ("System", 1);
    modeCombo->addItem ("Light", 2);
    modeCombo->addItem ("Dark", 3);
    {
        int idx = 1;
        if (settings.appearanceMode.equalsIgnoreCase ("light")) idx = 2;
        else if (settings.appearanceMode.equalsIgnoreCase ("dark")) idx = 3;
        modeCombo->setSelectedItemIndex (idx - 1, juce::dontSendNotification);
    }
    modeCombo->onChange = [this]
    {
        const int idx = modeCombo->getSelectedItemIndex();
        static const char* modes[] = { "system", "light", "dark" };
        settings.appearanceMode = modes[juce::jlimit (0, 2, idx)];
        saveSettings();
        applyAppearance();
    };
    addAndMakeVisible (*modeCombo);

    accentLabel.setText ("Accent", juce::dontSendNotification);
    accentLabel.setFont (font (TextSize::bodySmall));
    accentLabel.setColour (juce::Label::textColourId, colors::textSecondary());
    addAndMakeVisible (accentLabel);

    // Accent swatches
    const auto& palette = accentPalette();
    for (const auto& entry : palette)
    {
        const auto accentName = entry.name;
        auto* swatch = new AccentSwatch (entry, [this, accentName]
        {
            settings.accentName = accentName;
            saveSettings();
            for (auto& s : swatches)
                s->selected = s->entry.name.equalsIgnoreCase (accentName);
            repaint();
            applyAppearance();
        });
        swatch->selected = accentName.equalsIgnoreCase (settings.accentName);
        addAndMakeVisible (swatch);
        swatches.emplace_back (swatch);
    }

    // Preview section
    previewHeader.setText ("Preview", juce::dontSendNotification);
    previewHeader.setFont (font (TextSize::heading));
    previewHeader.setColour (juce::Label::textColourId, colors::textPrimary());
    addAndMakeVisible (previewHeader);

    addAndMakeVisible (previewPrimaryBtn);
    addAndMakeVisible (previewSecondaryBtn);
    addAndMakeVisible (previewDangerBtn);

    previewText.setText ("The quick brown fox jumps over the lazy dog.", juce::dontSendNotification);
    previewText.setFont (font (TextSize::body));
    previewText.setColour (juce::Label::textColourId, colors::textPrimary());
    addAndMakeVisible (previewText);

    addAndMakeVisible (previewTag);

    previewSlider.setRange (0.0, 100.0, 1.0);
    previewSlider.setValue (62.0, juce::dontSendNotification);
    addAndMakeVisible (previewSlider);

    // --- About panel ---
    aboutHeader.setText ("About", juce::dontSendNotification);
    aboutHeader.setFont (font (TextSize::heading));
    aboutHeader.setColour (juce::Label::textColourId, colors::textPrimary());
    addAndMakeVisible (aboutHeader);

    aboutName.setText ("Otoha", juce::dontSendNotification);
    aboutName.setFont (font (TextSize::title));
    aboutName.setColour (juce::Label::textColourId, colors::textPrimary());
    addAndMakeVisible (aboutName);

    aboutVersion.setText ("Version " + juce::String (OTOHA_VERSION), juce::dontSendNotification);
    aboutVersion.setFont (font (TextSize::body));
    aboutVersion.setColour (juce::Label::textColourId, colors::textSecondary());
    addAndMakeVisible (aboutVersion);

    aboutLicense.setText ("AGPL-3.0-or-later", juce::dontSendNotification);
    aboutLicense.setFont (font (TextSize::bodySmall));
    aboutLicense.setColour (juce::Label::textColourId, colors::textMuted());
    addAndMakeVisible (aboutLicense);

    aboutDescription.setText ("Simple, open-source recording and audio enhancement.\n\nRecord. Edit. Enhance. Export.", juce::dontSendNotification);
    aboutDescription.setFont (font (TextSize::body));
    aboutDescription.setColour (juce::Label::textColourId, colors::textSecondary());
    addAndMakeVisible (aboutDescription);

    selectCategory (Category::general);
    themeChangedBroadcaster().addChangeListener (this);
}

SettingsView::~SettingsView()
{
    themeChangedBroadcaster().removeChangeListener (this);
}

void SettingsView::paint (juce::Graphics& g)
{
    g.fillAll (colors::background());

    // Subtle rail background
    auto railArea = getLocalBounds().reduced (Spacing::xl).withWidth (180);
    g.setColour (colors::surface());
    g.fillRoundedRectangle (railArea.toFloat(), (float) Radius::large);
    g.setColour (colors::borderSubtle());
    g.drawRoundedRectangle (railArea.toFloat(), (float) Radius::large, 1.0f);
}

void SettingsView::resized()
{
    auto bounds = getLocalBounds().reduced (Spacing::xl);
    const int w = bounds.getWidth();
    const bool compact = otoha::ds::responsive::isCompact (w);
    const int railW = otoha::ds::responsive::settingsRailWidth (w);
    const int contentW = compact ? bounds.getWidth() : juce::jmin (600, bounds.getWidth() - railW - Spacing::xl);

    if (railW > 0)
    {
        // --- Category rail (medium/expanded) ---
        auto rail = bounds.removeFromLeft (railW);
        rail.removeFromTop (Spacing::xl); // space for header

        // Search
        searchField->setBounds (rail.removeFromTop (36));
        rail.removeFromTop (Spacing::md);

        // Category items
        const int itemH = 36;
        for (auto& cat : categories)
            cat->setBounds (rail.removeFromTop (itemH).reduced (4, 2));

        bounds.removeFromLeft (Spacing::xl);
    }
    else
    {
        // M35 Compact: horizontal category tabs at top
        auto tabBar = bounds.removeFromTop (48);
        searchField->setBounds (tabBar.removeFromTop (32));
        tabBar.removeFromTop (Spacing::xs);
        // Lay out category items horizontally
        const int tabW = tabBar.getWidth() / juce::jmax (1, (int) categories.size());
        for (int i = 0; i < (int) categories.size(); ++i)
            categories[(size_t) i]->setBounds (tabBar.getX() + i * tabW, tabBar.getY(), tabW, tabBar.getHeight());
        bounds.removeFromTop (Spacing::md);
    }

    // --- Content area ---
    auto content = bounds.withWidth (contentW);
    content.removeFromTop (Spacing::lg); // top padding

    // Header
    headerTitle.setBounds (content.removeFromTop (32));
    content.removeFromTop (Spacing::md);

    if (activeCategory == Category::general)
    {
        // General panel
        generalHeader.setBounds (content.removeFromTop (26));
        content.removeFromTop (Spacing::xs);
        generalDesc.setBounds (content.removeFromTop (18));
        content.removeFromTop (Spacing::lg);

        countdownLabel.setBounds (content.removeFromTop (22));
        content.removeFromTop (Spacing::xs);
        countdownDesc.setBounds (content.removeFromTop (16));
        content.removeFromTop (Spacing::xs);
        countdownCombo->setBounds (content.removeFromTop (32).withWidth (180));
    }
    else if (activeCategory == Category::appearance)
    {
        // Mode
        appearanceHeader.setBounds (content.removeFromTop (26));
        content.removeFromTop (Spacing::xs);
        modeLabel.setBounds (content.removeFromTop (18));
        content.removeFromTop (Spacing::xs);
        modeCombo->setBounds (content.removeFromTop (32).withWidth (180));
        content.removeFromTop (Spacing::lg);

        // Accent
        accentLabel.setBounds (content.removeFromTop (18));
        content.removeFromTop (Spacing::sm);

        const int swatchSize = 36;
        const int swatchGap = 8;
        auto swatchRow = content.removeFromTop (swatchSize);
        int x = 0;
        for (auto& s : swatches)
        {
            s->setBounds (swatchRow.getX() + x, swatchRow.getY(), swatchSize, swatchSize);
            x += swatchSize + swatchGap;
        }
        content.removeFromTop (Spacing::xl);

        // Preview
        previewHeader.setBounds (content.removeFromTop (24));
        content.removeFromTop (Spacing::sm);
        previewPrimaryBtn.setBounds (content.removeFromTop (36).withWidth (160));
        content.removeFromTop (Spacing::xs);
        previewSecondaryBtn.setBounds (content.removeFromTop (36).withWidth (160));
        content.removeFromTop (Spacing::xs);
        previewDangerBtn.setBounds (content.removeFromTop (36).withWidth (160));
        content.removeFromTop (Spacing::sm);
        previewText.setBounds (content.removeFromTop (22));
        content.removeFromTop (Spacing::xs);
        previewTag.setBounds (content.removeFromTop (24).withWidth (100));
        content.removeFromTop (Spacing::sm);
        previewSlider.setBounds (content.removeFromTop (40).withWidth (300));
    }
    else if (activeCategory == Category::about)
    {
        aboutHeader.setBounds (content.removeFromTop (26));
        content.removeFromTop (Spacing::lg);
        aboutName.setBounds (content.removeFromTop (32));
        content.removeFromTop (Spacing::xs);
        aboutVersion.setBounds (content.removeFromTop (22));
        content.removeFromTop (Spacing::xs);
        aboutLicense.setBounds (content.removeFromTop (18));
        content.removeFromTop (Spacing::lg);
        aboutDescription.setBounds (content.removeFromTop (60));
    }
}

void SettingsView::selectCategory (Category cat)
{
    activeCategory = cat;

    for (auto& c : categories)
        c->selected = false;

    int idx = 0;
    if (cat == Category::general)    idx = 0;
    if (cat == Category::appearance) idx = 1;
    if (cat == Category::about)      idx = 2;

    if (idx < (int) categories.size())
        categories[(size_t) idx]->selected = true;

    // Show/hide content children
    const auto show = [this] (juce::Component& c, bool v) { c.setVisible (v); };

    // General
    show (generalHeader, cat == Category::general);
    show (generalDesc, cat == Category::general);
    show (countdownLabel, cat == Category::general);
    show (countdownDesc, cat == Category::general);
    show (*countdownCombo, cat == Category::general);

    // Appearance
    show (appearanceHeader, cat == Category::appearance);
    show (modeLabel, cat == Category::appearance);
    show (*modeCombo, cat == Category::appearance);
    show (accentLabel, cat == Category::appearance);
    for (auto& s : swatches) show (*s, cat == Category::appearance);
    show (previewHeader, cat == Category::appearance);
    show (previewPrimaryBtn, cat == Category::appearance);
    show (previewSecondaryBtn, cat == Category::appearance);
    show (previewDangerBtn, cat == Category::appearance);
    show (previewText, cat == Category::appearance);
    show (previewTag, cat == Category::appearance);
    show (previewSlider, cat == Category::appearance);

    // About
    show (aboutHeader, cat == Category::about);
    show (aboutName, cat == Category::about);
    show (aboutVersion, cat == Category::about);
    show (aboutLicense, cat == Category::about);
    show (aboutDescription, cat == Category::about);

    resized();
    repaint();
}

void SettingsView::reloadSettings()
{
    // Countdown
    int cdIdx = 1;
    if (settings.countdownSeconds == 0) cdIdx = 0;
    else if (settings.countdownSeconds == 5) cdIdx = 2;
    else if (settings.countdownSeconds == 10) cdIdx = 3;
    countdownCombo->setSelectedItemIndex (cdIdx, juce::dontSendNotification);

    // Mode
    int modeIdx = 0;
    if (settings.appearanceMode.equalsIgnoreCase ("light")) modeIdx = 1;
    else if (settings.appearanceMode.equalsIgnoreCase ("dark")) modeIdx = 2;
    modeCombo->setSelectedItemIndex (modeIdx, juce::dontSendNotification);

    // Accent
    for (auto& s : swatches)
        s->selected = s->entry.name.equalsIgnoreCase (settings.accentName);
    repaint();
}

void SettingsView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    repaint();
}

void SettingsView::saveSettings()
{
    otoha::saveAppSettings (settings, otoha::defaultSettingsDirectory());
}

void SettingsView::applyAppearance()
{
    auto mode = settings.appearanceMode;
    if (mode.equalsIgnoreCase ("system"))
    {
        const bool osDark = juce::Desktop::getInstance().isDarkModeActive();
        mode = osDark ? "dark" : "light";
    }

    const auto accent = accentByName (settings.accentName);

    if (mode.equalsIgnoreCase ("light"))
        setTheme (lightThemeWithAccent (accent));
    else
        setTheme (darkThemeWithAccent (accent));

    applyToDesktopLookAndFeel();
}
