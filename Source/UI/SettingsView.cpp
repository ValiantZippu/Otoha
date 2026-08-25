#include "SettingsView.h"
#include "OtohaTheme.h"
#include "Components/DsButton.h"
#include "Components/DsControls.h"
#include "Components/DsSurfaces.h"

using namespace otoha::theme;

/* ======================================================================
   AccentSwatch — a coloured circle that selects an accent.
   ====================================================================== */
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

    // Outer ring: selected gets accent border, otherwise subtle
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

    // Fill
    g.setColour (entry.base);
    g.fillEllipse (bounds.reduced (4.0f));

    // Checkmark for selected
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

/* ======================================================================
   SettingsView — M24 Appearance picker.
   ====================================================================== */
SettingsView::SettingsView (otoha::AppSettings& s) : settings (s)
{
    setOpaque (true);

    // Header
    headerTitle.setFont (font (TextSize::title));
    headerTitle.setColour (juce::Label::textColourId, colors::textPrimary());
    addAndMakeVisible (headerTitle);

    // Appearance section
    appearanceHeader.setFont (font (TextSize::heading));
    appearanceHeader.setColour (juce::Label::textColourId, colors::textPrimary());
    addAndMakeVisible (appearanceHeader);

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
        setMode (modes[juce::jlimit (0, 2, idx)]);
    };
    addAndMakeVisible (*modeCombo);

    accentLabel.setFont (font (TextSize::bodySmall));
    accentLabel.setColour (juce::Label::textColourId, colors::textSecondary());
    addAndMakeVisible (accentLabel);

    // Accent swatches
    const auto& palette = accentPalette();
    for (const auto& entry : palette)
    {
        const auto accentName = entry.name;
        auto* swatch = new AccentSwatch (entry, [this, accentName] { setAccent (accentName); });
        swatch->selected = accentName.equalsIgnoreCase (settings.accentName);
        addAndMakeVisible (swatch);
        swatches.emplace_back (swatch);
    }

    // Preview section
    previewHeader.setFont (font (TextSize::heading));
    previewHeader.setColour (juce::Label::textColourId, colors::textPrimary());
    addAndMakeVisible (previewHeader);

    addAndMakeVisible (previewPrimaryBtn);
    addAndMakeVisible (previewSecondaryBtn);
    addAndMakeVisible (previewDangerBtn);

    previewText.setFont (font (TextSize::body));
    previewText.setColour (juce::Label::textColourId, colors::textPrimary());
    addAndMakeVisible (previewText);

    addAndMakeVisible (previewTag);

    previewSlider.setRange (0.0, 100.0, 1.0);
    previewSlider.setValue (62.0, juce::dontSendNotification);
    previewSlider.setColour (juce::Slider::trackColourId, colors::accent());
    previewSlider.setColour (juce::Slider::thumbColourId, colors::textPrimary());
    previewSlider.setColour (juce::Slider::backgroundColourId, colors::surfaceHover());
    previewSlider.setColour (juce::Slider::textBoxTextColourId, colors::textPrimary());
    addAndMakeVisible (previewSlider);

    // Listen for theme changes to update preview colours
    themeChangedBroadcaster().addChangeListener (this);
}

SettingsView::~SettingsView()
{
    themeChangedBroadcaster().removeChangeListener (this);
}

void SettingsView::paint (juce::Graphics& g)
{
    g.fillAll (colors::background());
}

void SettingsView::resized()
{
    auto bounds = getLocalBounds().reduced (Spacing::xl);
    const int maxW = 600;
    auto content = bounds.withSizeKeepingCentre (juce::jmin (maxW, bounds.getWidth()),
                                                 bounds.getHeight());

    // Header
    headerTitle.setBounds (content.removeFromTop (36));
    content.removeFromTop (Spacing::lg);

    // Mode
    appearanceHeader.setBounds (content.removeFromTop (24));
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
        s->setBounds (x, 0, swatchSize, swatchSize);
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

void SettingsView::reloadSettings()
{
    int idx = 0;
    if (settings.appearanceMode.equalsIgnoreCase ("light")) idx = 1;
    else if (settings.appearanceMode.equalsIgnoreCase ("dark")) idx = 2;
    modeCombo->setSelectedItemIndex (idx, juce::dontSendNotification);

    for (auto& s : swatches)
        s->selected = s->entry.name.equalsIgnoreCase (settings.accentName);
    repaint();
}

void SettingsView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // Theme changed externally — update preview widget colours
    repaint();
}

void SettingsView::setMode (const juce::String& mode)
{
    settings.appearanceMode = mode;
    otoha::saveAppSettings (settings, otoha::defaultSettingsDirectory());
    applyAppearance();
}

void SettingsView::setAccent (const juce::String& name)
{
    settings.accentName = name;
    otoha::saveAppSettings (settings, otoha::defaultSettingsDirectory());

    // Update swatch selection visual
    for (auto& s : swatches)
        s->selected = s->entry.name.equalsIgnoreCase (name);
    repaint();

    applyAppearance();
}

void SettingsView::applyAppearance()
{
    // Resolve mode
    auto mode = settings.appearanceMode;
    if (mode.equalsIgnoreCase ("system"))
    {
        // Use the OS dark mode if available, else dark as default
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
