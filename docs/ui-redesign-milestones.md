# Otoha UI Redesign — Milestones (Kaiteyo design language)

Otoha's current UI (post-M14) works but looks bare: default JUCE buttons, a flat top
button-row for navigation, mojibake emoji icons, unstyled sliders/combos. This plan
re-skins the whole app in the **Kaiteyo design language** (github.com/ValiantZippu/Kaiteyo),
adapted for a recorder instead of a study app.

**Rules for every milestone (do not skip):**

- Otoha is a recorder, NOT a DAW. No new audio features in these milestones — UI only.
- Do not rewrite working views from scratch. Re-skin and re-layout what exists.
- Every color/spacing/radius/duration comes from `Source/UI/OtohaTheme.h` tokens.
  No hardcoded `Colour(0xff...)` literals in views after M17.
- Keep every accessibility name set via `otoha::theme::label()` intact.
- After each milestone: `cmake --build build --config Release` must succeed with zero
  errors and all 12 test suites still pass (`ctest -C Release`), since tests cover
  non-UI code that these milestones must not touch.
- One milestone per pass. Do not pull work from a later milestone forward.

---

## Design source of truth (Kaiteyo → Otoha mapping)

Kaiteyo is Compose Multiplatform; Otoha is JUCE. We port the **visual system**, not the code.
All values below are lifted from Kaiteyo's `Color.kt` / `Dimens.kt` / `DsTokens.kt`.

### Base modes (surface palettes)

| Token | OLED Black (default) | Dark Gray | Midnight | Light |
|---|---|---|---|---|
| `background` | `#050505` | `#121212` | `#0A0D1A` | `#F5F5F5` |
| `surface` | `#0D0D0D` | `#1A1A1A` | `#121622` | `#EEEEEE` |
| `surfaceElevated` | `#101010` | `#242424` | `#1A1F30` | `#E8E8E8` |
| `surfaceInteractive` | `#1A1A1A` | `#2E2E2E` | `#232940` | `#FCFCFC` |
| `border` | `#2A2A2A` | `#2A2A2A` | `#2A324A` | `#D0D0D0` |
| `textPrimary` | `#F0F0F0` | `#F0F0F0` | `#EAEAFF` | `#1A1A1A` |
| `textSecondary` | `#A0A0A0` | `#A0A0A0` | `#B8B8D0` | `#606060` |
| `textMuted` | `#606060` | `#606060` | `#808098` | `#A0A0A0` |

(Sepia / Cream / Paper modes exist in Kaiteyo but are reading modes — skip them for a
recorder unless a later milestone asks.)

### Accent schemes (apply identically on every base mode)

| Scheme | primary | primaryDark | secondary | tertiary | onPrimary |
|---|---|---|---|---|---|
| **Signature Pineapple (default)** | `#C2FC8B` | `#9CE85E` | `#FEAB57` | `#7BC8FF` | `#050505` |
| Ocean | `#00D4AA` | `#00B894` | `#00A8FF` | `#0D47A1` | `#050505` |
| Sunset | `#FF6B6B` | `#E05555` | `#FFB347` | `#FF8C69` | `#1A0A0A` |
| Lavender | `#B39DDB` | `#9575CD` | `#CE93D8` | `#80CBC4` | `#1A1A2E` |
| Monochrome | `#E0E0E0` | `#BDBDBD` | `#9E9E9E` | `#616161` | `#121212` |

The old sakura pink (`#FF9ECF`) is retired as the global accent. It may survive only as
an optional "Sakura" accent entry (`primary #FF9ECF`, `secondary #FFB7D0`) for nostalgia.

### Semantic colors (recording-adapted — this is the "modify for this purpose" part)

| Token | Value (dark modes) | Used for |
|---|---|---|
| `success` | `#C2FC8B` | healthy level meter, export done, saved |
| `warning` | `#FEAB57` | hot signal, low disk space, unsaved edits |
| `error` / `clip` | `#FF6B6B` | clipping indicator, failed export, delete |
| `info` | `#7BC8FF` | playback cursor, selection handles, links |
| `recording` | `#FF6B6B` | record button + pulsing REC dot |
| `paused` | `#FEAB57` | paused state |
| `favorite` | `#FFD93D` | favorite star in Library |

### Tokens (from Kaiteyo `DsTokens.kt` / `Dimens.kt`)

- **Spacing:** 4 / 8 / 12 / 16 / 24 / 32 / 40 px (Xs…Section). Window padding 24, panel gap 24.
- **Radius:** Xs 4, Sm 8, Md 12, Lg 16, Xl 24, Full 999 (pills, record button).
- **Type scale:** Caption 13, Label 12, Body 14, BodyLarge 16, Title 18, Heading 22, Display 28.
  (Slightly smaller than the current M14 scale — Kaiteyo is denser and quieter.)
- **Motion:** Fast 120 ms, Normal 240 ms, Slow 380 ms; fades on page switches;
  a reduced-motion setting must collapse all of these to 0.
- **Elevation:** Flat 0, Raised 2, Floating 8, Overlay 16 (JUCE: fake with borders +
  slightly lighter surfaces; do not add drop shadows everywhere).
- **Layout (desktop):** floating rounded sidebar 220–260 px wide, radius 24, floating with
  24 px window padding; content panel radius 24. This replaces the top button-row.

---

## Milestone 17 (R1): Theme foundation — Kaiteyo tokens in `OtohaTheme.h`

**Prompt:** Rewrite `Source/UI/OtohaTheme.h` as a token system with zero behavior change
elsewhere. Add:

1. A `SurfacePalette` struct (background, surface, surfaceElevated, surfaceInteractive,
   border, textPrimary, textSecondary, textMuted) with the four base-mode tables above,
   plus `enum class BaseMode { oled, dark, midnight, light }` and one global active mode
   (hardcoded `oled` for now; persistence comes in M23).
2. An `AccentScheme` struct (name, primary, primaryDark, secondary, tertiary, onPrimary)
   with the five schemes above, plus one global active accent (default Signature Pineapple).
3. Semantic tokens (success/warning/error/info/recording/paused/favorite) as functions of
   the active base mode (use the dark-mode column for oled/dark/midnight; for `light` use
   Kaiteyo's light semantic set: success `#2E7D32`, warning `#EF6C00`, error `#E53935`,
   info `#1565C0`).
4. Spacing / radius / type-scale / motion tokens as `inline constexpr` values (keep the
   existing `font()` helper but re-map it onto the new scale).
5. Keep `styleCardButton` / `stylePrimaryButton` / `label` working, but reimplement them on
   the new tokens: card buttons = `surfaceInteractive` bg + `border` outline + `textPrimary`
   text; primary buttons = accent `primary` bg + `onPrimary` text.
6. Then sweep **every** view (`HomeView`, `RecordView`, `LibraryView`, `EditorView`,
   `SoundView`, `OnboardingView`, `AppShell`, `EnhancePanel`, `ExportUi`,
   `SoundAdvancedPanel`, `AboutWindow`) replacing literal colours with token calls.
   The sakura accent, `#141414` cards, `#2a1620` cardAccent literals must be gone.

**Accept:** app builds; visually it already reads as Kaiteyo-dark (lime accent on OLED
black); no layout changes; grep for `0xff141414`, `0xffff9ecf`, `0xff2a1620` in `Source/UI`
returns nothing; tests still 12/12.

---

## Milestone 18 (R2): Otoha design-system components (JUCE LookAndFeel)

**Prompt:** Create `Source/UI/DsComponents.h/.cpp`: a small Kaiteyo-style component kit so
views stop hand-styling raw JUCE widgets:

- `DsLookAndFeel` (one `juce::LookAndFeel_V4` subclass): rounded `TextButton` (radius Sm,
  `surfaceInteractive` bg, border outline, hover = `border`-tinted bg, pressed = accent-tinted),
  primary variant (accent bg, `onPrimary` text); `ComboBox` (radius Sm, `surfaceInteractive`,
  custom rounded popup menu using `surfaceElevated`); `Slider` (linear bar: 4 px track in
  `surfaceInteractive`, filled portion in accent primary, 14 px round thumb in `textPrimary`);
  `ToggleButton` (accent when on); `TextEditor`/search field (radius Sm, `surfaceInteractive`).
- `DsCard`: rounded panel component (radius Md, `surface` bg, 1 px `border` outline,
  hover = `surfaceElevated`) used for list/grid items.
- `DsTag`: small pill label (radius Full, accent-tinted bg, Label-size text) for
  format/duration/state chips.
- `DsToast`: transient bottom-center rounded toast (radius Md, `surfaceElevated`, semantic
  colour left edge) with a 240 ms fade — replaces nothing existing yet, just add it and use
  it in one place (e.g. "Saved" after recording) to prove it.
- Apply `DsLookAndFeel` from `AppShell` to the whole window.

**Accept:** no view hand-calls `setColour` on buttons/combos/sliders anymore except through
the kit; all controls share one visual family; builds clean; tests 12/12.

---

## Milestone 19 (R3): App shell — floating sidebar navigation

**Prompt:** Replace the top button-row in `AppShell` with Kaiteyo's desktop layout:
a floating rounded sidebar + rounded content panel, with 24 px window padding all around.

- Sidebar: 230 px, radius Xl (24), `surface` bg, `border` outline. Items: Studio (home),
  Library, Record, Sound, Camera, Settings — top-to-bottom, each a full-width rounded row
  (radius Sm, 40 px tall). Active item: `surfaceInteractive` bg + accent-coloured 3 px left
  indicator bar + `textPrimary` text; inactive: `textSecondary`. Icons: draw simple vector
  paths in `paint()` (mic, waveform, dot-record, sliders, camera, gear) tinted with the
  item's text colour — **no emoji, no unicode symbols** (that's what caused the mojibake
  "āœā€ Record" in the titlebar row).
- Otoha wordmark ("otoha", Display size, accent-coloured dot) pinned at the sidebar top;
  app version (from `BuildInfo.h`) in Caption/`textMuted` at the bottom.
- Content area: the active view sits in a rounded panel (radius Xl, `background` bg) with
  24 px gap from the sidebar. Page switches use a 240 ms crossfade (respect reduced motion).
- Keep the existing view-switching API (`showHome/showLibrary/...`) — only the chrome
  changes. Camera stays a disabled placeholder item.
- Window: keep the current default size but enforce a sensible minimum (900×600).

**Accept:** no top button row; sidebar matches the description; mojibake impossible
(no non-ASCII glyphs drawn); keyboard focus order covers sidebar items; tests 12/12.

---

## Milestone 20 (R4): Studio (home) screen

**Prompt:** Redesign `HomeView` in the Kaiteyo dashboard style:

- Greeting header: "Good morning/afternoon/evening" in `textSecondary` Body, big
  "otoha" or time-appropriate title in Display, all left-aligned inside the content panel —
  not centered like today.
- One prominent primary button: "Record" (accent bg, `onPrimary` text, radius Sm, with the
  vector mic icon from M19) that jumps to the Record screen.
- "Recent" section: horizontal row (or 2-column grid) of `DsCard`s, each showing title
  (Body, `textPrimary`), duration (Caption, `textMuted`), a `DsTag` with the format
  (WAV/FLAC/…), and a thin accent-tinted waveform thumbnail if cheaply available from the
  existing waveform cache — if not trivial, use a neutral placeholder bar, don't build new
  waveform plumbing.
- Empty state (no recordings): centered `textMuted` copy + "Record your first take"
  primary button. No "View library" duplicate — the sidebar already navigates.
- Clicking a recent card opens it in the editor (existing `openInEditor` path).

**Accept:** matches Kaiteyo dashboard feel (left-aligned, card-based, quiet); all colours
from tokens; tests 12/12.

---

## Milestone 21 (R5): Record screen

**Prompt:** Redesign `RecordView` for the Kaiteyo language while keeping every existing
behaviour (device pickers, quality, countdown, monitor, level meter, clipping, pause/resume,
crash-safe writer):

- Top row: "Record" Heading + device/quality/countdown controls as `DsLookAndFeel`
  combos/toggles in one tidy row (Label-size captions above each control).
- Center: large level visualization area on `surface` bg — input level meter as a rounded
  horizontal bar (radius Full track; fill gradients `success` → `warning` at −12 dB →
  `error` past 0 dBFS) plus the existing live waveform in `textSecondary`, selection/cursor
  accents in `info`.
- Record button: 72 px circle, radius Full, `recording` red bg with white dot while idle;
  while recording it becomes a rounded square (pause glyph) and a small pulsing REC dot +
  elapsed time (Caption, `recording` colour) sit above the meter. Stop = `surfaceInteractive`
  rounded button beside it. Countdown shows as a huge Display number centered in the
  visualization area, fading per second.
- Status line at the bottom: Caption text in `textMuted` (device/sample-rate info), errors
  in `error` colour. No red-on-red walls of text like today.
- Fix the current bug visible in screenshots: when no microphone exists, the pickers must
  be disabled (not blank) and the empty-state message centered in the visualization area.

**Accept:** all previous record behaviours still work; meter/clip colours come from
semantic tokens; countdown visually obvious; builds; tests 12/12.

---

## Milestone 22 (R6): Library screen

**Prompt:** Redesign `LibraryView` as a Kaiteyo-style card grid keeping all existing
functionality (search, sort, rename, favorite, trash-delete, batch export, open-in-editor):

- Header row: "Library" Heading left; search `TextEditor` (Ds styling, magnifier drawn as
  vector) and sort `ComboBox` right, all on one baseline.
- Items as `DsCard`s in a responsive grid (2–4 columns by width): title Body, duration
  Caption `textMuted`, format `DsTag`, favorite star (vector, `favorite` yellow when on),
  created-date Caption. Hover raises card to `surfaceElevated`.
- Selection for batch operations: accent-tinted border + corner checkmark (vector) on
  selected cards; a bottom action bar (rounded, `surfaceElevated`) appears with
  Export / Delete counts when selection is non-empty.
- Context actions (rename, delete, export) via a right-click rounded popup menu
  (`surfaceElevated`, radius Sm) — restyle whatever menu mechanism exists; do not add a new
  menu framework.

**Accept:** all library operations still pass their tests; grid reflows on resize;
tokens only; tests 12/12.

---

## Milestone 23 (R7): Editor + Sound (enhance) screens

**Prompt:** Re-skin `EditorView`, `EnhancePanel`, `ExportUi`, `SoundAdvancedPanel`:

- Editor: transport buttons as Ds buttons (play/pause/stop with vector glyphs), timeline
  ruler + clip blocks on `surface` bg, waveform in `textSecondary`, selection in `info`,
  playhead as a 2 px `textPrimary` line with a small accent handle. Undo/redo/cut/copy/
  paste/delete keep behaviour, get consistent icon+tooltip treatment.
- Enhance panel: section headers in Label/`textSecondary`; sliders per Ds spec; the A/B
  enable toggle becomes a proper Ds toggle labelled "Enhance" with the accent colour when
  active; preset `ComboBox` styled by Ds.
- Export dialog: rounded panel (radius Lg, `surfaceElevated`), format `DsTag`-style
  segmented choice, progress bar in accent primary, cancel as neutral button; success/error
  toasts via `DsToast`.
- Sound screen (`SoundView`): same treatment — sliders, preset combo, output meter in
  semantic colours; kill the "Starting" placeholder weirdness and the duplicated "Output"
  rows visible in the current screenshot.

**Accept:** edit/enhance/export behaviours unchanged (edit-engine + export tests still
green); visual family matches the rest; tests 12/12.

---

## Milestone 24 (R8): Settings — Appearance (base mode + accent picker)

**Prompt:** Build the Settings screen (currently a placeholder) Kaiteyo-Theme-Studio style:

- Sections: Appearance, Audio, Storage, About.
- Appearance: base-mode chooser (OLED Black / Dark Gray / Midnight / Light as `DsCard`
  swatches showing each palette's background+surface), accent chooser (5 accent rows, each
  a pill of its primary+secondary colours, radio-style selection), plus a "Reduce motion"
  toggle. Persist via the existing `AppSettings` (add keys `appearance.baseMode`,
  `appearance.accent`, `appearance.reducedMotion`); apply live — the whole window recolours
  with a 380 ms crossfade (Kaiteyo's theme-transition behaviour), instant if reduced motion.
- Audio: surface the existing device settings (sample rate, bit depth defaults) read-only
  pointers into the Record screen defaults; no new audio code.
- Storage: recordings folder path + "Open folder" button; disk space Caption line.
- About: version/commit from `BuildInfo.h`, AGPL notice, link text to the repo.

**Accept:** switching base mode/accent recolours everything (proves M17's token system is
complete — any leftover hardcoded colour will visibly break here); choice survives restart;
tests 12/12.

---

## Milestone 25 (R9): Polish, motion & accessibility pass

**Prompt:** Final coherence pass over all screens:

- Motion audit: every state change uses DsMotion durations; hover transitions 120 ms;
  record-button pulse subtle (opacity, not size); reduced-motion collapses all.
- Focus/keyboard: every interactive element shows a 2 px accent focus ring (radius of its
  shape); tab order logical on every screen; all accessibility names from
  `otoha::theme::label()` verified present.
- Contrast check: `textSecondary`/`textMuted` legible on all four base modes; meter
  gradient stops distinguishable for colour-blind users (add a clip peak-tick mark, not
  just colour).
- Remove dead UI code (old sakura helpers, unused colour constants, the top-nav remnants).
- Update `docs/user-guide.md` screenshots section text and README milestone log entry.

**Accept:** manual smoke pass of Record → Library → Edit → Enhance → Export on OLED and
Light modes; zero hardcoded colours left (`grep -rn "Colour (0x" Source/UI` only hits
OtohaTheme.h); tests 12/12.

---

## Suggested order & sizing

| Milestone | Theme | Rough size | Depends on |
|---|---|---|---|
| M17 | Token foundation | medium | — |
| M18 | Ds component kit | medium | M17 |
| M19 | Sidebar shell | medium | M17 (M18 helps) |
| M20 | Studio home | small | M18, M19 |
| M21 | Record screen | medium | M18, M19 |
| M22 | Library | medium | M18, M19 |
| M23 | Editor + Sound | large | M18, M19 |
| M24 | Settings/Appearance | medium | M17–M19 (proves tokens) |
| M25 | Polish/a11y | small | all |

Each milestone is a single agent prompt: paste the milestone section **plus** the
"Design source of truth" section and the "Rules" block at the top. Nothing else needed.
