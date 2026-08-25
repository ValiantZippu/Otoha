#pragma once

/*    ComponentsGallery — the M18/M26 design-system reference screen (dev only).

    Not part of normal navigation: AppShell opens it with Ctrl+Shift+D.
    Every Ds component and state lives here so future screens copy from a
    single visual source of truth.

    M26 additions:
      - accentTint button, TextButton, ButtonRow
      - ListItem, StatTile, ProgressBar, Badge, Skeleton
      - SearchField, Select, TextField
      - Dialog, ConfirmDialog, ProgressDialog
*/

#include "Components/DsButton.h"
#include "Components/DsControls.h"
#include "Components/DsSurfaces.h"
#include "Components/DsToast.h"
#include "Components/DsDialog.h"
#include "Components/DsMenu.h"

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
        makeRow ("Buttons — Primary / Secondary / Tertiary / Danger / AccentTint", 36, [&] (juce::Component& body)
        {
            place (body, makeButton ("Primary", ds::ButtonVariant::primary, false), 0);
            place (body, makeButton ("Secondary", ds::ButtonVariant::secondary, false), 1);
            place (body, makeButton ("Tertiary", ds::ButtonVariant::tertiary, false), 2);
            place (body, makeButton ("Danger", ds::ButtonVariant::danger, false), 3);
            place (body, makeButton ("AccentTint", ds::ButtonVariant::accentTint, false), 4);
            place (body, makeButton ("Disabled", ds::ButtonVariant::primary, true), 5);
        });

        // --- icon buttons ----------------------------------------------------
        makeRow ("Icon buttons", 44, [&] (juce::Component& body)
        {
            place (body, makeIconButton ("Play", otoha::icons::play(), false), 0);
            place (body, makeIconButton ("Pause", otoha::icons::pause(), false), 1);
            place (body, makeIconButton ("Close", otoha::icons::close(), false), 2);
            place (body, makeIconButton ("Undo (disabled)", otoha::icons::undo(), true), 3);
        });

        // --- text buttons -----------------------------------------------------
        makeRow ("Text buttons", 36, [&] (juce::Component& body)
        {
            auto b1 = std::make_unique<ds::TextButton> ("View all");
            b1->setSize (100, ds::buttonHeight (ds::ButtonSize::small));
            place (body, std::move (b1), 0);

            auto b2 = std::make_unique<ds::TextButton> ("Cancel");
            b2->setSize (100, ds::buttonHeight (ds::ButtonSize::small));
            place (body, std::move (b2), 1);
        });

        // --- button row -------------------------------------------------------
        makeRow ("Button row", 44, [&] (juce::Component& body)
        {
            auto row = std::make_unique<ds::ButtonRow>();
            auto* rowPtr = row.get();
            row->setSize (500, 44);
            body.addAndMakeVisible (*row);
            owned.push_back (std::move (row));

            auto b1 = std::make_unique<ds::Button> ("Record", ds::ButtonVariant::primary);
            rowPtr->addAndMakeVisible (*b1);
            owned.push_back (std::move (b1));

            auto b2 = std::make_unique<ds::Button> ("Library", ds::ButtonVariant::secondary);
            rowPtr->addAndMakeVisible (*b2);
            owned.push_back (std::move (b2));

            auto b3 = std::make_unique<ds::Button> ("Enhance", ds::ButtonVariant::secondary);
            rowPtr->addAndMakeVisible (*b3);
            owned.push_back (std::move (b3));

            auto b4 = std::make_unique<ds::Button> ("Export", ds::ButtonVariant::accentTint);
            rowPtr->addAndMakeVisible (*b4);
            owned.push_back (std::move (b4));
        });

        // --- controls --------------------------------------------------------
        makeRow ("Controls — ComboBox / Slider / Toggle / Input / Select", 6 * theme::Metrics::rowHeight + 5 * theme::Spacing::sm, [&] (juce::Component& body)
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

        // --- search field -----------------------------------------------------
        makeRow ("Search field", theme::Metrics::inputHeight + 8, [&] (juce::Component& body)
        {
            auto search = std::make_unique<ds::SearchField> ("Search recordings…");
            search->setSize (320, theme::Metrics::inputHeight + 8);
            place (body, std::move (search), 0);
        });

        // --- select -----------------------------------------------------------
        makeRow ("Select (dropdown)", theme::Metrics::inputHeight + 8, [&] (juce::Component& body)
        {
            auto sel = std::make_unique<ds::Select> ("Format");
            sel->addItem ("WAV", 1);
            sel->addItem ("FLAC", 2);
            sel->addItem ("MP3", 3);
            sel->setSelectedItemIndex (0, juce::dontSendNotification);
            sel->setSize (200, theme::Metrics::inputHeight + 8);
            place (body, std::move (sel), 0);
        });

        // --- tags --------------------------------------------------------------
        makeRow ("Tags / Badges", 24, [&] (juce::Component& body)
        {
            place (body, makeTag ("Enhanced", ds::Tag::Variant::accent), 0);
            place (body, makeTag ("Lossless", ds::Tag::Variant::success), 1);
            place (body, makeTag ("Pending", ds::Tag::Variant::warning), 2);
            place (body, makeTag ("Failed", ds::Tag::Variant::danger), 3);
            place (body, makeTag ("Voice", ds::Tag::Variant::neutral), 4);

            auto badge = std::make_unique<ds::Badge> ("3", theme::colors::accent());
            badge->setSize (40, 22);
            badge->setTopLeftPosition (5 * (90 + theme::Spacing::md), 0);
            body.addAndMakeVisible (*badge);
            owned.push_back (std::move (badge));
        });

        // --- progress bar ------------------------------------------------------
        makeRow ("Progress bar", 8, [&] (juce::Component& body)
        {
            auto bar = std::make_unique<ds::ProgressBar> (0.62f);
            bar->setSize (320, 6);
            place (body, std::move (bar), 0);
        });

        // --- cards --------------------------------------------------------------
        makeRow ("Cards (default / selected / prominent)", 96, [&] (juce::Component& body)
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

            auto prominent = std::make_unique<ds::Card> ("Hero card", true);
            prominent->setBounds (560 + theme::Spacing::md * 2, 0, 280, 96);
            prominent->setProminent (true);
            body.addAndMakeVisible (*prominent);
            owned.push_back (std::move (prominent));
        });

        // --- list item ----------------------------------------------------------
        makeRow ("List item", 48, [&] (juce::Component& body)
        {
            auto item = std::make_unique<ds::ListItem> ("Recording take 3", "2 min 15 sec · WAV · 48 kHz");
            item->setSize (400, 48);
            place (body, std::move (item), 0);
        });

        // --- stat tiles ---------------------------------------------------------
        makeRow ("Stat tiles", 100, [&] (juce::Component& body)
        {
            auto t1 = std::make_unique<ds::StatTile> ("TOTAL", "12", "3 this week", true);
            t1->setBounds (0, 0, 160, 100);
            body.addAndMakeVisible (*t1);
            owned.push_back (std::move (t1));

            auto t2 = std::make_unique<ds::StatTile> ("DURATION", "45 min", "12 min avg");
            t2->setBounds (160 + theme::Spacing::md, 0, 160, 100);
            body.addAndMakeVisible (*t2);
            owned.push_back (std::move (t2));

            auto t3 = std::make_unique<ds::StatTile> ("FORMAT", "WAV", "48 kHz");
            t3->setBounds (320 + theme::Spacing::md * 2, 0, 160, 100);
            body.addAndMakeVisible (*t3);
            owned.push_back (std::move (t3));
        });

        // --- skeleton -----------------------------------------------------------
        makeRow ("Skeleton (loading placeholder)", 24, [&] (juce::Component& body)
        {
            auto s1 = std::make_unique<ds::Skeleton> (160, 12);
            place (body, std::move (s1), 0);

            auto s2 = std::make_unique<ds::Skeleton> (120, 12);
            s2->setTopLeftPosition (160 + theme::Spacing::md, 0);
            body.addAndMakeVisible (*s2);
            owned.push_back (std::move (s2));

            auto s3 = std::make_unique<ds::Skeleton> (200, 12);
            s3->setTopLeftPosition (280 + theme::Spacing::md * 2, 0);
            body.addAndMakeVisible (*s3);
            owned.push_back (std::move (s3));
        });

        // --- toasts ---------------------------------------------------------------
        makeRow ("Toasts (info / success / warning / error)", 36, [&] (juce::Component& body)
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
        makeRow ("Divider & section header", 60, [&] (juce::Component& body)
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
                new ds::EmptyState ({ otoha::icons::play(), "No recordings yet.",
                                      "Record something to see it here.", actionPtr }));
            empty->setBounds (0, 0, 320, 160);
            body.addAndMakeVisible (*empty);
            owned.push_back (std::move (empty));
        });

        // --- dialog trigger ---------------------------------------------------------
        makeRow ("Dialogs (click to open)", 36, [&] (juce::Component& body)
        {
            auto confirmBtn = std::make_unique<ds::Button> ("Confirm dialog", ds::ButtonVariant::secondary);
            confirmBtn->setSize (130, ds::buttonHeight (ds::ButtonSize::medium));
            confirmBtn->onClick = [this] { showConfirmDialog(); };
            place (body, std::move (confirmBtn), 0);

            auto promptBtn = std::make_unique<ds::Button> ("Prompt dialog", ds::ButtonVariant::secondary);
            promptBtn->setSize (130, ds::buttonHeight (ds::ButtonSize::medium));
            promptBtn->onClick = [this] { showPromptDialog(); };
            place (body, std::move (promptBtn), 1);

            auto progressBtn = std::make_unique<ds::Button> ("Progress dialog", ds::ButtonVariant::secondary);
            progressBtn->setSize (130, ds::buttonHeight (ds::ButtonSize::medium));
            progressBtn->onClick = [this] { showProgressDialog(); };
            place (body, std::move (progressBtn), 2);
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
    // --- dialog helpers ---------------------------------------------------------

    void showConfirmDialog()
    {
        auto* dialog = new ds::ConfirmDialog ("Delete recording",
                                               "Are you sure you want to delete this recording? This cannot be undone.",
                                               "Delete", true);
        dialog->onConfirm = [dialog] { delete dialog; };
        dialog->onCancel = [dialog] { delete dialog; };
        addAndMakeVisible (dialog);
        dialog->setBounds (getLocalBounds());
        dialog->toFront (true);
    }

    void showPromptDialog()
    {
        auto* dialog = new ds::PromptDialog ("Rename recording", "Enter new name…", "My take");
        dialog->onSave = [dialog] (const juce::String&) { delete dialog; };
        dialog->onCancel = [dialog] { delete dialog; };
        addAndMakeVisible (dialog);
        dialog->setBounds (getLocalBounds());
        dialog->toFront (true);
    }

    void showProgressDialog()
    {
        auto* dialog = new ds::ProgressDialog ("Exporting…", "Exporting recording to WAV format");
        dialog->setProgress (0.0f);
        addAndMakeVisible (dialog);
        dialog->setBounds (getLocalBounds());
        dialog->toFront (true);

        // animate progress for demo
        struct ProgressAnimator : juce::Timer
        {
            ds::ProgressDialog* dlg;
            float progress = 0.0f;
            void timerCallback() override
            {
                progress += 0.02f;
                if (progress >= 1.0f)
                {
                    dlg->setProgress (1.0f);
                    stopTimer();
                    auto* d = dlg;
                    juce::Timer::callAfterDelay (500, [d] { delete d; });
                }
                else
                {
                    dlg->setProgress (progress);
                }
            }
        };
        auto* animator = new ProgressAnimator();
        animator->dlg = dialog;
        animator->startTimerHz (30);
    }

    // --- generic helpers --------------------------------------------------------

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
        titleLabel->setSize (600, 22);
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
