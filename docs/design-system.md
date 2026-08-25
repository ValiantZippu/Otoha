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

`rowHeight` 40 · `buttonHeight` 36 · `inputHeight` 30 · `titleStripHeight` 54 · `iconSize` 18 · `cardPadding` 16 · `controlGap` 12 · `touchTargetMin` 44 · `sidebarWidth` 200 · `sidebarCollapsed` 56 · `sidebarPadding` 8

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
| `NavItem` | `DsNavigation.h` | Sidebar nav entry: icon+label, active/hover states | `NavItem (label, icon, name)` |
| `Sidebar` | `DsNavigation.h` | Floating sidebar panel with brand + nav items | `addItem (id, label, icon)`, `setActiveItem (id)` |

**Vector icons** (`OtohaIcons.h`): 25 unit-square `juce::Path` icons (home, record, library, sound, settings, play, pause, stop, undo, redo, back, forward, search, more, plus, close, trash, check, waveform, microphone, musicNote, folder, info, warning, chevronDown). Scale via `getTransformToScaleToFit()`. No raster assets, no font glyphs.

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

---

## Navigation shell (M19 — `DsNavigation.h`, `OtohaIcons.h`)

The application shell uses a **floating sidebar** (`otoha::ds::Sidebar`) for all navigation.  The sidebar sits above the content background using `surface`/`border`/`borderSubtle` tokens, includes a brand mark, and navigates between the five primary destinations (Studio, Record, Library, Sound, Settings).

**Sidebar layout**: brand area top, primary nav items (icon + label), secondary items (Settings) bottom.  Below a width threshold, labels collapse to icon-only mode.

**Vector icon registry** (`otoha::icons`): 25 hand-drawn `juce::Path` icons.  All icons are unit-square paths that scale cleanly via `getTransformToScaleToFit()`.  No raster assets, no text-glyph fonts, no mojibake.

**Keyboard navigation**: arrow keys cycle between sidebar items, Enter/Space activate.  Number keys 1-5 provide direct page access.  Focus ring uses the shared M17 focus treatment.

**Theme integration**: sidebar recolors instantly via `ThemeWatcher`.  Accent colour drives the active indicator bar and active label colour.  All sidebar dimensions consume `Metrics::sidebarWidth`, `sidebarCollapsed`, `sidebarPadding`.

---

## Studio home (M20 — `HomeView.h/.cpp`)

The Studio landing screen answers three questions immediately: What can I do? What did I work on recently? Where do I go next?  Built from the M18 component kit, all visuals consume `OtohaTheme` tokens.

**Layout** (vertical, max content width 720px):
1. **Header** — time-of-day greeting + tagline.
2. **Primary action** — prominent `ds::Card` with accent tint and accent border. Routes to the existing Record screen.
3. **Recent** — up to 5 newest recordings from `LibraryService::query(newestFirst)`.  Each is a compact `ds::Card` showing name, duration, and friendly relative date.  Click → Editor.  Empty state uses `ds::EmptyState` with a Record action button.
4. **Quick actions** — two `ds::Card`s: Library (browse all recordings) and Sound (microphone/audio settings).

**Data source**: `LibraryService` — real library metadata only.  No fake recordings, no analytics.

**Empty state** (`ds::EmptyState`): icon (microphone), title, description, primary Record button.

**Keyboard navigation**: digit shortcuts (1-5) via the existing AppShell infrastructure map to sidebar destinations; Record is digit 3.

**Component reuse**: `ds::Card` (with `setProminent` for hero), `ds::EmptyState`, `ds::Button`.  `Card::setProminent(true)` tints toward `accentSoft` background and `accent` border — a generic DS variant, not screen-specific styling.

---

## Record screen (M21 — `RecordView.h/.cpp`)

The polished recording experience: choose mic → countdown → record → stop → save.  All visuals consume M17/M18 tokens.

**Layout** (vertical, max content width 720px):
1. **Config row** — `ds::ComboBox` mic selector, `ds::ComboBox` countdown (Off/3/5/10 sec), `ds::Toggle` monitor.
2. **Timer** — large centred readout during countdown/recording/playback.
3. **Waveform** — live `AudioThumbnail` visualizer with playhead.
4. **Level meter** — RMS + peak bar using `meterSafe`/`meterClip` tokens; clip indicator.
5. **Record button** — 44px circle using `recording`/`recordingPulse` tokens; toggles to Stop.
6. **Actions** — `ds::Button` (secondary) Play, Edit, Export, Stop, Delete (danger).
7. **Status/Error** — format label, status message, error message.

**State machine** (driven by `Recorder::TransportState`): Idle → Countdown → Recording → Paused → Idle.  Countdown uses a monotonic clock; no file is created until it finishes.

**Recording tokens**: `recording`, `recordingPulse`, `recordingBackground` from M17 — never hardcoded red.

**Permission handling**: Android mic permission requested on first record attempt only.  Denied state shows explanation + recovery suggestion.

**Device disconnect**: Recorder reports `FailureReason::deviceLost`; RecordView auto-stops, preserves the file if possible, refreshes the device list.

**Audio safety**: countdown and meter run on UI thread only.  Audio callback meters/monitors through the existing lock-free FIFO; no UI objects touched on the audio thread.

---

## Library (M22 — `LibraryView.h/.cpp`)

The recording library uses a responsive card-grid layout with M18 DS components.  All existing business logic (search, sort, multi-select, bulk actions, context menu, drag-drop import, rename, delete, export pipeline) is preserved while visuals consume theme tokens.

**Layout** (vertical, max content width 900px):
1. **Header** — title + recording count.
2. **Toolbar** — `ds::Input` search, `ds::ComboBox` sort (Newest/Oldest/Name/Duration), filter chips (`ds::Button` small: All/Audio/Favorites).
3. **Bulk bar** — selection count + Export/Delete (`ds::Button` secondary/danger).
4. **Card grid** — virtualised `juce::ListBox` with `RowComponent` cards: waveform thumbnail (from `WaveformCache`), name, duration, friendly date.  Selection tinted `accentSoft`, hover `surfaceHover`, focus ring via `drawFocusRing`.
5. **Details panel** — compact metadata for single selection.
6. **Empty states** — `ds::EmptyState` with icon + title + description + Record action (empty library) or search icon + "No recordings found" (search miss).

**Data source**: `LibraryService::query(searchText, filter, sort)` — real library metadata.  No fake recordings.

**Search**: `ds::Input` with placeholder.  Case-insensitive display-name match via existing SQLite query.  Updates results immediately on text change.

**Sort**: `ds::ComboBox` with 6 modes.  Default: Newest first.

**Filter**: Three `ds::Button` chips (All/Audio/Favorites).  Active chip disabled to indicate current filter.  Removed unused Video filter.

**Waveform preview**: Peak data from `WaveformCache`.  Background generation; rows repaint as jobs finish.  Fallback: muted music note icon.

**Selection + bulk actions**: Multi-select via Cmd/Shift click.  Bulk Export (connected to existing export pipeline) and Delete (with confirmation).  Cmd+A selects all.

**Context menu**: Play, Open in Editor, Rename, Duplicate, Favorite, Export, Show in Folder, Delete.  All existing actions preserved.

**Card states**: Default (surface), hover (surfaceHover), selected (accentSoft + accent border), focused (M17 focus ring).  Playing recording shows accent play icon overlay on waveform.

---

## Editor + Sound (M23 — `EditorView.h/.cpp`, `SoundView.h/.cpp`, `EnhancePanel.h/.cpp`)

The existing editor and sound/DSP controls are re-skinned with M18 design-system tokens while preserving all audio functionality unchanged.

**Editor** (`EditorView`):
- **Header** — Back (card-styled), recording name (title token), menu button.
- **Waveform** — background via `colors::background()`.  All existing selection/playback/edit logic unchanged.
- **Toolbar** — edit buttons restyled with `styleCardButton` (cut/copy/paste/delete/undo/redo/zoom), save button with `stylePrimaryButton`, enhance with `stylePrimaryButton`.  All onClick handlers, keyboard shortcuts (Ctrl+Z/X/C/V/Space/Delete), and edit operations unchanged.
- **Sound** — EnhancePanel overlay unchanged, restyled with theme tokens.
- **Feedback** — transient toast via `feedbackLabel` with accent color.

**Sound** (`SoundView`):
- **Status** — title-font readout, theme background.
- **Power toggle** — accessible name + description added.
- **Sliders** — Bass/Clarity/Space/Enhance using existing `juce::Slider` styled through theme colours.
- **Advanced button** — `styleCardButton` with label + tooltip.
- **Meter** — existing peak meter with `meterSafe`/`meterClip` tokens.

**EnhancePanel** (`EnhancePanel`):
- **Controls** — EQ (Bass/Mid/Treble), Compression, Noise Reduction, Limiter — all existing DSP parameters unchanged.
- **Preset** — built-in + custom presets via `ComboBox`.
- **A/B switch** — Original/Enhanced toggle for comparison.
- **Advanced section** — expanded EQ, compressor detail, limiter ceiling, NR strength.
- All visuals consume `OtohaTheme` tokens.  No new DSP added.