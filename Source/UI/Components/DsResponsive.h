#pragma once

/*    DsResponsive — responsive layout utilities (M35).

    Provides:
      - Width-tier queries (isCompact, isMedium, isExpanded)
      - Responsive column count calculator
      - Responsive padding/spacing calculator
      - Reduced-motion-aware animation helpers
      - Touch-target sizing helpers

    All functions are pure/inline — no component inheritance needed.
    Screens call these in their resized() to adapt layout.
*/

#include "DsCore.h"

namespace otoha::ds::responsive
{

// ---------------------------------------------------------------------------
// Width-tier queries
// ---------------------------------------------------------------------------

/** Compact: phone, small desktop window, narrow workspace. */
inline bool isCompact (int width) { return width < theme::Breakpoints::compact; }

/** Medium: tablet, small/medium desktop. */
inline bool isMedium (int width) { return width >= theme::Breakpoints::compact && width < theme::Breakpoints::wide; }

/** Expanded: normal desktop, large desktop. */
inline bool isExpanded (int width) { return width >= theme::Breakpoints::wide; }

// ---------------------------------------------------------------------------
// Responsive column count
// ---------------------------------------------------------------------------

/** Returns the number of grid columns for the given width and minimum card width. */
inline int columns (int availableWidth, int minCardWidth, int maxColumns = 4)
{
    if (availableWidth <= 0 || minCardWidth <= 0) return 1;
    return juce::jlimit (1, maxColumns, availableWidth / minCardWidth);
}

/** Convenience: responsive columns using default breakpoints. */
inline int gridColumns (int availableWidth)
{
    if (availableWidth < 400)  return 1;
    if (availableWidth < 720)  return 2;
    if (availableWidth < 1024) return 3;
    return 4;
}

// ---------------------------------------------------------------------------
// Responsive padding
// ---------------------------------------------------------------------------

/** Responsive horizontal padding that adapts to available width. */
inline int horizontalPadding (int availableWidth)
{
    if (availableWidth < 400)  return theme::Spacing::md;
    if (availableWidth < 720)  return theme::Spacing::lg;
    return theme::Spacing::xl;
}

// ---------------------------------------------------------------------------
// Sidebar helpers
// ---------------------------------------------------------------------------

/** Sidebar width: collapsed icon-only on compact, full on medium/expanded. */
inline int sidebarWidth (int shellWidth)
{
    return isCompact (shellWidth) ? 0 : theme::Metrics::sidebarWidth;
}

/** Whether the sidebar labels should be visible. */
inline bool sidebarLabelsVisible (int shellWidth)
{
    return ! isCompact (shellWidth);
}

// ---------------------------------------------------------------------------
// Animation helpers (reduced-motion aware)
// ---------------------------------------------------------------------------

/** Effective animation duration (0 when reduced motion is preferred). */
inline int animDuration (int ms) { return theme::Motion::effective (ms); }

/** Smooth value interpolation: lerps from current to target at the given fraction. */
inline float lerp (float current, float target, float fraction)
{
    return current + (target - current) * fraction;
}

/** Quick-check: should we animate this transition? */
inline bool shouldAnimate() { return ! theme::Motion::prefersReducedMotion(); }

// ---------------------------------------------------------------------------
// Touch helpers
// ---------------------------------------------------------------------------

/** Minimum interactive size for touch devices (uses DS touch target token). */
inline int touchTarget() { return theme::Metrics::touchTargetMin; }

/** Whether the current width suggests a touch-primary device. */
inline bool isTouchPrimary (int width) { return isCompact (width); }

// ---------------------------------------------------------------------------
// Dialog sizing
// ---------------------------------------------------------------------------

/** Responsive dialog width: full-ish on compact, bounded on expanded. */
inline int dialogWidth (int availableWidth)
{
    if (availableWidth < 400)  return juce::jmax (280, availableWidth - 48);
    if (availableWidth < 720)  return juce::jmin (480, availableWidth - 64);
    return juce::jmin (560, availableWidth - 80);
}

/** Responsive dialog max-height fraction of parent. */
inline float dialogMaxHeightFraction() { return 0.85f; }

// ---------------------------------------------------------------------------
// Settings category rail
// ---------------------------------------------------------------------------

/** Settings category rail width: full rail on expanded, compact selector on narrow. */
inline int settingsRailWidth (int availableWidth)
{
    if (availableWidth < 520)  return 0;     // horizontal tab mode
    if (availableWidth < 800)  return 140;   // narrow rail
    return 180;                              // full rail
}

} // namespace otoha::ds::responsive
