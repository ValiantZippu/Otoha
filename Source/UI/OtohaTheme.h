#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/*
    OtohaTheme — the one place that knows what Otoha looks like (M14 #1/#63).

    Personality: quiet, confident, audio-first. AMOLED black, restrained sakura
    accent, generous spacing, few corners. Every view reads from here so the
    product stays consistent (#72); the values below ARE the previous
    hand-tuned literals, now named.

    Typography scale — six sizes, no more:
        display 34 bold   (brand / Home hero)
        title  20 bold    (screen titles: editor, library header)
        section 16 bold   (group headers: "Recent")
        body   15         (primary copy, subtitles)
        ui     14         (default control text)
        caption 13        (durations, secondary metadata)
*/

namespace otoha::theme
{
// --- palette -----------------------------------------------------------------
inline juce::Colour background()      { return { 0xff000000 }; }   // true AMOLED black
inline juce::Colour card()            { return { 0xff141414 }; }   // cards, secondary buttons
inline juce::Colour cardAccent()      { return { 0xff2a1620 }; }   // primary-action background
inline juce::Colour sakura()          { return { 0xffff9ecf }; }   // THE accent
inline juce::Colour textPrimary()     { return juce::Colours::white; }
inline juce::Colour textMuted()       { return { 0xff8a7a82 }; }   // warm grey-sakura
inline juce::Colour textSoft()        { return { 0xffd8c7ce }; }   // soft rose for dark panels
inline juce::Colour clipRed()         { return { 0xffff5a7e }; }   // clipping / error states

inline constexpr int cornerRadius = 8;       // restrained rounding, never bubbly
inline constexpr int edgePadding  = 24;      // screen margins
inline constexpr int rowHeight    = 40;      // list rows

// --- typography ---------------------------------------------------------------
enum class TextSize { display, title, section, body, ui, caption };

inline juce::FontOptions font (TextSize size, bool bold = false)
{
    switch (size)
    {
        case TextSize::display: return { 34.0f, juce::Font::bold };
        case TextSize::title:   return { 20.0f, juce::Font::bold };
        case TextSize::section: return { 16.0f, juce::Font::bold };
        case TextSize::body:    return { 15.0f, false };
        case TextSize::ui:      return { 14.0f, false };
        case TextSize::caption: return { 13.0f, false };
    }
    return { 14.0f, bold };
}

// --- shared widget styling ------------------------------------------------------
/** Standard card/secondary button. */
inline void styleCardButton (juce::TextButton& b)
{
    b.setColour (juce::TextButton::buttonColourId, card());
    b.setColour (juce::TextButton::textColourOffId, textPrimary());
}

/** The dominant primary action (Record). Hover/pressed/down states come from
    JUCE's button colours; we set the trio explicitly for a consistent feel. */
inline void stylePrimaryButton (juce::TextButton& b)
{
    b.setColour (juce::TextButton::buttonColourId,    cardAccent());
    b.setColour (juce::TextButton::buttonOnColourId,  cardAccent().brighter (0.25f));
    b.setColour (juce::TextButton::textColourOffId,   sakura());
    b.setColour (juce::TextButton::textColourOnId,    sakura());
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
