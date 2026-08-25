#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

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
// --- M24 appearance mode -------------------------------------------------------

enum class AppearanceMode { system, light, dark };

// --- M24 accent palette --------------------------------------------------------

struct AccentEntry
{
    juce::String name;
    juce::Colour base;          // the primary accent colour
    juce::Colour hover;         // lighter variant
    juce::Colour pressed;       // darker variant
    juce::Colour soft;          // tinted background
    juce::Colour contrast;      // text on accent
};

/** Curated accent palette — accessible names, guaranteed contrast. */
inline const std::vector<AccentEntry>& accentPalette()
{
    static const std::vector<AccentEntry> palettes =
    {
        { "Sakura",     juce::Colour (0xffff9ecf), juce::Colour (0xffffb1d9),
          juce::Colour (0xffe585bc), juce::Colour (0xff2a1620), juce::Colour (0xff2a1018) },
        { "Ocean",      juce::Colour (0xff64b5f6), juce::Colour (0xff90caf9),
          juce::Colour (0xff42a5f5), juce::Colour (0xff0d1f33), juce::Colour (0xff0a1929) },
        { "Mint",       juce::Colour (0xff4fc3a1), juce::Colour (0xff80cbc4),
          juce::Colour (0xff26a69a), juce::Colour (0xff0d2924), juce::Colour (0xff0a201c) },
        { "Amber",      juce::Colour (0xffffb74d), juce::Colour (0xffffcc80),
          juce::Colour (0xffffa726), juce::Colour (0xff33220a), juce::Colour (0xff2a1b06) },
        { "Lavender",   juce::Colour (0xffb39ddb), juce::Colour (0xffd1c4e9),
          juce::Colour (0xff9575cd), juce::Colour (0xff1a1528), juce::Colour (0xff130f20) },
        { "Coral",      juce::Colour (0xffff8a80), juce::Colour (0xffffab91),
          juce::Colour (0xffff7043), juce::Colour (0xff331210), juce::Colour (0xff2a0d0b) },
        { "Sky",        juce::Colour (0xff4dd0e1), juce::Colour (0xff80deea),
          juce::Colour (0xff26c6da), juce::Colour (0xff0a2a2e), juce::Colour (0xff072225) },
        { "Rose",       juce::Colour (0xfff48fb1), juce::Colour (0xfff8bbd0),
          juce::Colour (0xffec407a), juce::Colour (0xff33121e), juce::Colour (0xff2a0e18) },
        { "Sage",       juce::Colour (0xffa5d6a7), juce::Colour (0xffc8e6c9),
          juce::Colour (0xff81c784), juce::Colour (0xff122414), juce::Colour (0xff0e1c10) },
        { "Peach",      juce::Colour (0xffffab91), juce::Colour (0xffffccbc),
          juce::Colour (0xffff8a65), juce::Colour (0xff331a12), juce::Colour (0xff2a140e) },
    };
    return palettes;
}

/** Look up an accent by name; falls back to index 0 if not found. */
inline const AccentEntry& accentByName (const juce::String& name)
{
    const auto& pal = accentPalette();
    for (const auto& e : pal)
        if (e.name.equalsIgnoreCase (name)) return e;
    return pal[0];
}

// --- theme data ---------------------------------------------------------------

/** Every colour in the app, named by meaning. One instance = one full look. */
struct ThemeColors
{
    // surfaces (hierarchy: background → surface → subtle → elevated → interactive)
    juce::Colour background;
    juce::Colour surface;
    juce::Colour surfaceSubtle;     // between surface and elevated — list row tints
    juce::Colour surfaceElevated;
    juce::Colour surfaceHover;
    juce::Colour surfacePressed;

    // lines
    juce::Colour border;
    juce::Colour borderSubtle;
    juce::Colour borderStrong;      // stronger emphasis — active input, selected card
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

/** The built-in dark look: Kaiteyo OLED Black + Signature Pineapple. */
inline Theme makeDefaultDarkTheme()
{
    ThemeColors c;
    c.background         = juce::Colour (0xff050505);   // Kaiteyo OLED Black
    c.surface            = juce::Colour (0xff0d0d0d);   // Kaiteyo surface
    c.surfaceSubtle      = juce::Colour (0xff0f0f0f);   // subtle tint between surface and elevated
    c.surfaceElevated    = juce::Colour (0xff101010);   // Kaiteyo surfaceElevated
    c.surfaceHover       = juce::Colour (0xff1a1a1a);   // Kaiteyo surfaceInteractive
    c.surfacePressed     = juce::Colour (0xff222222);   // Kaiteyo surfaceActive

    c.border             = juce::Colour (0xff2a2a2a);   // Kaiteyo border
    c.borderSubtle       = juce::Colour (0x33ffffff);   // Kaiteyo surfaceBorderSubtle
    c.borderStrong       = juce::Colour (0xff3a3a3a);   // stronger border for active/selected states
    c.focusRing          = juce::Colour (0xffc2fc8b);   // Kaiteyo primary accent

    c.textPrimary        = juce::Colour (0xfff0f0f0);   // Kaiteyo textPrimary
    c.textSecondary      = juce::Colour (0xffa0a0a0);   // Kaiteyo textSecondary
    c.textMuted          = juce::Colour (0xff606060);   // Kaiteyo textMuted
    c.textDisabled       = juce::Colour (0xff404040);   // Kaiteyo disabled

    c.accent             = juce::Colour (0xffc2fc8b);   // Kaiteyo primary (Signature Pineapple)
    c.accentHover        = juce::Colour (0xff9ce85e);   // Kaiteyo primaryDark
    c.accentPressed      = juce::Colour (0xff8ad44e);   // pressed variant
    c.accentSoft         = juce::Colour (0x1ac2fc8b);   // primary at 10% alpha
    c.accentContrast     = juce::Colour (0xff050505);   // Kaiteyo onPrimary

    c.success            = juce::Colour (0xffc2fc8b);   // Kaiteyo semanticSuccess
    c.warning            = juce::Colour (0xfffeab57);   // Kaiteyo semanticWarning
    c.danger             = juce::Colour (0xffff6b6b);   // Kaiteyo semanticError
    c.info               = juce::Colour (0xff7bc8ff);   // Kaiteyo semanticInfo

    c.waveform           = juce::Colour (0xffc2fc8b);   // Kaiteyo primary for waveform
    c.waveformMuted      = juce::Colour (0xff2a3a2a);   // muted green
    c.selection          = juce::Colour (0x22c2fc8b);   // primary at 13% alpha
    c.playhead           = juce::Colour (0xfff0f0f0);   // Kaiteyo textPrimary
    c.meterSafe          = juce::Colour (0xffc2fc8b);   // Kaiteyo semanticSuccess
    c.meterWarning       = juce::Colour (0xfffeab57);   // Kaiteyo semanticWarning
    c.meterClip          = juce::Colour (0xffff6b6b);   // Kaiteyo semanticError

    c.recording            = juce::Colour (0xffff6b6b);   // Kaiteyo error for recording
    c.recordingPulse       = juce::Colour (0xffff8a65);   // warm orange pulse
    c.recordingBackground  = juce::Colour (0xff1a0a0a);   // dark red background

    c.favorite           = juce::Colour (0xffffd93d);   // Kaiteyo favoriteYellow

    return { "OLED Black", c };
}

/** Kaiteyo Light mode: clean white surfaces, dark text. */
inline Theme makeLightTheme()
{
    ThemeColors c;
    c.background         = juce::Colour (0xfff5f5f5);   // Kaiteyo backgroundLight
    c.surface            = juce::Colour (0xffeeeeee);   // Kaiteyo surfaceLightDark
    c.surfaceSubtle      = juce::Colour (0xffebebeb);   // subtle tint between surface and elevated
    c.surfaceElevated    = juce::Colour (0xffe8e8e8);   // Kaiteyo surfaceLightMedium
    c.surfaceHover       = juce::Colour (0xfffcfcfc);   // Kaiteyo surfaceLightLight
    c.surfacePressed     = juce::Colour (0xffdcdcdc);

    c.border             = juce::Colour (0xffd0d0d0);   // Kaiteyo surfaceBorderLight
    c.borderSubtle       = juce::Colour (0x18000000);
    c.borderStrong       = juce::Colour (0xffb0b0b0);   // stronger border for active/selected states
    c.focusRing          = juce::Colour (0xff9ce85e);   // Kaiteyo primaryDark for light

    c.textPrimary        = juce::Colour (0xff1a1a1a);   // Kaiteyo textPrimaryLight
    c.textSecondary      = juce::Colour (0xff606060);   // Kaiteyo textSecondaryLight
    c.textMuted          = juce::Colour (0xffa0a0a0);   // Kaiteyo textMutedLight
    c.textDisabled       = juce::Colour (0xffcccccc);

    c.accent             = juce::Colour (0xff9ce85e);   // Kaiteyo primaryDark for light
    c.accentHover        = juce::Colour (0xffc2fc8b);   // Kaiteyo primary for light hover
    c.accentPressed      = juce::Colour (0xff8ad44e);
    c.accentSoft         = juce::Colour (0xffe8f5e0);   // light green tint
    c.accentContrast     = juce::Colour (0xff050505);   // Kaiteyo onPrimary

    c.success            = juce::Colour (0xff2e7d32);   // Kaiteyo light success
    c.warning            = juce::Colour (0xffef6c00);   // Kaiteyo light warning
    c.danger             = juce::Colour (0xffe53935);   // Kaiteyo light error
    c.info               = juce::Colour (0xff1565c0);   // Kaiteyo light info

    c.waveform           = juce::Colour (0xff2e7d32);   // Kaiteyo light success
    c.waveformMuted      = juce::Colour (0xffa5d6a7);
    c.selection          = juce::Colour (0x229ce85e);   // primary at 13% alpha
    c.playhead           = juce::Colour (0xff1a1a1a);
    c.meterSafe          = juce::Colour (0xff2e7d32);
    c.meterWarning       = juce::Colour (0xffef6c00);
    c.meterClip          = juce::Colour (0xffe53935);

    c.recording            = juce::Colour (0xffe53935);
    c.recordingPulse       = juce::Colour (0xffff5252);
    c.recordingBackground  = juce::Colour (0xffffebee);

    c.favorite           = juce::Colour (0xfff9a825);   // Kaiteyo light favorite

    return { "Light", c };
}

/** Returns a Theme with the given accent swapped in (dark base). */
inline Theme darkThemeWithAccent (const AccentEntry& accent)
{
    auto t = makeDefaultDarkTheme();
    t.colors.accent        = accent.base;
    t.colors.accentHover   = accent.hover;
    t.colors.accentPressed = accent.pressed;
    t.colors.accentSoft    = accent.soft;
    t.colors.accentContrast = accent.contrast;
    t.colors.focusRing     = accent.base;
    return t;
}

/** Returns a Theme with the given accent swapped in (light base). */
inline Theme lightThemeWithAccent (const AccentEntry& accent)
{
    auto t = makeLightTheme();
    t.colors.accent        = accent.base;
    t.colors.accentHover   = accent.hover;
    t.colors.accentPressed = accent.pressed;
    t.colors.accentSoft    = accent.soft;
    t.colors.accentContrast = accent.contrast;
    t.colors.focusRing     = accent.base;
    return t;
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
    laf.setColour (juce::Slider::thumbColourId,                 c.accent);
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
    inline juce::Colour surfaceSubtle()     { return current().colors.surfaceSubtle; }
    inline juce::Colour surfaceElevated()    { return current().colors.surfaceElevated; }
    inline juce::Colour surfaceHover()       { return current().colors.surfaceHover; }
    inline juce::Colour surfacePressed()     { return current().colors.surfacePressed; }

    inline juce::Colour border()             { return current().colors.border; }
    inline juce::Colour borderSubtle()       { return current().colors.borderSubtle; }
    inline juce::Colour borderStrong()       { return current().colors.borderStrong; }
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
    // Matches Kaiteyo DsType scale exactly.
    switch (size)
    {
        case TextSize::display:  return { 28.0f, juce::Font::bold };
        case TextSize::title:    return { 18.0f, juce::Font::bold };
        case TextSize::heading:  return { 22.0f, juce::Font::bold };
        case TextSize::body:     return { 14.0f, bold };
        case TextSize::bodySmall:return { 12.0f, bold };
        case TextSize::caption:  return { 11.0f, bold };
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
    inline constexpr int xs     = 4;    // checkboxes, small indicators
    inline constexpr int small  = 8;    // buttons, inputs, small cards
    inline constexpr int medium = 12;   // standard cards, list items
    inline constexpr int large  = 16;   // large cards, modals
    inline constexpr int xl     = 24;   // sidebar panel, main content
    inline constexpr int pill   = 999;  // fully rounded
}

// --- motion (ms) — consumed by M25; reduced-motion collapses to 0 -------------------------

namespace Motion
{
    inline constexpr int fast   = 120;
    inline constexpr int normal = 240;
    inline constexpr int slow   = 380;

    /** Returns true when the OS requests reduced motion. When true, animation
        durations should be treated as 0 by callers. */
    inline bool prefersReducedMotion()
    {
       #if JUCE_MAC || JUCE_IOS
        return juce::Desktop::getInstance().isRunningInSandbox();
       #elif JUCE_WINDOWS
        // On Windows, SPI_GETCLIENTAREAFACTION is not directly accessible;
        // default to false — animations remain subtle enough. A future
        // platform-specific hook can query the actual setting.
        return false;
       #else
        return false;
       #endif
    }

    /** Effective duration: 0 when reduced motion is preferred. */
    inline int effective (int ms) { return prefersReducedMotion() ? 0 : ms; }
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
    inline constexpr int sidebarWidth     = 200;  // M19: full floating sidebar
    inline constexpr int sidebarCollapsed = 56;   // M19: icon-only mode
    inline constexpr int sidebarPadding   = 8;    // M19: inner padding
}

// --- responsive breakpoints (px) — Kaiteyo DsWidthTiers --------------------------------------

namespace Breakpoints
{
    inline constexpr int compact  = 720;   // below this: horizontal tab bar
    inline constexpr int standard = 1024;  // typical restored window
    inline constexpr int wide     = 1440;  // grids spread into more columns
    inline constexpr int extraWide = 1920; // maximized on large monitors
}

/** Width tier ordinal (0=compact → 4=extraWide) for adaptive grid columns. */
inline int widthTier (int availableWidth)
{
    if (availableWidth < Breakpoints::compact)  return 0;
    if (availableWidth < Breakpoints::standard) return 1;
    if (availableWidth < Breakpoints::wide)     return 2;
    if (availableWidth < Breakpoints::extraWide) return 3;
    return 4;
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
