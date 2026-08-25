# Otoha — Kaiteyo Alignment Milestones

> Make Otoha look and feel like Kaiteyo — same family, same design language, different product.
> Source of truth: `_kaiteyo-reference/` (full Kaiteyo desktop codebase)

---

## Milestone M26: Design System Component Audit & Upgrade

**Goal**: Bring Otoha's DS component library to parity with Kaiteyo's desktop design system.

### Kaiteyo Reference Files
- `desktopApp/.../designsystem/DsTokens.kt` — spacing, radius, motion, type, elevation, semantic colors
- `desktopApp/.../designsystem/DsButtons.kt` — Primary/Secondary/Ghost/Danger/AccentTint, spring animations, hover scale
- `desktopApp/.../designsystem/DsCards.kt` — hoverable card with accent top-line, DsListItem, DsVirtualList, DsEmptyState, DsSkeleton
- `desktopApp/.../designsystem/DsMisc.kt` — DsBadge, DsStatTile, DsProgressBar, DsToggle, DsSectionHeader, DsLink, DsNumberLabel
- `desktopApp/.../designsystem/DsInputs.kt` — DsTextField (bottom-border focus), DsSearchField (with clear), DsNumericField
- `desktopApp/.../designsystem/DsDialog.kt` — animated entrance (scale+fade spring), DsConfirmDialog, DsPromptDialog, DsProgressDialog
- `desktopApp/.../designsystem/DsMenu.kt` — context menus, keyboard nav (↑/↓/Enter/Esc), sections, icons, shortcuts, checkmarks
- `desktopApp/.../designsystem/DsToast.kt` — DsToastHost, auto-dismiss, spring icon pop, kind-based coloring (success/warning/error/info)
- `desktopApp/.../designsystem/DsToolbar.kt` — DsToolbar (title+subtitle+actions), DsToolbarDivider, DsSplitPane
- `desktopApp/.../designsystem/DsResponsive.kt` — width tiers (Compact/Standard/Wide/ExtraWide), adaptive dialogs, grid columns
- `desktopApp/.../designsystem/DsSelect.kt` — styled dropdown select
- `desktopApp/.../designsystem/DsTag.kt` — tag/chip component
- `desktopApp/.../designsystem/DsTextArea.kt` — multi-line text input

### Otoha Changes

#### 26.1 — Upgrade `DsButton.h` to match Kaiteyo DsButtons.kt
- [ ] Add `DsButtonKind` enum: `Primary`, `Secondary`, `Ghost`, `Danger`, `AccentTint`
- [ ] Add hover-scale spring animation (0.97 pressed → 1.02 hover → 1.0 rest)
- [ ] Add `DsTextButton` (ghost text-only with hover tint)
- [ ] Add `DsIconButton` with hover background animation
- [ ] Add `DsButtonRow` for grouped buttons
- [ ] Match color mapping: Primary=accent, Secondary=surfaceElevated, Ghost=transparent, Danger=error, AccentTint=accent@16%
- [ ] Use `DsType::body` / `DsType::label` for font sizing

#### 26.2 — Upgrade `DsSurfaces.h` to match Kaiteyo DsCards.kt
- [ ] Add hoverable card with accent top-line on hover (2dp accent bar at top)
- [ ] Add elevation animation: Flat → Raised on hover (spring animation)
- [ ] Add `DsListItem` component (leading icon + title + subtitle + trailing slot)
- [ ] Add `DsEmptyState` component (icon + title + message + action)
- [ ] Add `DsSkeleton` loading placeholder (pulsing surface color)

#### 26.3 — Add missing DS components
- [ ] Add `DsBadge` (pill with tinted background, caption font)
- [ ] Add `DsStatTile` (label + big value + optional delta, rounded card)
- [ ] Add `DsProgressBar` (fraction-based, accent-colored fill)
- [ ] Add `DsSectionHeader` (title + subtitle + optional action slot)
- [ ] Add `DsSearchField` (search icon + text field + clear button)
- [ ] Add `DsSelect` (styled dropdown)
- [ ] Add `DsToggle` (labeled toggle switch)

#### 26.4 — Upgrade `DsToast.h` to match Kaiteyo DsToast.kt
- [ ] Add kind-based coloring: Success(green bg), Warning(amber bg), Error(red bg), Info(surface bg)
- [ ] Add spring icon pop animation on appearance
- [ ] Add auto-dismiss with configurable duration (default 3500ms)
- [ ] Add slide-up entrance + fade-out exit animation

#### 26.5 — Upgrade `DsControls.h` to match Kaiteyo DsInputs.kt
- [ ] Add bottom-border focus indicator (2dp accent line on focus, 0.25alpha border otherwise)
- [ ] Add leading icon support
- [ ] Add label above field
- [ ] Style `DsSearchField` with search icon + clear button

#### 26.6 — Upgrade `DsCore.h` motion system
- [ ] Add `DsMotion::Fast` (120ms), `DsMotion::Normal` (240ms), `DsMotion::Slow` (380ms)
- [ ] Add `DsElevation::Flat` (0dp), `Raised` (2dp), `Floating` (8dp), `Overlay` (16dp)
- [ ] Ensure `Motion::effective()` collapses to 0 when reduced-motion is active

---

## Milestone M27: Floating Sidebar — Kaiteyo Pattern

**Goal**: Match Kaiteyo's exact sidebar pattern: floating island, shadow, border, accent indicator, logo, expand/collapse, overflow.

### Kaiteyo Reference Files
- `desktopApp/.../ui/workspace/WorkspaceNav.kt` — full sidebar implementation
  - `DsNavRail` — vertical rail with floating island shape
  - `DsNavItem` — selected state with accent tint (16% alpha bg), accent indicator (3dp bar, 2dp radius)
  - `DsNavModeSwitcher` — 3 mode buttons at top
  - `DsLogoMark` — brand mark at top of sidebar
  - `DsMoreViewsButton` — overflow menu for secondary destinations
  - `NavTooltipHost` — delayed tooltip for compact mode
  - `DsCompactNavBar` — compact horizontal tab bar for narrow windows
  - Shadow: `DsElevation.Floating` (8dp) with accent-colored ambient/spot light (22% alpha)
  - Border: 1dp at `surfaceBorderSubtle` (0.3 alpha), high-contrast mode 0.8 alpha
  - Radius: `DsRadius.Lg` (16dp)
  - Inner padding: `DsSpacing.Lg` (16dp) vertical
  - Nav item spacing: `DsSpacing.Comfortable` (8dp)
  - Selected bg: `accent.copy(alpha = 0.16f)`
  - Hovered bg: `surfaceInteractive.copy(alpha = 0.6f)`
  - Selected icon tint: `accent.primary`
  - Normal icon tint: `textSecondary`
  - Label font: `DsType.Body` (14sp), selected: SemiBold

### Otoha Changes

#### 27.1 — Sidebar floating island treatment
- [ ] Add shadow with accent-colored ambient/spot light (0.22 alpha) — matches Kaiteyo exactly
- [ ] Add 1dp border at 0.3 alpha (0.8 alpha for high contrast)
- [ ] Set radius to `Radius::xl` (24px) — matches Kaiteyo `DsRadius.Lg`
- [ ] Inner vertical padding: `Spacing::xl` (24px)
- [ ] Nav item spacing: `Spacing::sm` (8px)

#### 27.2 — Nav item states
- [ ] Selected: accent tinted background (accent at 16% alpha)
- [ ] Hovered: surfaceInteractive at 60% alpha
- [ ] Default: transparent
- [ ] Selected icon tint: accent
- [ ] Normal icon tint: textSecondary
- [ ] Label font: body (14px), selected: bold
- [ ] Accent indicator: 3px wide, 16px tall, 2px radius accent bar on left of selected item

#### 27.3 — Logo mark at top
- [ ] Add Otoha brand mark (headphone/waveform icon) at top of sidebar
- [ ] Below logo: "Otoha" title text (title size, bold) + "Audio Studio" subtitle (caption, accent color)
- [ ] Only show subtitle when expanded

#### 27.4 — Overflow menu for secondary items
- [ ] Add "More" button at bottom of primary nav items
- [ ] Overflow menu shows: Editor (hidden when no recording), Sound/Enhance
- [ ] Match Kaiteyo's `DsMenuPanel` pattern: keyboard nav, sections, icons

#### 27.5 — Compact mode (narrow windows)
- [ ] Below 720px width, switch to horizontal tab bar
- [ ] Tab bar: icon + label, accent indicator at top
- [ ] Same floating island treatment (rounded, elevated, bordered)

---

## Milestone M28: TopBar + Workspace Shell

**Goal**: Match Kaiteyo's workspace shell pattern: top bar with title/subtitle/search/settings, content column with toolbar divider.

### Kaiteyo Reference Files
- `desktopApp/.../ui/workspace/WorkspaceShell.kt` — `DsTopBar`, `ContentColumn`
- `desktopApp/.../designsystem/DsToolbar.kt` — `DsToolbar`, `DsToolbarDivider`
- `desktopApp/.../ui/workspace/WorkspaceShell.kt` — `Breakpoints` (720dp compact, 760dp exit)

### Otoha Changes

#### 28.1 — Top bar
- [ ] Add top bar with: current view title (title size, semi-bold) + subtitle (caption, muted)
- [ ] Right side: search/palette trigger (styled like Kaiteyo: elevated surface, search icon, "Search…" text, "Ctrl K" badge) + settings icon button
- [ ] Padding: `Spacing::xl` horizontal, `Spacing::md` vertical
- [ ] Gap between items: `Spacing::sm`

#### 28.2 — Toolbar divider
- [ ] Add 1dp divider below top bar at border color (0.4 alpha)
- [ ] Divider should be full width of content area

#### 28.3 — Content area
- [ ] Content area gets `Spacing::xl` padding on all sides
- [ ] Content fills the remainder after sidebar + top bar
- [ ] Background: `colors::background()`

#### 28.4 — Responsive breakpoints
- [ ] Below 720px: compact layout (horizontal tab bar)
- [ ] 720-1024px: standard layout
- [ ] 1024-1440px: wide layout (grids start spreading)
- [ ] 1440px+: extra-wide (content max-width or multi-column)

---

## Milestone M29: Studio Home — Kaiteyo Dashboard Pattern

**Goal**: Match Kaiteyo's Dashboard pattern: hero card, quick actions, stat tiles, recent activity.

### Kaiteyo Reference Files
- `desktopApp/.../ui/dashboard/DashboardView.kt` — ContinueHero, StudyTargetCard, QuickActions, StatTiles, RecentActivity

### Otoha Changes

#### 29.1 — Hero card (elevated)
- [ ] Elevated card at top with accent-tinted action button
- [ ] Title: "Welcome to Otoha" or "Continue where you left off"
- [ ] Subtitle: context-dependent (e.g., "3 recordings in your library")
- [ ] Primary action: "New Recording" button (Primary kind)
- [ ] Secondary actions: "Library", "Settings" (Ghost kind)

#### 29.2 — Quick actions row
- [ ] Horizontal row of compact buttons below hero
- [ ] Actions: "Record" (Primary), "Library" (Secondary), "Enhance" (Secondary), "Export" (Secondary)
- [ ] Match Kaiteyo's `DsButtonRow` pattern

#### 29.3 — Stat tiles
- [ ] Row of `DsStatTile` components showing:
  - Total recordings
  - Total duration
  - Library items
  - Last recording date
- [ ] Each tile: uppercase label (caption, muted) + big value (heading, bold) + optional delta

#### 29.4 — Recent recordings
- [ ] `DsSectionHeader` with "Recent Recordings" title + "View all" action
- [ ] List of `DsListItem` components for last 5 recordings
- [ ] Each item: waveform thumbnail + name + duration + date
- [ ] Click opens in editor

#### 29.5 — Empty state
- [ ] When no recordings: `DsEmptyState` with microphone icon + "No recordings yet" + "Record something and it will appear here"

---

## Milestone M30: Record Screen — Kaiteyo Card Pattern

**Goal**: Match Kaiteyo's clean card-based layout for the recording screen.

### Kaiteyo Reference Files
- `desktopApp/.../designsystem/DsCards.kt` — DsCard pattern
- `desktopApp/.../designsystem/DsMisc.kt` — DsStatTile, DsBadge

### Otoha Changes

#### 30.1 — Recording card
- [ ] Main recording area as a `DsCard` (elevated when recording)
- [ ] Mic selector: styled `DsSelect` with icon
- [ ] Countdown: styled `DsSelect` with "Off", "3 sec", "5 sec", "10 sec"
- [ ] Format/quality: styled selects
- [ ] Each control: label above (caption, muted) + control below

#### 30.2 — Meter + visualizer
- [ ] Level meter as `DsProgressBar` (accent-colored, meter-safe/warning/clip states)
- [ ] Waveform visualizer inside card
- [ ] Recording timer: display size, bold, accent color when recording

#### 30.3 — Transport buttons
- [ ] Record button: large, accent, with pulse animation when recording
- [ ] Stop button: danger kind when recording
- [ ] Play/preview: ghost kind
- [ ] All buttons use `DsButton` with proper kinds

#### 30.4 — Status indicators
- [ ] Recording state badge: "Recording" (red), "Ready" (green), "Paused" (amber)
- [ ] Duration badge: formatted time
- [ ] Sample rate badge: "44.1 kHz" or "48 kHz"

---

## Milestone M31: Library — Kaiteyo Grid Pattern

**Goal**: Match Kaiteyo's card grid with search, sort, filter, and selection.

### Kaiteyo Reference Files
- `desktopApp/.../designsystem/DsCards.kt` — DsCard, DsListItem, DsVirtualList
- `desktopApp/.../designsystem/DsInputs.kt` — DsSearchField

### Otoha Changes

#### 31.1 — Search bar
- [ ] `DsSearchField` at top of library
- [ ] Placeholder: "Search recordings…"
- [ ] Clear button when text present

#### 31.2 — Sort + filter chips
- [ ] Horizontal chip row below search
- [ ] Sort options: Name, Date, Duration, Size
- [ ] Filter options: All, Recent, Favorites
- [ ] Active chip: accent tinted background
- [ ] Inactive chip: surface elevated

#### 31.3 — Card grid
- [ ] Responsive grid: 1 col (compact) → 2 cols (standard) → 3 cols (wide) → 4 cols (extra-wide)
- [ ] Each card: `DsCard` with waveform thumbnail + name + duration + date
- [ ] Hover: elevation lift + accent top-line
- [ ] Click: open in editor
- [ ] Selection mode: checkbox overlay + bulk actions toolbar

#### 31.4 — Empty state
- [ ] `DsEmptyState` with library icon + "No recordings yet" + "Record something to see it here"

---

## Milestone M32: Editor + Sound — Kaiteyo Panel Pattern

**Goal**: Match Kaiteyo's panel-based layout for editor and sound views.

### Kaiteyo Reference Files
- `desktopApp/.../designsystem/DsToolbar.kt` — DsToolbar, DsSplitPane
- `desktopApp/.../designsystem/DsCards.kt` — DsCard
- `desktopApp/.../designsystem/DsMisc.kt` — DsSectionHeader

### Otoha Changes

#### 32.1 — Editor toolbar
- [ ] `DsToolbar` at top with: title ("Editor") + subtitle (filename) + actions (undo, redo, export)
- [ ] Below toolbar: `DsToolbarDivider`

#### 32.2 — Editor layout
- [ ] `DsSplitPane` (horizontal) for timeline + properties
- [ ] Timeline panel: `DsCard` with waveform + transport
- [ ] Properties panel: `DsCard` with editing controls
- [ ] Each panel has `DsSectionHeader`

#### 32.3 — Sound/Enhance panel
- [ ] `DsCard` for each DSP control group
- [ ] `DsSectionHeader` for "Enhance", "Bass", "Clarity", "Space"
- [ ] Sliders with labels (caption, muted) + value display
- [ ] Preset selector: `DsSelect`
- [ ] A/B comparison: toggle button (ghost kind)

#### 32.4 — Export panel
- [ ] `DsCard` with export settings
- [ ] Format select: `DsSelect`
- [ ] Quality select: `DsSelect`
- [ ] Export button: Primary kind
- [ ] Progress: `DsProgressBar` during export

---

## Milestone M33: Settings — Kaiteyo Category Rail Pattern

**Goal**: Match Kaiteyo's settings layout: category rail + content area.

### Kaiteyo Reference Files
- `desktopApp/.../ui/settings/SettingsView.kt` — SettingsRail, CategoryRow, SettingsContent, AboutSettingsSection

### Otoha Changes

#### 33.1 — Settings rail (left side)
- [ ] Category rail with icons + labels
- [ ] Categories: General, Recording, Library, Export, Appearance, About
- [ ] Selected: accent tinted background + accent indicator dot
- [ ] Hovered: surfaceInteractive background
- [ ] Search field at top of rail

#### 33.2 — Settings content (right side)
- [ ] Category header card: icon + title + description + Reset button
- [ ] Settings grid: 2 columns on wide, 1 on narrow
- [ ] Each setting: `DsListItem` or `DsToggle` or `DsSelect`
- [ ] Reset all settings button at bottom (ghost kind)

#### 33.3 — Appearance settings (Kaiteyo Theme Studio pattern)
- [ ] Mode selector: System / Light / Dark (chips)
- [ ] Accent palette: grid of color circles
- [ ] Live preview card showing current theme
- [ ] Density selector: Compact / Comfortable / Spacious

#### 33.4 — About section
- [ ] App identity card: logo + name + tagline + version badge
- [ ] Stats row: total recordings, total duration, library items
- [ ] Credits card
- [ ] License card

---

## Milestone M34: Dialogs + Menus + Toasts

**Goal**: Match Kaiteyo's dialog, menu, and toast patterns.

### Kaiteyo Reference Files
- `desktopApp/.../designsystem/DsDialog.kt` — animated entrance, DsConfirmDialog, DsPromptDialog, DsProgressDialog
- `desktopApp/.../designsystem/DsMenu.kt` — context menus, keyboard navigation
- `desktopApp/.../designsystem/DsToast.kt` — toast host, kind-based coloring

### Otoha Changes

#### 34.1 — Dialog entrance animation
- [ ] Scale from 0.94 → 1.0 with spring (medium bouncy)
- [ ] Fade from 0 → 1 with 180ms tween
- [ ] Reduced motion: instant (no animation)

#### 34.2 — Confirm dialog
- [ ] Title + message + Cancel (ghost) + Confirm (primary or danger) buttons
- [ ] Used for: delete recording, discard changes, bulk delete

#### 34.3 — Prompt dialog
- [ ] Title + text field + Cancel + Save buttons
- [ ] Used for: rename recording, new folder name

#### 34.4 — Progress dialog
- [ ] Title + message + progress bar + percentage
- [ ] Used for: export, bulk export

#### 34.5 — Context menus
- [ ] Right-click menus with: icon + label + shortcut label + checkmark
- [ ] Keyboard navigation: ↑/↓ to move, Enter to activate, Esc to dismiss
- [ ] Danger items: red tinted

#### 34.6 — Toast system
- [ ] Bottom-center positioning
- [ ] Kind-based coloring: Success (green), Warning (amber), Error (red), Info (surface)
- [ ] Spring icon pop on appearance
- [ ] Auto-dismiss after 3500ms
- [ ] Slide-up entrance + fade-out exit

---

## Milestone M35: Responsive Layout + Motion Polish

**Goal**: Match Kaiteyo's responsive behavior and motion system.

### Kaiteyo Reference Files
- `desktopApp/.../designsystem/DsResponsive.kt` — width tiers, adaptive dialogs, grid columns
- `desktopApp/.../ui/workspace/WorkspaceShell.kt` — breakpoints, hysteresis
- `desktopApp/.../ui/workspace/WorkspaceNav.kt` — dock animation, compact mode

### Otoha Changes

#### 35.1 — Width tiers
- [ ] Compact: < 720px → horizontal tab bar
- [ ] Standard: 720-1024px → sidebar + single column
- [ ] Wide: 1024-1440px → sidebar + multi-column grids
- [ ] Extra-wide: 1440px+ → sidebar + max-width content or 3+ columns

#### 35.2 — Dialog adaptive width
- [ ] Compact dialogs: 50% of window, min 400px, max 560px
- [ ] Rich dialogs: 60% of window, min 480px, max 860px

#### 35.3 — Grid adaptive columns
- [ ] Library grid: 1→2→3→4 columns based on width tier
- [ ] Settings grid: 1→2 columns based on width tier
- [ ] Stat tiles: 2→4→5 based on width tier

#### 35.4 — Sidebar animation
- [ ] Expand/collapse with 240ms tween (fast-out-slow-in)
- [ ] Width animates between collapsed (56px) and expanded (200px)
- [ ] Reduced motion: instant (0ms)

#### 35.5 — View transition animation
- [ ] Slide-in from right (forward) or left (backward) with 280ms tween
- [ ] Cross-fade with 280ms
- [ ] Reduced motion: instant cross-fade only

#### 35.6 — Hover animations
- [ ] Card elevation: Flat → Raised with 200ms tween
- [ ] Button scale: spring (damping 0.6, stiffness 500)
- [ ] Button background: 160ms color animation
- [ ] Nav item background: 160ms color animation

---

## Milestone M36: Final Visual Consistency Audit

**Goal**: Every screen looks like one application — Otoha — not six separate screens.

### Checklist

#### 36.1 — Typography consistency
- [ ] All screen titles: `TextSize::title` (18px bold)
- [ ] All section headers: `TextSize::heading` (22px bold) or `TextSize::bodyLarge` (16px)
- [ ] All body text: `TextSize::body` (14px)
- [ ] All captions/metadata: `TextSize::caption` (11px)
- [ ] All button labels: `TextSize::button` (14px bold)
- [ ] No hardcoded font sizes outside `OtohaTheme.h`

#### 36.2 — Spacing consistency
- [ ] All screen margins: `Spacing::xl` (24px)
- [ ] All card padding: `Spacing::lg` (16px)
- [ ] All control gaps: `Spacing::md` (12px)
- [ ] All tight gaps: `Spacing::sm` (8px)
- [ ] All minimal gaps: `Spacing::xs` (4px)
- [ ] No hardcoded spacing values outside `OtohaTheme.h`

#### 36.3 — Color consistency
- [ ] All surfaces use `colors::surfaceElevated()` for cards
- [ ] All interactive hovers use `colors::surfaceHover()`
- [ ] All borders use `colors::border()` or `colors::borderSubtle()`
- [ ] All text hierarchy uses text tokens (primary/secondary/muted/disabled)
- [ ] No `juce::Colour(0x...)` literals outside `OtohaTheme.h`

#### 36.4 — Component consistency
- [ ] All buttons use `DsButton` with proper kind
- [ ] All cards use `DsCard` with proper elevation
- [ ] All list items use `DsListItem`
- [ ] All selects use `DsSelect`
- [ ] All text fields use `DsTextField`
- [ ] All dialogs use `DsDialog`
- [ ] All toasts use `DsToast`

#### 36.5 — Layout consistency
- [ ] All screens have top bar with title + subtitle
- [ ] All screens use `DsToolbarDivider` below top bar
- [ ] All content areas use `Spacing::xl` padding
- [ ] All sidebars use floating island treatment

#### 36.6 — Interaction consistency
- [ ] All hoverable elements have hover state
- [ ] All clickable elements have press state
- [ ] All animations respect reduced motion
- [ ] All keyboard shortcuts are documented

---

## Execution Order

1. **M26** (DS Components) — foundation for everything else
2. **M27** (Sidebar) — most visible visual change
3. **M28** (TopBar + Shell) — completes the shell
4. **M29** (Studio Home) — first screen users see
5. **M30** (Record) — core functionality screen
6. **M31** (Library) — core functionality screen
7. **M32** (Editor + Sound) — existing screens get DS treatment
8. **M33** (Settings) — existing screen gets DS treatment
9. **M34** (Dialogs + Menus + Toasts) — interaction layer
10. **M35** (Responsive + Motion) — polish layer
11. **M36** (Final Audit) — consistency verification

---

## Kaiteyo Reference Map

| Kaiteyo Component | Otoha Equivalent | Milestone |
|---|---|---|
| `DsTokens.kt` | `OtohaTheme.h` | M26.6 |
| `DsButtons.kt` | `DsButton.h` | M26.1 |
| `DsCards.kt` | `DsSurfaces.h` | M26.2 |
| `DsMisc.kt` | new components | M26.3 |
| `DsInputs.kt` | `DsControls.h` | M26.5 |
| `DsDialog.kt` | `DsToast.h` (upgrade) | M34.1-34.3 |
| `DsMenu.kt` | new context menu | M34.5 |
| `DsToast.kt` | `DsToast.h` | M26.4 |
| `DsToolbar.kt` | new toolbar | M28.1-28.2 |
| `DsResponsive.kt` | new responsive | M35.1-35.3 |
| `DsSelect.kt` | new select | M26.3 |
| `DsTag.kt` | new tag/chip | M26.3 |
| `WorkspaceNav.kt` | `DsNavigation.h` | M27 |
| `WorkspaceShell.kt` | `AppShell.cpp` | M28 |
| `DashboardView.kt` | `HomeView.cpp` | M29 |
| `SettingsView.kt` | `SettingsView.cpp` | M33 |

---

## Current State Assessment

### What Otoha Already Has (from Kaiteyo alignment)
- ✅ OLED Black theme with Kaiteyo hex values
- ✅ Light theme with Kaiteyo hex values
- ✅ Accent palette (10 colors)
- ✅ Typography scale matching Kaiteyo DsType
- ✅ Spacing scale matching Kaiteyo DsSpacing
- ✅ Radius scale matching Kaiteyo DsRadius
- ✅ Motion tokens (Fast/Normal/Slow)
- ✅ Floating sidebar with rounded corners
- ✅ Nav items with selected/hovered states
- ✅ Basic design system components (DsButton, DsCard, DsControls)
- ✅ Toast system
- ✅ Empty states
- ✅ Basic settings with appearance picker

### What Otoha Needs (from Kaiteyo alignment)
- ❌ Hover animations (elevation spring, scale spring, color tween)
- ❌ Card accent top-line on hover
- ❌ DsListItem, DsBadge, DsStatTile, DsProgressBar, DsSectionHeader
- ❌ DsSearchField with clear button
- ❌ DsSelect styled dropdown
- ❌ DsConfirmDialog, DsPromptDialog, DsProgressDialog
- ❌ Context menus with keyboard navigation
- ❌ Top bar with title/subtitle/search/settings
- ❌ Toolbar divider
- ❌ Logo mark in sidebar
- ❌ Overflow menu for secondary nav items
- ❌ Compact mode (horizontal tab bar for narrow windows)
- ❌ Responsive width tiers
- ❌ Adaptive dialog widths
- ❌ Adaptive grid columns
- ❌ Sidebar expand/collapse animation
- ❌ View transition animation (slide + fade)
- ❌ Kaiteyo-style dashboard (hero card, stat tiles, recent activity)
- ❌ Kaiteyo-style settings (category rail + content area)
