#pragma once

/*
    ComponentsGallery — the M18 design-system reference screen (dev only).

    Not part of normal navigation: AppShell opens it with Ctrl+Shift+D.
    Every Ds component and state lives here so future screens copy from a
    single visual source of truth.
*/

#include "Components/DsButton.h"
#include "Components/DsControls.h"
#include "Components/DsSurfaces.h"
#include "Components/DsToast.h"

namespace otoha
{

class ComponentsGallery : public juce::Component
{
public:
    ComponentsGallery()
    {
        theme::label (*this, "Design system gallery");
        addAndMakeVisible (toastHost);

        // --- buttons -------------------------------------------------------
        makeRow ("Buttons", 36, [&] (juce::Component& body)
        {
            place (body, makeButton ("Primary", ds::ButtonVariant::primary, false), 0);
            place (body, makeButton ("Secondary", ds::ButtonVariant::secondary, false), 1);
            place (body, makeButton ("Tertiary", ds::ButtonVariant::tertiary, false), 2);
            place (body, makeButton ("Danger", ds::ButtonVariant::danger, false), 3);
            place (body, makeButton ("Disabled", ds::ButtonVariant::primary, true), 4);
        });

        // --- icon buttons ----------------------------------------------------
        makeRow ("Icon buttons", 44, [&] (juce::Component& body)
        {
            place (body, makeIconButton ("Play", ds::icons::play(), false), 0);
            place (body, makeIconButton ("Pause", ds::icons::pause(), false), 1);
            place (body, makeIconButton ("Close", ds::icons::close(), false), 2);
            place (body, makeIconButton ("Undo (disabled)", ds::icons::undo(), true), 3);
        });

        // --- controls --------------------------------------------------------
        makeRow ("Controls", 6 * theme::Metrics::rowHeight + 5 * theme::Spacing::sm, [&] (juce::Component& body)
        {
            int y = 0;
            auto combo = std::make_unique<ds::ComboBox> ("Example combo", "Choose…");
            for (auto* item : { "Microphone A", "Microphone B", "System Default" })
                combo->addItem (item, combo->getNumItems() + 1);
            combo->setSelectedItemIndex (0, juce::dontSendNotification);
            placeColumn (body, std::move (combo), y);

            auto slider = std::make_unique<ds::Slider> ("Example slider", true);
            slider->setRange (0.0, 100.0, 1.0);
            slider->setValue (62.0, juce::dontSendNotification);
            placeColumn (body, std::move (slider), y);

            auto toggleOn = std::make_unique<ds::Toggle> ("Example toggle on");
            toggleOn->setButtonText ("Enhance");
            toggleOn->setToggleState (true, juce::dontSendNotification);
            placeColumn (body, std::move (toggleOn), y);

            auto toggleOff = std::make_unique<ds::Toggle> ("Example toggle off");
            toggleOff->setButtonText ("Noise reduction");
            placeColumn (body, std::move (toggleOff), y);

            auto inputOk = std::make_unique<ds::Input> ("Example input", "Recording name");
            inputOk->setText ("My take 3", juce::dontSendNotification);
            placeColumn (body, std::move (inputOk), y);

            auto inputErr = std::make_unique<ds::Input> ("Example input error", "Export filename");
            inputErr->setError (true);
            placeColumn (body, std::move (inputErr), y);
        });

        // --- tags --------------------------------------------------------------
        makeRow ("Tags", 24, [&] (juce::Component& body)
        {
            place (body, makeTag ("Enhanced", ds::Tag::Variant::accent), 0);
            place (body, makeTag ("Lossless", ds::Tag::Variant::success), 1);
            place (body, makeTag ("Pending", ds::Tag::Variant::warning), 2);
            place (body, makeTag ("Failed", ds::Tag::Variant::danger), 3);
            place (body, makeTag ("Voice", ds::Tag::Variant::neutral), 4);
        });

        // --- cards --------------------------------------------------------------
        makeRow ("Cards", 96, [&] (juce::Component& body)
        {
            auto card = std::make_unique<ds::Card> ("Example card");
            card->setBounds (0, 0, 280, 96);
            body.addAndMakeVisible (*card);
            owned.push_back (std::move (card));

            auto selected = std::make_unique<ds::Card> ("Example card selected");
            selected->setBounds (280 + theme::Spacing::md, 0, 280, 96);
            selected->setSelected (true);
            body.addAndMakeVisible (*selected);
            owned.push_back (std::move (selected));
        });

        // --- toasts ---------------------------------------------------------------
        makeRow ("Toasts", 36, [&] (juce::Component& body)
        {
            int i = 0;
            for (auto kind : { ds::ToastHost::Kind::info, ds::ToastHost::Kind::success,
                               ds::ToastHost::Kind::warning, ds::ToastHost::Kind::error })
            {
                auto b = std::make_unique<ds::Button> (toastLabel (kind), ds::ButtonVariant::secondary);
                b->setSize (110, ds::buttonHeight (ds::ButtonSize::medium));
                b->onClick = [this, kind]
                {
                    toastHost.show (kind, "Example toast — " + toastLabel (kind));
                };
                b->setTopLeftPosition (i * (110 + theme::Spacing::md), 0);
                body.addAndMakeVisible (*b);
                owned.push_back (std::move (b));
                ++i;
            }
        });

        // --- divider + section -------------------------------------------------------
        makeRow ("Divider & section", 60, [&] (juce::Component& body)
        {
            auto divider = std::make_unique<ds::Divider>();
            divider->setBounds (0, 0, 320, 1);
            body.addAndMakeVisible (*divider);
            owned.push_back (std::move (divider));

            auto section = std::make_unique<ds::Section> ("Recording", "Microphone, countdown and format");
            section->setBounds (0, theme::Spacing::md, 320, 40);
            body.addAndMakeVisible (*section);
            owned.push_back (std::move (section));
        });

        // --- empty state ---------------------------------------------------------------
        makeRow ("Empty state", 160, [&] (juce::Component& body)
        {
            auto action = std::make_unique<ds::Button> ("Record", ds::ButtonVariant::primary);
            action->setSize (160, ds::buttonHeight (ds::ButtonSize::medium));
            auto* actionPtr = action.get();
            owned.push_back (std::move (action));

            std::unique_ptr<ds::EmptyState> empty (
                new ds::EmptyState ({ ds::icons::play(), "No recordings yet.", "Record something to see it here.", actionPtr }));
            empty->setBounds (0, 0, 320, 160);
            body.addAndMakeVisible (*empty);
            owned.push_back (std::move (empty));
        });
    }

    void paint (juce::Graphics& g) override { g.fillAll (theme::colors::background()); }

    void resized() override
    {
        toastHost.setBounds (getLocalBounds());
        auto area = getLocalBounds().reduced (theme::Spacing::xl);
        for (auto& row : rows)
        {
            row.title->setBounds (area.removeFromTop (22));
            area.removeFromTop (theme::Spacing::sm);
            row.body->setBounds (area.removeFromTop (row.body->getHeight()));
            area.removeFromTop (theme::Spacing::lg);
        }
    }

private:
    template <typename Item>
    void place (juce::Component& body, std::unique_ptr<Item> item, int index)
    {
        const int w = item->getWidth() > 0 ? item->getWidth() : 140;
        const int h = item->getHeight() > 0 ? item->getHeight() : 36;
        item->setSize (w, h);
        item->setTopLeftPosition (index * (w + theme::Spacing::md), 0);
        body.addAndMakeVisible (*item);
        owned.push_back (std::move (item));
    }

    void placeColumn (juce::Component& body, std::unique_ptr<juce::Component> item, int& y)
    {
        item->setSize (320, theme::Metrics::rowHeight);
        item->setTopLeftPosition (0, y);
        body.addAndMakeVisible (*item);
        owned.push_back (std::move (item));
        y += theme::Metrics::rowHeight + theme::Spacing::sm;
    }

    void makeRow (const juce::String& title, int bodyHeight, const std::function<void (juce::Component&)>& fill)
    {
        auto titleLabel = std::make_unique<juce::Label>();
        titleLabel->setText (title, juce::dontSendNotification);
        titleLabel->setFont (theme::font (theme::TextSize::heading));
        titleLabel->setColour (juce::Label::textColourId, theme::colors::textMuted());
        titleLabel->setSize (400, 22);
        addAndMakeVisible (*titleLabel);
        owned.push_back (std::move (titleLabel));

        auto body = std::make_unique<juce::Component>();
        fill (*body);
        body->setSize (100, bodyHeight);
        addAndMakeVisible (*body);
        rows.push_back ({ titleLabel.get(), std::move (body) });
    }

    std::unique_ptr<ds::Button> makeButton (const char* label, ds::ButtonVariant v, bool disabled)
    {
        auto b = std::make_unique<ds::Button> (label, v);
        b->setSize (140, ds::buttonHeight (ds::ButtonSize::medium));
        b->setEnabled (! disabled);
        return b;
    }

    std::unique_ptr<ds::IconButton> makeIconButton (const char* name, juce::Path p, bool disabled)
    {
        auto b = std::make_unique<ds::IconButton> (name, std::move (p));
        b->setSize (44, 44);
        b->setEnabled (! disabled);
        return b;
    }

    std::unique_ptr<ds::Tag> makeTag (const char* text, ds::Tag::Variant v)
    {
        auto t = std::make_unique<ds::Tag> (text, v);
        t->setSize (90, 22);
        return t;
    }

    static juce::String toastLabel (ds::ToastHost::Kind k)
    {
        switch (k)
        {
            case ds::ToastHost::Kind::info:    return "Info";
            case ds::ToastHost::Kind::success: return "Success";
            case ds::ToastHost::Kind::warning: return "Warning";
            case ds::ToastHost::Kind::error:   return "Error";
        }
        return {};
    }

    struct Row { juce::Component* title = nullptr; std::unique_ptr<juce::Component> body; };
    std::vector<Row> rows;
    std::vector<std::unique_ptr<juce::Component>> owned;
    ds::ToastHost toastHost;
};

} // namespace otoha
