#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/*
    OtohaTheme — the one place that knows what Otoha looks like (M14 #1/#63, M17).

    M17 turned the M14 constants into a *token system*:

      * `Theme` holds every semantic colour as data, so the whole app can be
        re-skinned at runtime (M24's appearance picker) without touching views.
      * `otoha::theme::current()` returns the active Theme; `setTheme()` swaps
        it and broadcasts via `themeChangedBroadcaster()` so components can
        repaint/re-apply their colours.
      * Spacing / Radius / Typography / Metrics / Motion are centralised scales.
      * Views ask "what does this colour *mean*" (textMuted, danger, accent…),
        never "what hex value is it".

    Personality: quiet, confident, audio-first. AMOLED black, restrained sakura
    accent, generous spacing, few corners.

    Typography scale — seven sizes, no more:
        display  34 bold   (brand / Home hero)
        title    20 bold   (screen titles: editor, library header)
        heading  16 bold   (group headers, panel titles)
        body     15        (primary copy, subtitles)
        bodySmall 14       (default control text)
        caption  13        (durations, secondary metadata)
        button   14 bold   (button labels)

    Rules for consumers (enforced by review, not the compiler):
      * No `juce::Colour (0x...)` literals outside this file.
      * No opacity hacks for text hierarchy — use the text tokens.
      * Audio/DSP numbers are NOT UI tokens; they stay in their modules.
*/

namespace otoha::theme
{
// --- theme data ---------------------------------------------------------------

/** Every colour in the app, named by meaning. One instance = one full look. */
struct ThemeColors
{
    // surfaces (hierarchy: background → surface → elevated → interactive)
    juce::Colour background;
    juce::Colour surface;
    juce::Colour surfaceElevated;
    juce::Colour surfaceHover;
    juce::Colour surfacePressed;

    // lines
    juce::Colour border;
    juce::Colour borderSubtle;
    juce::Colour focusRing;

    // text
    juce::Colour textPrimary;
    juce::Colour textSecondary;
    juce::Colour textMuted;
    juce::Colour textDisabled;

    // the one accent (record button, selection, primary actions all derive)
    juce::Colour accent;
    juce::Colour accentHover;
    juce::Colour accentPressed;
    juce::Colour accentSoft;      // quiet accent-tinted panel/button background
    juce::Colour accentContrast;  // readable text/icon colour on top of accent

    // semantic states
    juce::Colour success;
    juce::Colour warning;
    juce::Colour danger;
    juce::Colour info;

    // audio visualization
    juce::Colour waveform;
    juce::Colour waveformMuted;
    juce::Colour selection;       // opaque selection background (lists/rows)
    juce::Colour playhead;
    juce::Colour meterSafe;
    juce::Colour meterWarning;
    juce::Colour meterClip;

    // recording state
    juce::Colour recording;           // record button / REC indicator
    juce::Colour recordingPulse;      // pulsing halo while recording
    juce::Colour recordingBackground; // dark red-tinted backing (meter track, chips)

    // library extras
    juce::Colour favorite;
};

/** A complete named look. M24 adds persisted base modes/accents on top of this. */
struct Theme
{
    juce::String name;
    ThemeColors colors;
};

/** The built-in dark look: AMOLED black + sakura. Matches the M14 product. */
inline Theme makeDefaultDarkTheme()
{
    ThemeColors c;
    c.background         = juce::Colour (0xff000000);
    c.surface            = juce::Colour (0xff141414);
    c.surfaceElevated    = juce::Colour (0xff1c1c1e);
    c.surfaceHover       = juce::Colour (0xff242424);
    c.surfacePressed     = juce::Colour (0xff0d0d0d);

    c.border             = juce::Colour (0xff2a2a2a);
    c.borderSubtle       = juce::Colour (0x18ffffff);
    c.focusRing          = juce::Colour (0xffff9ecf);

    c.textPrimary        = juce::Colours::white;
    c.textSecondary      = juce::Colour (0xffd8c7ce);
    c.textMuted          = juce::Colour (0xff8a7a82);
    c.textDisabled       = juce::Colour (0xff5a5058);

    c.accent             = juce::Colour (0xffff9ecf);
    c.accentHover        = juce::Colour (0xffffb1d9);
    c.accentPressed      = juce::Colour (0xffe585bc);
    c.accentSoft         = juce::Colour (0xff2a1620);
    c.accentContrast     = juce::Colour (0xff2a1018);

    c.success            = juce::Colour (0xff4fc3a1);
    c.warning            = juce::Colour (0xffe8c35a);
    c.danger             = juce::Colour (0xffe05252);
    c.info               = juce::Colour (0xff7bc8ff);

    c.waveform           = juce::Colour (0xff4fc3a1);
    c.waveformMuted      = juce::Colour (0xff2f584c);
    c.selection          = juce::Colour (0xff2b3a36);
    c.playhead           = juce::Colours::white;
    c.meterSafe          = juce::Colour (0xff4fc3a1);
    c.meterWarning       = juce::Colour (0xffe8c35a);
    c.meterClip          = juce::Colour (0xffff5a7e);

    c.recording            = juce::Colour (0xffe05252);
    c.recordingPulse       = juce::Colour (0xffff7a6e);
    c.recordingBackground  = juce::Colour (0xff3a1512);

    c.favorite           = juce::Colour (0xffe8c35a);

    return { "Dark (AMOLED + Sakura)", c };
}

namespace detail
{
    inline Theme& activeTheme()
    {
        static Theme t = makeDefaultDarkTheme();
        return t;
    }

    inline juce::ChangeBroadcaster& changeBroadcaster()
    {
        static juce::ChangeBroadcaster b;
        return b;
    }
}

// --- theme API ------------------------------------------------------------------

/** The active look. Views read tokens through here (see `colors` below). */
inline const Theme& current()                    { return detail::activeTheme(); }

/** Swap the whole look at runtime and notify every listener synchronously (M24). */
inline void setTheme (const Theme& t)
{
    detail::activeTheme() = t;
    detail::changeBroadcaster().sendSynchronousChangeMessage();
}

/** Components add a ChangeListener to this to re-apply colours when the theme changes. */
inline juce::ChangeBroadcaster& themeChangedBroadcaster() { return detail::changeBroadcaster(); }

/** Re-applies the active theme to the window/desktop-wide JUCE colour ids. */
inline void applyToDesktopLookAndFeel()
{
    auto& laf = juce::Desktop::getInstance().getDefaultLookAndFeel();
    const auto& c = current().colors;
    laf.setColour (juce::ResizableWindow::backgroundColourId,   c.background);
    laf.setColour (juce::AlertWindow::backgroundColourId,       c.surface);
    laf.setColour (juce::AlertWindow::textColourId,             c.textPrimary);
    laf.setColour (juce::AlertWindow::outlineColourId,          c.border);
    laf.setColour (juce::ComboBox::backgroundColourId,          c.surface);
    laf.setColour (juce::ComboBox::textColourId,                c.textPrimary);
    laf.setColour (juce::ComboBox::outlineColourId,             c.border);
    laf.setColour (juce::ComboBox::buttonColourId,              c.surfaceElevated);
    laf.setColour (juce::PopupMenu::backgroundColourId,         c.surfaceElevated);
    laf.setColour (juce::PopupMenu::textColourId,               c.textPrimary);
    laf.setColour (juce::PopupMenu::highlightedBackgroundColourId, c.accentSoft);
    laf.setColour (juce::PopupMenu::highlightedTextColourId,    c.accent);
    laf.setColour (juce::TextEditor::backgroundColourId,        c.surface);
    laf.setColour (juce::TextEditor::textColourId,              c.textPrimary);
    laf.setColour (juce::TextEditor::outlineColourId,           c.border);
    laf.setColour (juce::TextEditor::focusedOutlineColourId,    c.focusRing);
    laf.setColour (juce::TextButton::buttonColourId,            c.surfaceElevated);
    laf.setColour (juce::TextButton::textColourOffId,           c.textPrimary);
    laf.setColour (juce::ToggleButton::textColourId,            c.textPrimary);
    laf.setColour (juce::ListBox::backgroundColourId,           c.background);
    laf.setColour (juce::Label::textColourId,                   c.textPrimary);
    laf.setColour (juce::Slider::backgroundColourId,            c.surfaceHover);
    laf.setColour (juce::Slider::trackColourId,                 c.accent);
    laf.setColour (juce::Slider::thumbColourId,                 c.textPrimary);
    laf.setColour (juce::Slider::textBoxTextColourId,           c.textPrimary);
    laf.setColour (juce::ScrollBar::backgroundColourId,         c.surface);
    laf.setColour (juce::ScrollBar::thumbColourId,              c.surfaceHover);
    laf.setColour (juce::TooltipWindow::backgroundColourId,     c.surfaceElevated);
    laf.setColour (juce::TooltipWindow::textColourId,           c.textPrimary);
}

// --- colour tokens ----------------------------------------------------------------
// Access the ACTIVE theme's colours through these; never cache the values.

namespace colors
{
    inline juce::Colour background()         { return current().colors.background; }
    inline juce::Colour surface()            { return current().colors.surface; }
    inline juce::Colour surfaceElevated()    { return current().colors.surfaceElevated; }
    inline juce::Colour surfaceHover()       { return current().colors.surfaceHover; }
    inline juce::Colour surfacePressed()     { return current().colors.surfacePressed; }

    inline juce::Colour border()             { return current().colors.border; }
    inline juce::Colour borderSubtle()       { return current().colors.borderSubtle; }
    inline juce::Colour focusRing()          { return current().colors.focusRing; }

    inline juce::Colour textPrimary()        { return current().colors.textPrimary; }
    inline juce::Colour textSecondary()      { return current().colors.textSecondary; }
    inline juce::Colour textMuted()          { return current().colors.textMuted; }
    inline juce::Colour textDisabled()       { return current().colors.textDisabled; }

    inline juce::Colour accent()             { return current().colors.accent; }
    inline juce::Colour accentHover()        { return current().colors.accentHover; }
    inline juce::Colour accentPressed()      { return current().colors.accentPressed; }
    inline juce::Colour accentSoft()         { return current().colors.accentSoft; }
    inline juce::Colour accentContrast()     { return current().colors.accentContrast; }

    inline juce::Colour success()            { return current().colors.success; }
    inline juce::Colour warning()            { return current().colors.warning; }
    inline juce::Colour danger()             { return current().colors.danger; }
    inline juce::Colour info()               { return current().colors.info; }

    inline juce::Colour waveform()           { return current().colors.waveform; }
    inline juce::Colour waveformMuted()      { return current().colors.waveformMuted; }
    inline juce::Colour selection()          { return current().colors.selection; }
    inline juce::Colour playhead()           { return current().colors.playhead; }
    inline juce::Colour meterSafe()          { return current().colors.meterSafe; }
    inline juce::Colour meterWarning()       { return current().colors.meterWarning; }
    inline juce::Colour meterClip()          { return current().colors.meterClip; }

    inline juce::Colour recording()            { return current().colors.recording; }
    inline juce::Colour recordingPulse()       { return current().colors.recordingPulse; }
    inline juce::Colour recordingBackground()  { return current().colors.recordingBackground; }

    inline juce::Colour favorite()           { return current().colors.favorite; }
}

// --- typography ---------------------------------------------------------------------

enum class TextSize { display, title, heading, body, bodySmall, caption, button };

inline juce::FontOptions font (TextSize size, bool bold = false)
{
    switch (size)
    {
        case TextSize::display:  return { 34.0f, juce::Font::bold };
        case TextSize::title:    return { 20.0f, juce::Font::bold };
        case TextSize::heading:  return { 16.0f, juce::Font::bold };
        case TextSize::body:     return { 15.0f, bold };
        case TextSize::bodySmall:return { 14.0f, bold };
        case TextSize::caption:  return { 13.0f, bold };
        case TextSize::button:   return { 14.0f, juce::Font::bold };
    }
    return { 14.0f, bold };
}

// --- spacing scale (px) ---------------------------------------------------------------

namespace Spacing
{
    inline constexpr int xs  = 4;
    inline constexpr int sm  = 8;
    inline constexpr int md  = 12;
    inline constexpr int lg  = 16;
    inline constexpr int xl  = 24;   // screen margins
    inline constexpr int xxl = 32;
}

// --- corner radius (px) -----------------------------------------------------------------

namespace Radius
{
    inline constexpr int small  = 6;    // chips, tags
    inline constexpr int medium = 8;    // buttons, inputs, cards
    inline constexpr int large  = 12;   // panels, dialogs
    inline constexpr int pill   = 999;  // fully rounded
}

// --- motion (ms) — consumed by M25; reduced-motion collapses to 0 -------------------------

namespace Motion
{
    inline constexpr int fast   = 120;
    inline constexpr int normal = 240;
    inline constexpr int slow   = 380;
}

// --- component metrics (px) — only values that recur ----------------------------------------

namespace Metrics
{
    inline constexpr int rowHeight        = 40;
    inline constexpr int buttonHeight     = 36;
    inline constexpr int inputHeight      = 30;
    inline constexpr int titleStripHeight = 54;   // "Otoha / <screen>" strip
    inline constexpr int iconSize         = 18;
    inline constexpr int cardPadding      = 16;
    inline constexpr int controlGap       = 12;
    inline constexpr int touchTargetMin   = 44;
    inline constexpr int sidebarWidth     = 240;  // reserved for M19
}

// Back-compat aliases (legacy M14 names) — prefer the namespaces above.
inline constexpr int edgePadding  = Spacing::xl;
inline constexpr int cornerRadius = Radius::medium;
inline constexpr int rowHeight    = Metrics::rowHeight;

// Legacy M14 colour free-functions — kept so older call sites keep working.
// New code must use the `colors::` accessors instead.
inline juce::Colour sakura()    { return colors::accent(); }
inline juce::Colour card()      { return colors::surfaceElevated(); }
inline juce::Colour textSoft()  { return colors::textSecondary(); }
inline juce::Colour textMuted() { return colors::textMuted(); }
inline juce::Colour background(){ return colors::background(); }
inline juce::Colour clipRed()   { return colors::meterClip(); }

// --- shared widget styling --------------------------------------------------------------------

/** Standard card/secondary button: elevated surface, quiet border. */
inline void styleCardButton (juce::TextButton& b)
{
    b.setColour (juce::TextButton::buttonColourId,   colors::surfaceElevated());
    b.setColour (juce::TextButton::buttonOnColourId, colors::surfaceHover());
    b.setColour (juce::TextButton::textColourOffId,  colors::textPrimary());
    b.setColour (juce::TextButton::textColourOnId,   colors::accent());
}

/** The dominant primary action (Record on Home). Accent-tinted, accent-labelled. */
inline void stylePrimaryButton (juce::TextButton& b)
{
    b.setColour (juce::TextButton::buttonColourId,    colors::accentSoft());
    b.setColour (juce::TextButton::buttonOnColourId,  colors::accentSoft().brighter (0.25f));
    b.setColour (juce::TextButton::textColourOffId,   colors::accent());
    b.setColour (juce::TextButton::textColourOnId,    colors::accent());
}

/** Every interactive control gets a semantic name (#56) — this makes the
    intent impossible to forget at call sites. */
inline void label (juce::Component& c, const juce::String& accessibleName,
                   const juce::String& tooltipText = {})
{
    c.setName (accessibleName);
    c.setDescription (accessibleName);
    if (tooltipText.isNotEmpty())
        c.setHelpText (tooltipText);
}
} // namespace otoha::theme
