#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Core/AppSettings.h"
#include "Components/DsButton.h"
#include "Components/DsControls.h"
#include "Components/DsCore.h"
#include "Components/DsSurfaces.h"
#include "Components/OtohaIcons.h"

/*
    SettingsView — M33 Kaiteyo-aligned settings page.

    Desktop:
    ┌──────────────┬─────────────────────────────────────┐
    │ Search...    │                                     │
    │              │  General                            │
    │  General     │                                     │
    │  Audio       │  Application behavior               │
    │  Recording   │                                     │
    │  Export      │  ┌───────────────────────────────┐  │
    │  Appearance  │  │ Countdown          3 sec ▾    │  │
    │  Shortcuts   │  │ Start with system   [ ]       │  │
    │  About       │  └───────────────────────────────┘  │
    └──────────────┴─────────────────────────────────────┘

    All settings are immediately applied and persisted.
    Uses existing AppSettings, ExportSettingsStore, and OtohaTheme.
*/
class SettingsView : public juce::Component,
                     private juce::ChangeListener
{
public:
    explicit SettingsView (otoha::AppSettings& settings);
    ~SettingsView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void reloadSettings();

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    // Category system
    enum class Category { general, appearance, about };
    void selectCategory (Category cat);

    // Category rail item
    struct CategoryItem : public juce::Component
    {
        CategoryItem (const juce::String& label, juce::Path icon, std::function<void()> onClick);
        void paint (juce::Graphics& g) override;
        void mouseUp (const juce::MouseEvent&) override;
        juce::String label;
        juce::Path iconPath;
        bool selected = false;
        std::function<void()> onSelect;
        juce::Font getLabelFont() const;
    };

    // Appearance swatch
    struct AccentSwatch : public juce::Component
    {
        AccentSwatch (const otoha::theme::AccentEntry& e, std::function<void()> onSelected);
        void paint (juce::Graphics& g) override;
        void mouseUp (const juce::MouseEvent&) override;
        otoha::theme::AccentEntry entry;
        bool selected = false;
        std::function<void()> onSelect;
    };

    // Settings persistence
    void saveSettings();
    void applyAppearance();

    otoha::AppSettings& settings;

    // --- Header ---
    juce::Label headerTitle;

    // --- Category rail (left side) ---
    juce::Label railSearchLabel;
    std::unique_ptr<otoha::ds::SearchField> searchField;
    std::vector<std::unique_ptr<CategoryItem>> categories;

    // --- Content area (right side) ---
    Category activeCategory = Category::general;

    // General panel
    juce::Label generalHeader;
    juce::Label generalDesc;
    juce::Label countdownLabel;
    juce::Label countdownDesc;
    std::unique_ptr<otoha::ds::ComboBox> countdownCombo;

    // Appearance panel (preserved from M24)
    juce::Label appearanceHeader;
    juce::Label modeLabel;
    std::unique_ptr<otoha::ds::ComboBox> modeCombo;
    juce::Label accentLabel;
    std::vector<std::unique_ptr<AccentSwatch>> swatches;

    // Preview panel
    juce::Label previewHeader;
    otoha::ds::Button previewPrimaryBtn { "Primary", otoha::ds::ButtonVariant::primary };
    otoha::ds::Button previewSecondaryBtn { "Secondary", otoha::ds::ButtonVariant::secondary };
    otoha::ds::Button previewDangerBtn { "Danger", otoha::ds::ButtonVariant::danger };
    juce::Label previewText;
    otoha::ds::Tag previewTag { "Enhanced", otoha::ds::Tag::Variant::accent };
    otoha::ds::Slider previewSlider { "Preview value", true };

    // About panel
    juce::Label aboutHeader;
    juce::Label aboutName;
    juce::Label aboutVersion;
    juce::Label aboutLicense;
    juce::Label aboutDescription;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsView)
};
