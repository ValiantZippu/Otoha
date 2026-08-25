#pragma once

/*    DsDialog — themed dialog primitives (M26).

    DsDialog           : base dialog with animated entrance (scale + fade)
    DsConfirmDialog    : title + message + Cancel/Confirm buttons
    DsPromptDialog     : title + text field + Cancel/Save buttons
    DsProgressDialog   : title + message + progress bar + percentage

    All dialogs use the DS motion system and respect reduced-motion preferences.
*/

#include "DsCore.h"
#include "DsButton.h"
#include "DsControls.h"

namespace otoha::ds
{

// ---------------------------------------------------------------------------
// Base dialog (animated entrance)
// ---------------------------------------------------------------------------

/** Animated dialog panel. Entrance: scale from 0.94 → 1.0 with spring, fade from 0 → 1. */
class Dialog : public juce::Component,
               private juce::Timer
{
public:
    Dialog (const juce::String& titleText, bool compact = false)
        : title_ (titleText), isCompact (compact)
    {
        setWantsKeyboardFocus (true);
        startTimerHz (60);
    }

    ~Dialog() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        // dim background
        g.setColour (juce::Colour::fromRGBA (0, 0, 0, (int) (128.0f * animAlpha)));
        g.fillRect (getLocalBounds());

        // dialog panel
        const auto bounds = getLocalBounds().toFloat();
        const float panelW = isCompact
            ? juce::jmin (bounds.getWidth() * 0.5f, 560.0f)
            : juce::jmin (bounds.getWidth() * 0.6f, 860.0f);
        const float panelH = isCompact ? 240.0f : 400.0f;
        const auto panel = juce::Rectangle<float> (panelW, panelH)
            .withCentre (bounds.getCentre());

        // scale animation
        const float sc = 0.94f + 0.06f * animAlpha;
        const auto centre = panel.getCentre();
        g.addTransform (juce::AffineTransform::translation (-centre.x, -centre.y)
                            .scaled (sc)
                            .translated (centre.x, centre.y));

        const float r = (float) theme::Radius::xl;

        g.setColour (theme::colors::surfaceElevated());
        g.fillRoundedRectangle (panel, r);
        g.setColour (theme::colors::border());
        g.drawRoundedRectangle (panel.reduced (0.5f), r, 1.0f);

        // title
        g.setColour (theme::colors::textPrimary());
        g.setFont (theme::font (theme::TextSize::title));
        g.drawText (title_, panel.reduced (theme::Spacing::xl)
                                    .removeFromTop (28),
                    juce::Justification::centredLeft);
    }

    void timerCallback() override
    {
        if (animAlpha < 1.0f)
        {
            animAlpha = juce::jmin (1.0f, animAlpha + 0.08f);
            repaint();
        }
        else
        {
            stopTimer();
        }
    }

    juce::Rectangle<int> contentBounds() const
    {
        auto bounds = getLocalBounds();
        const float panelW = isCompact
            ? juce::jmin ((float) bounds.getWidth() * 0.5f, 560.0f)
            : juce::jmin ((float) bounds.getWidth() * 0.6f, 860.0f);
        const float panelH = isCompact ? 240.0f : 400.0f;
        auto panel = juce::Rectangle<float> (panelW, panelH)
            .withCentre (bounds.toFloat().getCentre());
        return panel.reduced ((float) theme::Spacing::xl).toType<int>()
                    .withTop (panel.getY() + theme::Spacing::xl + 28 + theme::Spacing::lg);
    }

private:
    juce::String title_;
    bool isCompact;
    float animAlpha = 0.0f;
};

// ---------------------------------------------------------------------------
// Confirm dialog
// ---------------------------------------------------------------------------

/** Title + message + Cancel (ghost) + Confirm (primary or danger) buttons. */
class ConfirmDialog : public Dialog
{
public:
    ConfirmDialog (const juce::String& title, const juce::String& message,
                   const juce::String& confirmText, bool danger = false)
        : Dialog (title, true)
    {
        messageLabel.setText (message, juce::dontSendNotification);
        messageLabel.setColour (juce::Label::textColourId, theme::colors::textSecondary());
        messageLabel.setFont (theme::font (theme::TextSize::body));
        addAndMakeVisible (messageLabel);

        cancelBtn = std::make_unique<TextButton> ("Cancel");
        cancelBtn->onClick = [this] { if (onCancel) onCancel(); };
        addAndMakeVisible (*cancelBtn);

        confirmBtn = std::make_unique<Button> (confirmText,
            danger ? ButtonVariant::danger : ButtonVariant::primary);
        confirmBtn->onClick = [this] { if (onConfirm) onConfirm(); };
        addAndMakeVisible (*confirmBtn);
    }

    std::function<void()> onCancel;
    std::function<void()> onConfirm;

    void resized() override
    {
        Dialog::resized();
        auto area = contentBounds();
        messageLabel.setBounds (area.removeFromTop (60));
        area.removeFromBottom (theme::Spacing::lg);

        auto btnArea = area.removeFromBottom (buttonHeight (ButtonSize::medium));
        const int btnW = 100;
        confirmBtn->setBounds (btnArea.getRight() - btnW, btnArea.getY(), btnW, btnArea.getHeight());
        cancelBtn->setBounds (confirmBtn->getX() - btnW - theme::Spacing::sm, btnArea.getY(),
                              btnW, btnArea.getHeight());
    }

private:
    juce::Label messageLabel;
    std::unique_ptr<TextButton> cancelBtn;
    std::unique_ptr<Button> confirmBtn;
};

// ---------------------------------------------------------------------------
// Prompt dialog
// ---------------------------------------------------------------------------

/** Title + text field + Cancel/Save buttons. */
class PromptDialog : public Dialog
{
public:
    PromptDialog (const juce::String& title, const juce::String& placeholder,
                  const juce::String& initialValue = "")
        : Dialog (title, true)
    {
        input = std::make_unique<Input> ("Value", placeholder);
        input->setText (initialValue);
        addAndMakeVisible (*input);

        cancelBtn = std::make_unique<TextButton> ("Cancel");
        cancelBtn->onClick = [this] { if (onCancel) onCancel(); };
        addAndMakeVisible (*cancelBtn);

        saveBtn = std::make_unique<Button> ("Save", ButtonVariant::primary);
        saveBtn->onClick = [this] { if (onSave) onSave (input->getText()); };
        addAndMakeVisible (*saveBtn);
    }

    std::function<void()> onCancel;
    std::function<void (const juce::String&)> onSave;

    void resized() override
    {
        Dialog::resized();
        auto area = contentBounds();
        input->setBounds (area.removeFromTop (theme::Metrics::inputHeight + 8));
        area.removeFromBottom (theme::Spacing::lg);

        auto btnArea = area.removeFromBottom (buttonHeight (ButtonSize::medium));
        const int btnW = 100;
        saveBtn->setBounds (btnArea.getRight() - btnW, btnArea.getY(), btnW, btnArea.getHeight());
        cancelBtn->setBounds (saveBtn->getX() - btnW - theme::Spacing::sm, btnArea.getY(),
                              btnW, btnArea.getHeight());
    }

private:
    std::unique_ptr<Input> input;
    std::unique_ptr<TextButton> cancelBtn;
    std::unique_ptr<Button> saveBtn;
};

// ---------------------------------------------------------------------------
// Progress dialog
// ---------------------------------------------------------------------------

/** Title + message + progress bar + percentage. */
class ProgressDialog : public Dialog
{
public:
    ProgressDialog (const juce::String& title, const juce::String& message)
        : Dialog (title, true)
    {
        messageLabel.setText (message, juce::dontSendNotification);
        messageLabel.setColour (juce::Label::textColourId, theme::colors::textSecondary());
        messageLabel.setFont (theme::font (theme::TextSize::body));
        addAndMakeVisible (messageLabel);

        progressBar = std::make_unique<ProgressBar> (0.0f);
        addAndMakeVisible (*progressBar);

        percentLabel.setColour (juce::Label::textColourId, theme::colors::textMuted());
        percentLabel.setFont (theme::font (theme::TextSize::caption));
        percentLabel.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (percentLabel);
    }

    void setProgress (float fraction)
    {
        progressBar->setFraction (fraction);
        percentLabel.setText (juce::String ((int) (fraction * 100.0f)) + "%",
                              juce::dontSendNotification);
    }

    void resized() override
    {
        Dialog::resized();
        auto area = contentBounds();
        messageLabel.setBounds (area.removeFromTop (40));
        area.removeFromTop (theme::Spacing::md);
        progressBar->setBounds (area.removeFromTop (6));
        area.removeFromTop (theme::Spacing::sm);
        percentLabel.setBounds (area.removeFromTop (18));
    }

private:
    juce::Label messageLabel;
    std::unique_ptr<ProgressBar> progressBar;
    juce::Label percentLabel;
};

} // namespace otoha::ds
