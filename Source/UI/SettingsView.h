#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Core/AppSettings.h"
#include "Components/DsButton.h"
#include "Components/DsControls.h"
#include "Components/DsCore.h"
#include "Components/DsSurfaces.h"

/*    SettingsView — Otoha's settings page (M24).

      Currently hosts the Appearance section: System / Light / Dark mode
      selector and a curated accent colour picker.  All visuals consume
      OtohaTheme tokens; live recolor happens through otoha::theme::setTheme()
      and the existing ChangeBroadcaster infrastructure.
*/
class SettingsView : public juce::Component,
                     private juce::ChangeListener
{
public:
    explicit SettingsView (otoha::AppSettings& settings);
    ~SettingsView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Reload settings from disk (e.g. after external modification). */
    void reloadSettings();

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void applyAppearance();
    void setMode (const juce::String& mode);
    void setAccent (const juce::String& name);

    otoha::AppSettings& settings;

    // Header
    juce::Label headerTitle { {}, "Settings" };

    // Appearance section
    juce::Label appearanceHeader { {}, "Appearance" };
    juce::Label modeLabel { {}, "Mode" };
    std::unique_ptr<otoha::ds::ComboBox> modeCombo;
    juce::Label accentLabel { {}, "Accent" };

    // Accent swatches — a row of coloured circles
    struct AccentSwatch : public juce::Component
    {
        AccentSwatch (const otoha::theme::AccentEntry& e, std::function<void()> onSelected);
        void paint (juce::Graphics& g) override;
        void mouseUp (const juce::MouseEvent&) override;
        otoha::theme::AccentEntry entry;
        bool selected = false;
        std::function<void()> onSelect;
    };
    std::vector<std::unique_ptr<AccentSwatch>> swatches;

    // Preview
    juce::Label previewHeader { {}, "Preview" };
    otoha::ds::Button previewPrimaryBtn { "Primary", otoha::ds::ButtonVariant::primary };
    otoha::ds::Button previewSecondaryBtn { "Secondary", otoha::ds::ButtonVariant::secondary };
    otoha::ds::Button previewDangerBtn { "Danger", otoha::ds::ButtonVariant::danger };
    juce::Label previewText { {}, "The quick brown fox jumps over the lazy dog." };
    otoha::ds::Tag previewTag { "Enhanced", otoha::ds::Tag::Variant::accent };
    juce::Slider previewSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsView)
};
