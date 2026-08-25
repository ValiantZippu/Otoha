#pragma once

/*
    OtohaIcons — central vector icon registry (M19).

    Every icon is a unit-square juce::Path that scales via getTransformToScaleToFit().
    No raster assets, no font glyphs, no mojibake — pure vector math.

    Consumed by the sidebar navigation, toolbar buttons, and anywhere that needs
    a semantic icon. All icons respect the caller's fill colour.

    Icon sizes live in OtohaTheme::Metrics — screens never invent sizes.
*/

#include <juce_graphics/juce_graphics.h>

namespace otoha::icons
{

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------

/** Home / Studio — house silhouette. */
inline juce::Path home()
{
    juce::Path p;
    // roof peak
    p.startNewSubPath (0.5f, 0.0f);
    p.lineTo (1.0f, 0.45f);
    p.lineTo (0.82f, 0.45f);
    p.lineTo (0.82f, 1.0f);
    p.lineTo (0.58f, 1.0f);
    p.lineTo (0.58f, 0.72f);
    p.lineTo (0.42f, 0.72f);
    p.lineTo (0.42f, 1.0f);
    p.lineTo (0.18f, 1.0f);
    p.lineTo (0.18f, 0.45f);
    p.lineTo (0.0f, 0.45f);
    p.closeSubPath();
    return p;
}

/** Record — solid circle (record button metaphor). */
inline juce::Path record()
{
    juce::Path p;
    p.addEllipse (0.15f, 0.15f, 0.7f, 0.7f);
    return p;
}

/** Library — two vertical rectangles (bookshelf). */
inline juce::Path library()
{
    juce::Path p;
    p.addRoundedRectangle (0.08f, 0.1f, 0.32f, 0.8f, 0.04f);
    p.addRoundedRectangle (0.42f, 0.2f, 0.26f, 0.7f, 0.04f);
    p.addRoundedRectangle (0.72f, 0.05f, 0.2f, 0.85f, 0.04f);
    return p;
}

/** Sound / Audio — speaker cone with wave arcs. */
inline juce::Path sound()
{
    juce::Path p;
    // speaker body
    p.addRoundedRectangle (0.08f, 0.32f, 0.3f, 0.36f, 0.03f);
    // cone
    p.startNewSubPath (0.38f, 0.32f);
    p.lineTo (0.68f, 0.1f);
    p.lineTo (0.68f, 0.9f);
    p.lineTo (0.38f, 0.68f);
    p.closeSubPath();
    return p;
}

/** Settings — gear/cog (simplified: circle with notch marks). */
inline juce::Path settings()
{
    juce::Path p;
    // outer ring
    p.addEllipse (0.2f, 0.2f, 0.6f, 0.6f);
    // centre hole
    p.addEllipse (0.38f, 0.38f, 0.24f, 0.24f);
    // Four notch marks at cardinal points (teeth)
    p.addRectangle (0.44f, 0.0f, 0.12f, 0.18f);
    p.addRectangle (0.44f, 0.82f, 0.12f, 0.18f);
    p.addRectangle (0.0f, 0.44f, 0.18f, 0.12f);
    p.addRectangle (0.82f, 0.44f, 0.18f, 0.12f);
    return p;
}

// ---------------------------------------------------------------------------
// Toolbar / Transport
// ---------------------------------------------------------------------------

/** Play triangle pointing right. */
inline juce::Path play()
{
    juce::Path p;
    p.addTriangle (0.15f, 0.0f, 1.0f, 0.5f, 0.15f, 1.0f);
    return p;
}

/** Pause — two vertical bars. */
inline juce::Path pause()
{
    juce::Path p;
    p.addRectangle (0.15f, 0.0f, 0.25f, 1.0f);
    p.addRectangle (0.6f, 0.0f, 0.25f, 1.0f);
    return p;
}

/** Stop — square. */
inline juce::Path stop()
{
    juce::Path p;
    p.addRectangle (0.15f, 0.15f, 0.7f, 0.7f);
    return p;
}

/** Undo — counter-clockwise arrow arc. */
inline juce::Path undo()
{
    juce::Path p;
    p.addArc (0.05f, 0.1f, 0.75f, 0.7f,
              juce::MathConstants<float>::halfPi,
              juce::MathConstants<float>::twoPi + juce::MathConstants<float>::halfPi * 0.5f,
              true);
    p.lineTo (0.6f, 0.0f);
    p.lineTo (0.8f, 0.0f);
    p.closeSubPath();
    return p;
}

/** Redo — clockwise arrow arc (mirror of undo). */
inline juce::Path redo()
{
    juce::Path p;
    p.addArc (0.2f, 0.1f, 0.75f, 0.7f,
              juce::MathConstants<float>::halfPi * 1.5f,
              juce::MathConstants<float>::twoPi,
              true);
    p.lineTo (0.4f, 0.0f);
    p.lineTo (0.2f, 0.0f);
    p.closeSubPath();
    return p;
}

/** Close / dismiss — X mark. */
inline juce::Path close()
{
    juce::Path p;
    p.startNewSubPath (0.15f, 0.15f); p.lineTo (0.85f, 0.85f);
    p.startNewSubPath (0.85f, 0.15f); p.lineTo (0.15f, 0.85f);
    p.closeSubPath();
    return p;
}

/** Back / chevron-left. */
inline juce::Path back()
{
    juce::Path p;
    p.startNewSubPath (0.7f, 0.1f);
    p.lineTo (0.25f, 0.5f);
    p.lineTo (0.7f, 0.9f);
    return p;
}

/** Forward / chevron-right. */
inline juce::Path forward()
{
    juce::Path p;
    p.startNewSubPath (0.3f, 0.1f);
    p.lineTo (0.75f, 0.5f);
    p.lineTo (0.3f, 0.9f);
    return p;
}

/** Search — magnifying glass. */
inline juce::Path search()
{
    juce::Path p;
    p.addEllipse (0.05f, 0.05f, 0.55f, 0.55f);
    p.startNewSubPath (0.48f, 0.5f);
    p.lineTo (0.9f, 0.92f);
    p.closeSubPath();
    return p;
}

/** More / ellipsis (three dots). */
inline juce::Path more()
{
    juce::Path p;
    p.addEllipse (0.1f, 0.38f, 0.18f, 0.18f);
    p.addEllipse (0.41f, 0.38f, 0.18f, 0.18f);
    p.addEllipse (0.72f, 0.38f, 0.18f, 0.18f);
    return p;
}

/** Plus / add. */
inline juce::Path plus()
{
    juce::Path p;
    p.startNewSubPath (0.5f, 0.08f); p.lineTo (0.5f, 0.92f);
    p.startNewSubPath (0.08f, 0.5f); p.lineTo (0.92f, 0.5f);
    p.closeSubPath();
    return p;
}

/** Trash / delete. */
inline juce::Path trash()
{
    juce::Path p;
    // lid
    p.addRoundedRectangle (0.15f, 0.08f, 0.7f, 0.12f, 0.04f);
    // handle
    p.startNewSubPath (0.35f, 0.08f);
    p.lineTo (0.35f, 0.0f);
    p.lineTo (0.65f, 0.0f);
    p.lineTo (0.65f, 0.08f);
    // bin body
    p.addRoundedRectangle (0.2f, 0.22f, 0.6f, 0.7f, 0.04f);
    return p;
}

/** Checkmark. */
inline juce::Path check()
{
    juce::Path p;
    p.startNewSubPath (0.1f, 0.52f);
    p.lineTo (0.38f, 0.82f);
    p.lineTo (0.9f, 0.18f);
    return p;
}

/** Waveform / audio — sine wave shape. */
inline juce::Path waveform()
{
    juce::Path p;
    p.startNewSubPath (0.0f, 0.5f);
    p.cubicTo (0.16f, 0.0f, 0.33f, 1.0f, 0.5f, 0.5f);
    p.cubicTo (0.66f, 0.0f, 0.83f, 1.0f, 1.0f, 0.5f);
    return p;
}

/** Microphone. */
inline juce::Path microphone()
{
    juce::Path p;
    // mic body
    p.addRoundedRectangle (0.38f, 0.0f, 0.24f, 0.5f, 0.12f);
    // stand arc
    p.addArc (0.2f, 0.35f, 0.6f, 0.5f,
              juce::MathConstants<float>::pi,
              juce::MathConstants<float>::twoPi,
              true);
    // stand leg
    p.startNewSubPath (0.5f, 0.85f);
    p.lineTo (0.5f, 1.0f);
    // stand base
    p.startNewSubPath (0.35f, 1.0f);
    p.lineTo (0.65f, 1.0f);
    return p;
}

/** Music note. */
inline juce::Path musicNote()
{
    juce::Path p;
    // note head
    p.addEllipse (0.1f, 0.6f, 0.3f, 0.25f);
    // stem
    p.startNewSubPath (0.38f, 0.65f);
    p.lineTo (0.38f, 0.1f);
    // flag
    p.startNewSubPath (0.38f, 0.1f);
    p.cubicTo (0.6f, 0.15f, 0.65f, 0.35f, 0.5f, 0.45f);
    return p;
}

/** Folder. */
inline juce::Path folder()
{
    juce::Path p;
    p.addRoundedRectangle (0.0f, 0.25f, 1.0f, 0.7f, 0.08f);
    p.addRoundedRectangle (0.0f, 0.1f, 0.45f, 0.18f, 0.06f);
    return p;
}

/** Info / circle-i. */
inline juce::Path info()
{
    juce::Path p;
    p.addEllipse (0.05f, 0.05f, 0.9f, 0.9f);
    p.addEllipse (0.42f, 0.25f, 0.16f, 0.16f);
    p.addRoundedRectangle (0.42f, 0.48f, 0.16f, 0.3f, 0.06f);
    return p;
}

/** Warning / triangle with exclamation. */
inline juce::Path warning()
{
    juce::Path p;
    p.startNewSubPath (0.5f, 0.05f);
    p.lineTo (0.98f, 0.9f);
    p.lineTo (0.02f, 0.9f);
    p.closeSubPath();
    return p;
}

/** Chevron-down (for dropdowns). */
inline juce::Path chevronDown()
{
    juce::Path p;
    p.startNewSubPath (0.15f, 0.25f);
    p.lineTo (0.5f, 0.7f);
    p.lineTo (0.85f, 0.25f);
    return p;
}

} // namespace otoha::icons
