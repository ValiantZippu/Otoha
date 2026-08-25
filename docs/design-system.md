# Otoha Design System

One rule above all others: **UI code asks "what does this colour mean?", never "what hex value is it?"**

Everything visual lives in **`Source/UI/OtohaTheme.h`**. If a value isn't there, it's either an audio/DSP constant (belongs in its DSP module) or an intentional, commented exception.

Components consume **semantic tokens** — never raw `juce::Colour` literals. This is what makes M24 (runtime appearance switching) possible without touching a single view.

---

## Architecture

```
OtohaTheme.h
├── ThemeColors        every colour, named by meaning (a plain struct — data, not constants)
├── Theme              a complete named look  { name, colors }
├── makeDefaultDarkTheme()   the built-in look: AMOLED black + sakura accent
├── current()          the active Theme (views read through this)
├── setTheme(t)        swap the whole look at runtime + broadcast
├── themeChangedBroadcaster()   ChangeListener hook so components re-apply colours
├── applyToDesktopLookAndFeel() pushes tokens onto JUCE-wide colour ids (called at startup)
└── colors:: / font() / Spacing / Radius / Metrics / Motion   the token accessors
```

**Runtime recoloring (M24 prep):** colours live in a mutable `Theme` instance, not `constexpr` values. `setTheme()` swaps it and fires `themeChangedBroadcaster()`; any component that needs to react adds a `ChangeListener`. Stock widgets recolor automatically via `applyToDesktopLookAndFeel()`.

**Persistence (M24 prep):** a base mode + accent can be loaded from `AppSettings` at startup, turned into a `Theme`, and passed to `setTheme()` — no per-component changes required.

---

## Colours

Access via `otoha::theme::colors::<name>()` — always call, never cache, so a theme swap takes effect on the next paint.

| Group | Tokens |
|---|---|
| Surfaces | `background`, `surface`, `surfaceElevated`, `surfaceHover`, `surfacePressed` |
| Lines | `border`, `borderSubtle`, `focusRing` |
| Text | `textPrimary`, `textSecondary`, `textMuted`, `textDisabled` |
| Accent | `accent`, `accentHover`, `accentPressed`, `accentSoft`, `accentContrast` |
| States | `success`, `warning`, `danger`, `info` |
| Audio | `waveform`, `waveformMuted`, `selection`, `playhead`, `meterSafe`, `meterWarning`, `meterClip` |
| Recording | `recording`, `recordingPulse`, `recordingBackground` |
| Library | `favorite` |

### Surface hierarchy

```
background        window base
└─ surface        cards, panels, rows
   └─ surfaceElevated   dialogs, inputs, popups
      └─ surfaceHover / surfacePressed   interactive feedback
```

Never invent a new shade per panel — pick the level that matches the hierarchy.

### Accent

One canonical accent (`accent`). Everything that means "the primary Otoha action" derives from it: record button, selection, focus ring, primary buttons, toggles, sliders. Do not pick "slightly different pinks" per screen.

### Semantic naming

Good: `textSecondary`, `surfaceElevated`, `accentHover`, `borderSubtle`, `meterClip`.
Bad: `niceGray`, `coolBlue`, `newRed2`, `finalBackground`.

---

## Typography

`otoha::theme::font (TextSize, bold)` — seven sizes, no more:

| Token | Size | Weight | Used for |
|---|---|---|---|
| `display` | 34 | bold | brand / hero text |
| `title` | 20 | bold | screen titles ("Otoha" strip, editor title) |
| `heading` | 16 | bold | group headers, panel titles |
| `body` | 15 | regular | primary copy, subtitles |
| `bodySmall` | 14 | regular | default control text |
| `caption` | 13 | regular | durations, metadata, secondary info |
| `button` | 14 | bold | button labels |

Known intentional exceptions (commented in code, to be tokenized in M18): RecordView timer (26 bold), SoundView status (28 bold), Onboarding hero (44 bold).

---

## Spacing

`otoha::theme::Spacing` — 4 px base unit:

`xs` 4 · `sm` 8 · `md` 12 · `lg` 16 · `xl` 24 (screen margins) · `xxl` 32

Do not create new spacing values. Audio/DSP numbers are **not** UI tokens.

## Corner radius

`otoha::theme::Radius`:

`small` 6 (chips, tags) · `medium` 8 (buttons, inputs, cards) · `large` 12 (panels, dialogs) · `pill` 999

## Component metrics

`otoha::theme::Metrics` — only recurring dimensions:

`rowHeight` 40 · `buttonHeight` 36 · `inputHeight` 30 · `titleStripHeight` 54 · `iconSize` 18 · `cardPadding` 16 · `controlGap` 12 · `touchTargetMin` 44 · `sidebarWidth` 240 (reserved for M19)

## Motion

`otoha::theme::Motion` (ms, consumed by M25): `fast` 120 · `normal` 240 · `slow` 380

---

## Shared styling helpers

| Helper | Purpose |
|---|---|
| `styleCardButton(b)` | standard secondary button: elevated surface, quiet border |
| `stylePrimaryButton(b)` | dominant action: accent-tinted background, accent label |
| `label(c, name, tip)` | mandatory accessible naming for interactive controls |

## Focus & disabled states

- **Focus**: `focusRing` — visible under keyboard navigation; every M18 interactive component must use it.
- **Disabled**: use `textDisabled` / muted surfaces, not opacity hacks — disabled controls must stay understandable.

---

## Components (M18 — `Source/UI/Components/`)

Namespace `otoha::ds`. Header-only, token-consuming, no screen-specific styling. Reference renderings live in the dev-only gallery (`Ctrl+Shift+D` → `ComponentsGallery`).

| Component | File | Purpose | Key API |
|---|---|---|---|
| `Button` | `DsButton.h` | Label button; variants primary/secondary/tertiary/danger × sizes small/medium/large | `Button (text, variant, size)` |
| `IconButton` | `DsButton.h` | Compact square button for one vector icon | `IconButton (accessibleName, juce::Path)` |
| `ComboBox` | `DsControls.h` | Themed dropdown (keyboard: arrows/Enter/Esc) | inherits `juce::ComboBox` |
| `Slider` | `DsControls.h` | Themed slider, optional value readout | `Slider (name, showValue)` |
| `Toggle` | `DsControls.h` | On/off switch; state readable without colour | inherits `juce::ToggleButton` |
| `Input` | `DsControls.h` | Text field with optional error state | `setError (bool)` |
| `Card` | `DsSurfaces.h` | Surface card; hoverable / selected / interactive states | `setSelected (bool)`, `onClick` |
| `Tag` | `DsSurfaces.h` | Compact semantic label: neutral/accent/success/warning/danger | `Tag (text, Variant)` |
| `Divider` | `DsSurfaces.h` | Subtle `borderSubtle` rule — use sparingly | — |
| `Section` | `DsSurfaces.h` | Title + optional description header | `Section (title, description?)` |
| `EmptyState` | `DsSurfaces.h` | Icon/title/description/action pattern for empty screens | `EmptyState (Setup{...})` |
| `ToastHost` | `DsToast.h` | Stacked auto-dismissing notifications: info/success/warning/error | `show (Kind, message)` |

**State model**: every interactive component supports Default/Hover/Pressed/Focused/Disabled (plus Selected/Error where relevant), shares the single M17 focus ring, and recolors instantly on runtime theme change via `ThemeWatcher`. All colours, spacing, radii, typography and metrics come from `OtohaTheme.h` tokens only. Tests: `Tests/DsComponentsTests.cpp` (ctest suite `ds_components`).

---

## Rules for contributors

1. No `juce::Colour (0x...)` literals outside `OtohaTheme.h`.
2. No opacity tricks for text hierarchy — use the text tokens.
3. No per-screen accent variants — derive from `accent`.
4. One surface hierarchy; don't invent shades.
5. Audio parameters are not UI tokens.
6. Intentional exceptions must carry an inline comment saying why.

Legacy M14 free functions (`sakura()`, `card()`, `textSoft()`, `clipRed()`, …) still exist as back-compat shims mapped onto the token system — new code must use `colors::` accessors instead.
