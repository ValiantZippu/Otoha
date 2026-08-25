# Kaiteyo → Otoha Design Reference

This document maps Kaiteyo's Compose Multiplatform design system to Otoha's JUCE implementation.

## Color System

### Base Mode: OLED Black (default)
| Token | Hex | Otoha Token |
|---|---|---|
| background | `#050505` | `background` |
| surface | `#0D0D0D` | `surface` |
| surfaceElevated | `#101010` | `surfaceElevated` |
| surfaceInteractive | `#1A1A1A` | `surfaceHover` |
| border | `#2A2A2A` | `border` |
| textPrimary | `#F0F0F0` | `textPrimary` |
| textSecondary | `#A0A0A0` | `textSecondary` |
| textMuted | `#606060` | `textMuted` |

### Accent: Signature Pineapple (default)
| Token | Hex | Otoha Token |
|---|---|---|
| primary | `#C2FC8B` | `accent` |
| primaryDark | `#9CE85E` | `accentHover` |
| secondary | `#FEAB57` | `accentSecondary` |
| secondaryDark | `#FD8A2E` | `accentSecondaryHover` |
| tertiary | `#7BC8FF` | `info` |
| onPrimary | `#050505` | `accentContrast` |

### Semantic Colors
| Token | Hex | Otoha Token |
|---|---|---|
| success | `#C2FC8B` | `success` |
| warning | `#FEAB57` | `warning` |
| error | `#FF6B6B` | `danger` |
| info | `#7BC8FF` | `info` |
| favorite | `#FFD93D` | `favorite` |

### Recording Colors (Otoha-specific)
| Token | Hex | Otoha Token |
|---|---|---|
| recording | `#FF6B6B` | `recording` |
| recordingPulse | `#FF8A65` | `recordingPulse` |
| recordingBackground | `#1A0A0A` | `recordingBackground` |

### Meter Colors
| Token | Hex | Otoha Token |
|---|---|---|
| meterSafe | `#C2FC8B` | `meterSafe` |
| meterWarning | `#FEAB57` | `meterWarning` |
| meterClip | `#FF6B6B` | `meterClip` |

### Waveform Colors
| Token | Hex | Otoha Token |
|---|---|---|
| waveform | `#C2FC8B` | `waveform` |
| waveformMuted | `#2A3A2A` | `waveformMuted` |
| selection | `#C2FC8B22` | `selection` |
| playhead | `#F0F0F0` | `playhead` |

## Typography Scale

| Name | Size | Weight | Letter Spacing |
|---|---|---|---|
| Caption | 11sp | Regular | 0.5sp |
| Label | 12sp | Medium | 0.5sp |
| Body | 14sp | Regular | 0.25sp |
| BodyLarge | 16sp | Regular | 0.5sp |
| Title | 18sp | Medium | 0.1sp |
| Heading | 22sp | Bold | 0sp |
| Display | 28sp | Regular | -0.25sp |

## Spacing Scale

| Token | Value |
|---|---|
| Xs | 4dp |
| Sm | 8dp |
| Md | 12dp |
| Lg | 16dp |
| Xl | 24dp |
| Xxl | 32dp |
| Section | 40dp |

## Corner Radius

| Token | Value |
|---|---|
| Xs | 4dp |
| Sm | 8dp |
| Md | 12dp |
| Lg | 16dp |
| Xl | 24dp |
| Full | 999dp |

## Elevation

| Token | Value |
|---|---|
| Flat | 0dp |
| Raised | 2dp |
| Floating | 8dp |
| Overlay | 16dp |

## Sidebar Design

- Width: 260dp expanded, 72dp compact
- Radius: 24dp (Xl)
- Background: surfaceElevated
- Border: 1dp, border with 0.3 alpha
- Shadow: Floating elevation with accent tint
- Brand mark: Logo + "Otoha" title + subtitle
- Active item: accent primary at 0.16 alpha background + 3dp left indicator bar
- Hover: surfaceInteractive at 0.6 alpha
- Item spacing: 8dp (Sm)
- Icon size: 18dp (Medium)
- Label: Body size, SemiBold when selected

## Navigation Items

Primary (top):
1. Studio (home icon)
2. Record (record icon)
3. Library (library icon)
4. Sound (speaker icon)

Secondary (bottom):
5. Settings (settings icon)

## Animation

- Fast: 120ms
- Normal: 240ms
- Slow: 380ms
- Reduced motion: 0ms
