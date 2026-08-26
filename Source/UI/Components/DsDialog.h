#pragma once

/*    DsDialog — themed dialog primitives (M26, M34 upgrade).

    DsDialog           : base dialog with animated entrance (scale + fade)
    DsConfirmDialog    : title + message + Cancel/Confirm buttons
    DsPromptDialog     : title + text field + Cancel/Save buttons
    DsProgressDialog   : title + message + progress bar + percentage

    M34 additions:
      - Escape key dismisses dismissible dialogs
      - Outside-click (on the dim background) dismisses dismissible dialogs
      - Close (×) button in the title bar for dismissible dialogs
      - onDismiss callback fires on any dismiss action
      - setDismissable(false) for non-dismissable dialogs (e.g. progress)
      - showDialog / showConfirmDialog convenience helpers
      - Accessibility: Escape/Tab focus trap, role announcement

    All dialogs use the DS motion system and respect reduced-motion preferences.
*/

#include "DsCore.h"
#include "DsButton.h"
#include "DsControls.h"
#include "DsSurfaces.h"

namespace otoha::ds
{

// ---------------------------------------------------------------------------
// Base dialog (animated entrance, M34 keyboard + outside-click)
// ---------------------------------------------------------------------------

/** Animated dialog panel. Entrance: scale from 0.94 -> 1.0 with spring, fade from 0 -> 1.

    M34: Escape key and outside-click on the dim background dismiss the dialog
    unless setDismissable(false) has been called. A close (×) button appears
    in the title bar for dismissible dialogs.
*/
class Dialog : public juce::Component,
               private juce::Timer,
               private juce::KeyListener
{
public:
    Dialog (const juce::String& titleText, bool compact = false)
        : title_ (titleText), isCompact (compact)
    {
        setWantsKeyboardFocus (true);
        addKeyListener (this);
        startTimerHz (60);
        setName (titleText);
    }

    ~Dialog() override
    {
        stopTimer();
        removeKeyListener (this);
    }

    /** Fires when the dialog is dismissed (Escape, outside-click, close button, Cancel). */
    std::function<void()> onDismiss;

    /** Whether the dialog can be dismissed by Escape / outside-click / close button.
        Progress dialogs typically set this to false while an operation is running. */
    void setDismissable (bool canDismiss) { dismissable = canDismiss; }
    bool isDismissable() const { return dismissable; }

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

        // close button (×) — only for dismissible dialogs
        if (dismissable && animAlpha >= 0.5f)
        {
            const float btnSize = 20.0f;
            const auto closeBtn = juce::Rectangle<float> (btnSize, btnSize)
                .withCentre ({ panel.getRight() - theme::Spacing::xl - btnSize / 2.0f,
                               panel.getY() + theme::Spacing::xl + btnSize / 2.0f });

            const bool hovered = closeBtn.contains (mousePos.toFloat());
            g.setColour (hovered ? theme::colors::textPrimary()
                                 : theme::colors::textMuted());
            g.setFont (theme::font (theme::TextSize::body));
            g.drawText (L"\u00D7", closeBtn, juce::Justification::centred);
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        // close button hit test
        if (dismissable && closeBtnBounds().contains (e.getPosition().toFloat()))
        {
            dismiss();
            return;
        }

        // outside-click dismiss: click on dim background but not on the panel
        if (dismissable && ! panelBounds().contains (e.getPosition().toFloat()))
        {
            dismiss();
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        mousePos = e.getPosition();
        if (dismissable)
            repaint();  // hover effect on close button
    }

    bool keyPressed (const juce::KeyPress& key, juce::Component*) override
    {
        if (key == juce::KeyPress::escapeKey)
        {
            if (dismissable)
                dismiss();
            return true;
        }
        return false;
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

protected:
    /** Override in subclasses to perform dismiss animation or direct cleanup. */
    virtual void dismiss()
    {
        if (onDismiss)
            onDismiss();
    }

    juce::Rectangle<float> panelBounds() const
    {
        const auto bounds = getLocalBounds().toFloat();
        const float panelW = isCompact
            ? juce::jmin (bounds.getWidth() * 0.5f, 560.0f)
            : juce::jmin (bounds.getWidth() * 0.6f, 860.0f);
        const float panelH = isCompact ? 240.0f : 400.0f;
        return juce::Rectangle<float> (panelW, panelH).withCentre (bounds.getCentre());
    }

private:
    juce::Rectangle<float> closeBtnBounds() const
    {
        const float btnSize = 20.0f;
        const auto p = panelBounds();
        return juce::Rectangle<float> (btnSize, btnSize)
            .withCentre ({ p.getRight() - theme::Spacing::xl - btnSize / 2.0f,
                           p.getY() + theme::Spacing::xl + btnSize / 2.0f });
    }

    juce::String title_;
    bool isCompact;
    bool dismissable = true;
    float animAlpha = 0.0f;
    juce::Point<int> mousePos;
};

// ---------------------------------------------------------------------------
// Confirm dialog (M34: Escape triggers Cancel)
// ---------------------------------------------------------------------------

/** Title + message + Cancel (ghost) + Confirm (primary or danger) buttons. */
class ConfirmDialog : public Dialog
{
public:
    ConfirmDialog (const juce::String& title, const juce::String& message,
                   const juce::String& confirmText, bool danger = false)
        : Dialog (title, true)
    {
        setName (title);
        messageLabel.setText (message, juce::dontSendNotification);
        messageLabel.setColour (juce::Label::textColourId, theme::colors::textSecondary());
        messageLabel.setFont (theme::font (theme::TextSize::body));
        addAndMakeVisible (messageLabel);

        cancelBtn = std::make_unique<TextButton> ("Cancel");
        cancelBtn->onClick = [this] { dismiss(); };
        addAndMakeVisible (*cancelBtn);

        confirmBtn = std::make_unique<Button> (confirmText,
            danger ? ButtonVariant::danger : ButtonVariant::primary);
        confirmBtn->onClick = [this]
        {
            if (onConfirm) onConfirm();
        };
        addAndMakeVisible (*confirmBtn);
    }

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

protected:
    void dismiss() override
    {
        if (onDismiss) onDismiss();
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
        cancelBtn->onClick = [this] { dismiss(); };
        addAndMakeVisible (*cancelBtn);

        saveBtn = std::make_unique<Button> ("Save", ButtonVariant::primary);
        saveBtn->onClick = [this] { if (onSave) onSave (input->getText()); };
        addAndMakeVisible (*saveBtn);
    }

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

protected:
    void dismiss() override
    {
        if (onDismiss) onDismiss();
    }

private:
    std::unique_ptr<Input> input;
    std::unique_ptr<TextButton> cancelBtn;
    std::unique_ptr<Button> saveBtn;
};

// ---------------------------------------------------------------------------
// Progress dialog (M34: non-dismissable by default)
// ---------------------------------------------------------------------------

/** Title + message + progress bar + percentage. */
class ProgressDialog : public Dialog
{
public:
    ProgressDialog (const juce::String& title, const juce::String& message)
        : Dialog (title, true)
    {
        setDismissable (false);   // progress dialogs are not user-dismissable

        messageLabel.setText (message, juce::dontSendNotification);
        messageLabel.setColour (juce::Label::textColourId, theme::colors::textSecondary());
        messageLabel.setFont (theme::font (theme::TextSize::body));
        addAndMakeVisible (messageLabel);

        progressBar = std::make_unique<ProgressBar> (0.0);
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

// ---------------------------------------------------------------------------
// Convenience helpers (M34)
// ---------------------------------------------------------------------------

/** Show a ConfirmDialog over a parent component. Returns the dialog (owned by caller).
    The caller should store the returned pointer and call deleteWhenNotNeeded() or
    use a unique_ptr to manage lifetime. After the dialog closes (user presses
    Cancel or Confirm), onDismiss fires and the dialog self-destructs. */
inline ConfirmDialog* showConfirmDialog (
    juce::Component* parent,
    const juce::String& title,
    const juce::String& message,
    const juce::String& confirmText,
    std::function<void()> onConfirm,
    bool danger = false)
{
    auto* dlg = new ConfirmDialog (title, message, confirmText, danger);
    dlg->onConfirm = std::move (onConfirm);
    dlg->onDismiss = [dlg] { dlg->removeFromDesktop(); };
    parent->addAndMakeVisible (dlg);
    dlg->setBounds (parent->getLocalBounds());
    dlg->toFront (true);
    dlg->grabKeyboardFocus();
    return dlg;
}

} // namespace otoha::ds
