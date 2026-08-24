# Otoha

**Simple, open-source recording and audio enhancement.**

> **Record. Edit. Enhance. Export.**

Otoha is a free, open-source audio app for desktop and mobile. It is
deliberately **not** a DAW — it's a focused modern utility that happens to
have serious audio processing underneath.

## Features

* **Recording** — countdown, live input meter, clipping warning, crash-safe lossless capture
* **Library** — search, sort, rename, duplicate, drag-and-drop import
* **Waveform editing** — select, cut/copy/paste, ripple delete, undo/redo, zoom, autosave recovery
* **Enhance** — one-tap presets: noise reduction, EQ, bass, clarity, compressor, limiter, with A-B compare
* **Export** — WAV/FLAC lossless; M4A/Opus/MP3 via your installed FFmpeg; batch export
* **Windows Sound** — enhance *all* system playback in real time, with per-device profiles
* **Portable projects** (`.otoha`) that move between machines

## Platforms

| Platform | Status | Notes |
|---|---|---|
| Windows 10+ (x64) | Stable | Installer + Sound. Unsigned builds may trigger SmartScreen. |
| Linux (x64) | Experimental | Tarball + desktop entry |
| Android | In preparation | Core ready; build not distributed yet |
| macOS | Experimental | Builds from source; no bundle yet |
| iOS | Not distributed | Core is portable |

## Installation

Grab `Otoha-<version>-Windows-x64.exe` from
[Releases](../../releases), install, launch, press Record. Full guide:
[docs/user-guide.md](docs/user-guide.md). Verify downloads with the published
SHA-256 checksums.

## Building & contributing

From source: [BUILDING.md](BUILDING.md) · Architecture: [ARCHITECTURE.md](ARCHITECTURE.md) ·
Contributing: [CONTRIBUTING.md](CONTRIBUTING.md)

## License

AGPL-3.0-or-later — see [LICENSE.md](LICENSE.md) and
docs/licensing.md for the dependency audit. Your recordings stay on your
device, always: [docs/privacy.md](docs/privacy.md).

---

## Development milestone log

> Next up: **UI redesign (M17–M25)** — re-skinning the app in the Kaiteyo design
> language. Plan and per-milestone agent prompts:
> [docs/ui-redesign-milestones.md](docs/ui-redesign-milestones.md).

## Status — Milestone 16: packaging & release engineering (code complete)

- Build metadata (`BuildInfo.h`: version/commit/date/type) baked in via CMake, shown on About (#3/#4).
- Release pipeline: tag-triggered GitHub Actions building Linux tarball + Windows installer with test gates and optional Authenticode signing via secrets; predictable artifact names `Otoha-<ver>-<Platform>-x64` (#39/#40).
- App icon (single-shape SVG), Linux `.desktop` + packaging script (#24/#27/#28).
- Community/legal set: LICENSE.md (AGPLv3 decision + vendoring checklist), CHANGELOG.md, SECURITY.md, CONTRIBUTING.md, docs/privacy.md, docs/user-guide.md, docs/release-matrix.md, docs/android-release.md, issue templates.

## Status — Milestone 15: QA, stress testing & reliability (code complete)

M15 tries to break Otoha before users can. Static + headless pass; hardware rows honestly open (see docs/qa-report.md).

- **New `otoha_qa_tests` suite**: DSP across 7 rates × 11 buffer sizes × mono/stereo; quiet-input amplification; impulse explosion; +6 dB sine gain measurement; extreme parameter stack; compressor reset isolation; seeded property-based timeline fuzzing with per-op invariant checks; 1000-op undo-all/redo-all; undo branching; unicode project paths; damaged-payload refusals.
- **Security fix (#72)**: FFmpeg export now refuses destination paths containing quote characters instead of interpolating them into the command string.
- **Privacy audit (#70)**: zero network calls in Source/; JUCE networking compiled out.
- **Triage table** with severities and fix status in docs/qa-report.md; no known P0s.

---

## Status — Milestone 14: UI/UX polish & visual identity (code complete)

M14 makes the existing feature set feel like one finished product. No audio-architecture changes.

- **Design system** (`Source/UI/OtohaTheme.h`) — the AMOLED + sakura palette, six-step typography scale, spacing constants, button styling helpers and an accessibility-label helper, all in one named place; HomeView is refactored as the reference implementation (#1/#63/#72).
- **Dominant Record button** — explicit hover/pressed colours, `● Record` label, accessible name + tooltip (#5).
- **Plain-language editing** — "Ripple Delete" is now just **Delete**; "Trim" is **Keep Selection**; Enhance reads **✨ Enhance** (#27/#73).
- **Human feedback after edits (#28)** — transient "Deleted 12 s / Cut … / Pasted …" toast over the waveform, auto-clearing (~2 s), no command names.
- **Selection-aware toolbar verified** — cut/copy/delete/keep disable without a selection; paste disabled with an empty clipboard (#26/#65).
- **Accessibility pass on icon controls** — Back/More-options/Zoom/Delete/Keep-selection/Play all carry semantic names + tooltips; record button reachable by name (#56).
- **Pass 2** — time-of-day greeting on Home (#4); ✨ Enhanced toggle wording + reset on close (#29); removed the dead "Enhance placeholder" button from RecordView — enhancing lives one Edit tap away, per the less-UI rule (#78); every remaining raw colour literal in UI code now flows through `OtohaTheme` incl. new `clipRed()`/`textSoft()` tokens (#72).

---

## Status — Milestone 13: Cross-platform completion & hardening (code complete)

M13 finishes the shared architecture: audits, edge-case hardening, CI, and developer docs. Pure code this pass; platform hardware items are reported honestly.

- **Architecture & leakage audit** — zero Windows/platform APIs outside `Source/Sound/platform/`; dependency direction documented in **ARCHITECTURE.md** with the thread-ownership table (#3/#4/#8).
- **DSP NaN/Inf guard (#12)** — `DspChain` sanitizes chain output in real time and counts invalid samples (`invalidSampleCount()`); tested headlessly.
- **Timeline edge cases (#14/#15)** — delete-at-start/end/all, zero-length ops, oversized selections, paste at begin/end, undo/redo invalidation: all specified in tests; empty-timeline state remains unreachable by design.
- **Centralized `AppState` (#6) + typed `EventBroadcaster` (#7)** — single observation point for UI; audio engine never calls widgets.
- **CI (#57/#58)** — GitHub Actions runs every headless core suite on Linux as a blocking gate (`.github/workflows/ci.yml`).
- **BUILDING.md (#56)** — honest per-platform build matrix; untested platforms marked NOT TESTED, never assumed.

---

## Status — Milestone 12: Cross-platform core & Android architecture (code complete)

Milestone 12 formalizes the split between the shared core and platform layers, and lays down the honest Android/mobile architecture (pure code this pass; see docs/cross-platform.md):

- **Shared core audit** — timeline, DSP chain, renderer/export, library, waveform, presets and settings confirmed free of any Windows includes; Windows code stays confined to `Source/Sound/platform/`.
- **`.otoha` project container** (`Source/Editor/ProjectFormat.*`) — directory-based (`project.json` + `audio/` + `waveform/`), `formatVersion` envelope with a migration seam, atomic writes; newer formats refuse gracefully.
- **Capability system** (`Source/Core/PlatformCapabilities.h`) — one honest table per platform (Windows Sound/tray/startup = true; Android system-wide = impossible, never faked; background recording not claimed until hardware-tested).
- **Shared error model** (`Source/Core/OtohaError.h`) — 8 categories translated to jargon-free desktop/mobile wording (#64/#65).
- **Recorder lifecycle** (`Source/Audio/RecorderPhase.h`) — Preparing→Countdown→Recording→…→Complete/Error state machine shared by all platforms, alongside the untouched audio-transport truth.
- **Logical paths** (`Source/Core/AppPaths.h`) and **structured logging** (`Source/Core/OtohaLog.h`, release-stripped debug, no audio/secret logging).
- **Android recording design documented** — JUCE AAudio/OpenSL input → shared recorder → crash-safe lossless writer; record-time-only permission flow, foreground-service background policy, share sheet/document picker for export/import.
- Fixed a real pre-existing compile bug: duplicate `sourceFileForTest()` declaration in `AudioDocument.h`.
- New `otoha_xplat_tests` suite (phases, capabilities, errors, project roundtrip); see Tests/CrossPlatformTests.cpp.

---

## Status — Milestone 11: Studio v1 cohesion (complete)

Milestones 1–8 already delivered the Studio mechanics; Milestone 11 closed the remaining cohesion gaps without rewriting working systems:

- **Studio Home** (`HomeView`): the new landing screen — big Record button, five most-recent recordings with durations, View Library (#2/#3)
- **Navigation**: `Studio | Sound` top-level; Studio contains Home → Library → Record → Editor → Export
- **Duplicate recording**: `LibraryService::duplicateMedia()` + context-menu action — independent file copy ("Name copy.ext"), own database row, original untouched (#20)
- **Drag-and-drop import**: drop audio files anywhere in the Library; they are *referenced where they live* — never converted or moved on import (#63/#64); unsupported formats get "Otoha can't open this audio format" (#65); startup scan tolerates externally-moved sources (#66)
- **Ctrl/Cmd+S** save shortcut added to the editor's existing #68 shortcut set

Everything else in the v1 loop — mic selection, input meter + clipping indicator, countdown (Off/3/5/10), crash-safe background writer, lossless source format, library search/sort/rename/trash-delete/favorites/batch export, non-destructive timeline with cut/copy/paste/ripple delete, snapshot undo/redo, multiresolution waveform cache, autosave sidecar + recovery, one-tap Enhance with A/B preview, WAV/FLAC/M4A/Opus/MP3 export via the single DSP-aware pipeline — was verified present from earlier milestones and left alone.

## Status — Milestone 10: Windows release hardening, installer, updates & audio UX (code complete)

Milestone 10 turns the validated M8 implementation into a shippable release candidate (pure code this pass; see docs/release.md for the hardware/installer checklist that still needs a real Windows machine):

- **Centralized version** `1.0.0` in CMake, propagated everywhere via `OTOHA_VERSION` (#34)
- **AppSettings store** (`Source/Core/AppSettings.h`): versioned `configVersion`, step-wise migration, atomic saves, corrupt/future-file resilience, and two distinct reset policies — audio-only vs full (#29/#42/#46/#47)
- **AppLifecycle state machine** (`Source/App/AppLifecycle.h`): Starting → Ready → ON/OFF ⇄ Recovering/Unavailable → Stopping replaces scattered booleans; the UI literally cannot claim ON while inactive (#18/#23)
- **First-launch onboarding** (`OnboardingView`): one screen — Get Started → Output (System Default) → Enhance ON → Natural preset (#3–#5)
- **Custom presets** (`Source/Dsp/UserPresets.h`): Save/rename/duplicate/delete with automatic name disambiguation and JSON persistence; built-ins stay immutable (#13)
- **Advanced menu** on the Sound screen: EQ/compressor-amount/limiter panel with a confirmation before disabling protection (#7–#10), diagnostics export (#44), About (#35), audio reset
- **About window + third-party notices**: reads `THIRD-PARTY-NOTICES.txt` beside the exe, with an accurate built-in fallback (#36/#37); opt-in update check behind a pluggable source — disabled by default, never forced (#39–#41)
- **Safe Mode**: `Otoha.exe --safe-mode` disables custom presets/auto-switching for recovery (#45)
- **Installer & release tooling**: Inno Setup script (`packaging/windows/Otoha.iss`), `scripts/release.sh` (build → test gate → package → SHA-256 checksums), `docs/release.md` reproducibility + RC checklist (#27/#54–#57)
- **Fixed a real build bug found during the audit**: `EditorView.cpp` and `SoundView.cpp` were missing from the app's `target_sources`
- New headless `release_hardening` test suite: signal-artifact matrix (#50: silence/impulse/sine/sweep/pink × five DSP states vs NaN/clipping/DC/channel integrity), settings migration/resets, preset CRUD persistence, exhaustive lifecycle transition table, semver compare, diagnostics sanity

## Status — Milestone 8: Otoha Sound real-time system audio (complete)

Milestone 7 extracted the DSP Core; Milestone 8 points it at **live system audio**, Windows-first:

- **Otoha Sound mode** in the app shell (Studio | Record | **Sound**) — a deliberately separate experience from recording/editing (#40)
- **SoundEngine**: the SAME `DspChain` processors as Studio (no duplicate DSP, #20), wrapped with a smoothed wet/dry mixer so **Enhance** is one 0–100% control that can never destabilize the chain (#5)
- **Windows backend** (`WindowsAudioBackend`): user-mode WASAPI shared-mode loopback capture → DSP Core → shared render to the chosen output endpoint. No kernel driver, no FxSound dependency; a virtual-device/APO architecture remains isolated behind the same `AudioBackend` interface for later (#11-#14)
- **Feedback-loop guard**: capturing and rendering the same endpoint is refused with an understandable error (#41)
- Format negotiation from endpoint mix formats (32-bit float shared mode), proper Lagrange resampling when rates differ (#15/#16), >2-channel endpoints refused with a clear message rather than silently downmixed (#17)
- Preallocated buffers only in the audio path; ON/OFF is a flag flip — never a device restart (#4/#19)
- Output meter (smoothed peak), honest latency estimate from negotiated device periods or "unavailable" (#27), underrun counters (#30)
- Device profiles persisted to `<Otoha>/Sound/profiles.json` (atomic write), resolved device-bound → Default (#33-#36); default-output change detection via WASAPI notifications
- Sound presets (**Bass**, **Clarity**) added to the *shared* preset table — one preset engine across Studio and Sound (#6); Enhance panel now iterates the real list
- Non-Windows platforms get an honest "not implemented" backend — no fake functionality (#52)
- New headless `sound_engine` test suite: mock-backend live pipeline (1 kHz → Bass/EQ/Limiter → sink), bypass passthrough identity, Enhance monotonicity, limiter cleanliness, diagnostics counters, profile persistence round-trip, preset-table integrity

## Status — Milestone 7: Otoha DSP Core & Sound architecture (complete)

Milestone 7 is an architecture milestone: the DSP engine is now a reusable,
platform-independent core that will power both Studio and the future Otoha Sound.

- **Otoha DSP Core** (`Source/Dsp/Core`): processor interface, `ProcessingContext`,
  block-based `AudioBlock`, and every processor behind one `DspProcessor` contract —
  depends only on juce_core + juce_audio_basics (no UI, Library, Editor, FFmpeg,
  filesystem, or platform audio)
- **Processors**: NoiseReduction, EQ, Bass, Clarity, Compressor, Limiter, Gain,
  StereoWidth, Meter tap; real-time-safe `process()`, control-thread `prepare()`,
  smoothed parameters so slider moves never click
- **Studio facade** (`DspChain`): composes the core in one explicit code-defined order
  (NoiseReduction → EQ → Bass → Clarity → Compressor → Limiter → Meter); preview and
  export still share it unchanged
- **New parameters** in `ProcessingState`: bassAmount, clarityAmount, stereoWidth,
  input/output gain — neutral defaults, backwards-compatible sidecar serialization
- **Platform layer** (`Source/Platform`): `AudioBackend` interface for live audio,
  `MockAudioBackend` proving the pipeline end to end without drivers or hardware,
  and `ProfileManager` device profiles (device-bound → default → none resolution;
  automatic switching deliberately not activated yet)
- **Docs**: `docs/architecture.md`, `dsp.md`, `audio-backends.md`, `profiles.md`,
  `licensing.md` (incl. FFmpeg distribution considerations and explicit no-copy
  statements re FxSound / ViPER4Android)
- **Tests**: new headless `dsp_core` suite runs the core with no other project module
  linked — bypass identity, M5 regressions, new processors, mono/stereo integrity,
  mock-backend end-to-end pipeline, profile resolution
- No virtual audio drivers, system capture, or OS routing implemented — architecture only

## Status — Milestone 6: FFmpeg export & batch export (complete)

Milestone 5 built the pipeline; Milestone 6 gives it real-world outputs:

- **Export service** (`ExportManager`): UI-free queue, one job at a time on a background worker, per-job status/progress/cancel/retry
- **Formats**: WAV + FLAC natively (lossless, never routed through FFmpeg); M4A/AAC, Opus, MP3 via an external FFmpeg binary after Otoha's own DSP
- **FFmpeg strategy**: bundled-next-to-exe → user-configured → validated PATH; version-checked (4.x–7.x); unavailable = compressed export gracefully off, lossless always works
- **Centralized presets**: Lossless WAV/FLAC, M4A Small/Standard/High, Opus S/Std/High, MP3 S/Std/High (`ExportPresets.cpp` only)
- **Single & batch flows** from editor/Library: options dialog → folder picker → queued background jobs with overall progress, current file, cancel and summary; failure isolation with per-job retry
- **Per-recording state**: each batch item uses its own sidecar timeline+DSP unless explicitly overridden; naming/collision policies (Keep Both / Replace / Skip) with sanitized Unicode-safe names
- **Crash safety**: unique temp files, verify-then-move into place, temps cleaned on success/failure/cancellation; sources never touched

## Status — Milestone 5: DSP & Enhance (complete)

Milestone 4 made editing safe; Milestone 5 makes recordings sound better:

- **One DSP chain, two consumers** — real-time preview and offline rendering share the exact same processors and `ProcessingState` (no preview/export duplication)
- Explicit order, never UI-driven: **Noise Reduction → EQ → Compressor → Limiter**
- **EQ**: 5-band (shelves + 3 peaking biquads), neutral by default — enabling DSP never colors audio by itself
- **Compressor**: linked peak detector, hard-knee gain computer with smoothed gain; defaults bypassed or gentle
- **Limiter**: final stage, instant attack / exponential release, conservative −1 dBFS ceiling; silence stays silent
- **Noise reduction**: 85 Hz high-pass + downward expander tuned per mode (Off/Gentle/Strong); modular for future upgrades
- **One-tap Enhance** plus centralized presets: Natural, Voice, Vocal, Music, Acoustic, Live, Podcast (`Presets.cpp` only — tune without touching the engine)
- **A/B Original/Enhanced** flips a bypass flag live — no reloads, no re-renders; smoothing avoids clicks
- Processing state persists in the edit sidecar; preset changes show `*` when modified; Reset returns to neutral/bypassed
- Export renders through the same chain into WAV or FLAC with progress + cancellation; original recording always untouched
- Headless `dsp_engine` tests: bypass identity, neutral EQ unity, compressor reduction/no-runaway, limiter ceiling & silence, NR stability/monotonic strength, determinism, stereo integrity, all presets at 44.1/48 kHz

## Status — Milestone 4: lightweight editor (complete)

Milestone 3 gave Otoha a memory; Milestone 4 gives it a small pair of scissors:

- **Single-track editor** opened from the Library ("Open in Editor") or Record's Edit button
- **Non-destructive timeline**: edits are a clip list over the decoded source — no new WAV per operation
- **Sample-accurate** cut, copy, internal clipboard (with linear-resample + mono/stereo adaptation on paste), ripple delete, trim to selection
- **Snapshot undo/redo** (Ctrl/Cmd+Z / Ctrl/Cmd+Shift+Z) — history entries are clip lists, not audio
- Drag selection with Shift+Arrow nudging; click-to-seek playback cursor; wheel/button zoom with fit-all
- Play whole take or selection only (auto-stop at selection end)
- **Save** renders an `(edited)` copy into the Library as its own item; **Export** renders anywhere — original always untouched, verified temp-file rendering
- **Autosave**: edit state persists as a tiny JSON sidecar after every edit and is restored on reopen; title shows `*` for unsaved changes; close prompts Keep Editing / Save / Discard
- Delete-safety: the Library refuses to trash a recording whose document is open in the editor
- Waveform draws from a per-edit-state peak cache — repaints never decode audio

## Status — Milestone 3: media library (complete)

Milestone 2 made recording solid; Milestone 3 makes Otoha a real application:

- **SQLite-backed local library** (`Database/library.sqlite`) with schema versioning and a migration hook
- **Library screen**: search, filters (All / Audio / Video / Favorites), six sort orders, multi-select (Cmd/Ctrl + Shift), details panel
- **Automatic registration** of finished recordings; startup scan recovers unregistered files and drops stale rows — a failed database write never loses a recording
- **Cached waveform summaries** (`Cache/Waveforms/`, 256-bucket peak files) generated on one background worker, reused while the source file is unchanged
- Rename (display name only — physical filenames are never touched), favorites, delete via OS trash with waveform-cache cleanup, bulk favorite/export/delete, context menu, Show in Folder
- Desktop navigation: **Library | Record | Camera | Settings** (Camera/Settings disabled placeholders for later milestones)
- Keyboard: Ctrl/Cmd+F search · Space play/pause · Delete · Ctrl/Cmd+A select all
- Empty states that feel intentional: "No recordings yet." / "No videos yet."

## Status — Milestone 2: a real recording experience (complete)

Milestone 1 laid the vertical slice; Milestone 2 makes it feel like a real recorder:

- Engine-owned recording state machine (`idle → recording ⇄ paused → idle`) with validated transitions
- Real microphone/output device selection from live system enumeration; selectors lock during a take
- Sample-rate negotiation: prefers 48 kHz, always falls back to the closest supported rate
- Audio-driven level meter with peak hold and latching clip indicator (resets per take)
- Countdown (Off / 3 / 5 / 10 s) on a monotonic clock — cancelling creates no file
- Pause/resume keeps one continuous recording; duration comes from the sample counter, not UI frames
- Live waveform via peak-aggregated `AudioThumbnail`; click-to-seek during playback
- Post-recording actions: Play, Export (WAV copy), Delete (with confirmation + trash); Edit/Enhance are labelled placeholders for later milestones
- Graceful failure handling: no microphone, permission denied (Android flow), disk full, mid-take device disconnect
- Headless test suites: WAV structure/round-trip, state-machine rules, naming/duration helpers

## Technology

| Layer      | Choice                                            |
|------------|---------------------------------------------------|
| Language   | C++20                                             |
| App/UI/audio | [JUCE 8](https://juce.com) (fetched automatically by CMake) |
| Build      | CMake ≥ 3.22 + system SQLite dev package (`libsqlite3-dev` on Debian/Ubuntu; bundled on macOS) |
| Export     | FFmpeg binary discovered/bundled externally (4.x–7.x); optional — lossless export works without it |

## Building

### Prerequisites

**Linux (Debian/Ubuntu):**

```sh
sudo apt install build-essential cmake pkg-config libsqlite3-dev \
    libasound2-dev libx11-dev libxext-dev libxinerama-dev libxrender-dev \
    libxrandr-dev libxcursor-dev libfreetype6-dev libfontconfig1-dev \
    libgl1-mesa-dev mesa-common-dev
```

**macOS:** Xcode command line tools (`xcode-select --install`) plus CMake (`brew install cmake`).

**Windows:** Visual Studio 2022 with the "Desktop development with C++" workload.

### Configure, build, test, run

JUCE 8.0.8 is downloaded automatically on first configure (FetchContent), so a fresh
clone is all you need:

```sh
git clone https://github.com/YOUR_USER/otoha.git
cd otoha
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Run the tests (headless, no audio device or display required):

```sh
cmake --build build --target otoha_tests
./build/otoha_tests/otoha_tests_artefacts/otoha_tests
```

Run the app:

```sh
./build/Otoha/Otoha_artefacts/Release/Otoha        # Linux
# macOS: build/Otoha/Otoha_artefacts/Release/Otoha.app
# Windows: build\Otoha\Otoha_artefacts\Release\Otoha.exe
```

### Where recordings go

Recordings are plain WAV files in `Music/Otoha/` (falling back to `~/Otoha` when the
music folder isn't defined). Nothing is hidden in a proprietary container — the media
is always yours.

## Architecture (milestone 1)

```
AudioDeviceManager (shared, one open device)
        │
        ├── Recorder ── audio callback: lock-free FIFO only
        │                    └→ message thread: ThreadedWriter → WAV file
        │                                 └→ AudioThumbnail → live waveform
        └── Player ──── AudioTransportSource ← AudioFormatReaderSource (buffered)
```

Real-time rules respected from day one: the audio callback never allocates, does no
file I/O, and touches no database. Disk writing, waveform peaks, and metering all
happen off the audio thread.

```
Source/
├── App/          entry point + main window
├── Audio/        Recorder (capture + state machine), Player (playback)
├── Core/         device-independent helpers (naming, duration math)
├── Dsp/          ProcessingState, Presets, DspChain (shared preview+export)
├── Editor/       AudioDocument (clip timeline + undo), TimelineSource, Renderer
├── Export/       ExportManager (queue), AudioExporter, FfmpegSupport, presets, naming
├── Library/      Database (SQLite), LibraryService, WaveformCache, model
└── UI/           AppShell (nav), LibraryView, RecordView
Tests/            headless test suites (WAV, state machine, support, library)
```

On-disk layout:

```
<Music>/Otoha/
├── Library/Audio   Library/Video
├── Cache/Waveforms Cache/Thumbnails   (video thumbnails reserved)
└── Database/library.sqlite
```

## Roadmap

- [x] **Phase 1 — Foundation:** window → mic → level → countdown → record → WAV → playback
- [x] **Phase 2 — Recording UX:** engine-owned state machine, pause/resume, seek, export/delete, error handling, permissions hook
- [x] **Phase 3 — Library:** SQLite metadata, library UI, search/sort/rename/favorites, background waveforms, scan recovery
- [x] **Phase 4 — Editor:** non-destructive clip timeline, cut/copy/paste/ripple/trim, snapshot undo, save/export render
- [x] **Phase 5 — DSP:** shared chain (NR→EQ→Comp→Limiter), Enhance presets, A/B preview, processed export
- [x] **Phase 6 — Export:** WAV/FLAC/M4A/Opus/MP3 via shared renderer + external FFmpeg, batch queue, progress/cancel/retry
- [ ] **Phase 3 — Library:** SQLite metadata, library UI, search/sort/rename/favorites
- [ ] **Phase 4 — Editor:** selection, cut/copy/paste, ripple delete, trim, undo/redo (non-destructive edit model)
- [ ] **Phase 5 — DSP:** EQ, compressor, limiter, noise reduction, de-esser, one-tap Enhance with A/B
- [ ] **Phase 6 — Export:** WAV/FLAC/M4A/Opus/MP3 via FFmpeg, presets, bulk export
- [ ] **Phase 7 — Video:** camera record/preview/trim/export
- [ ] **Phase 8 — Mobile:** Android and iOS/iPadOS ports

## License

Otoha is licensed under the **GNU GPLv3** (see `LICENSE`). JUCE 8 is used under its
GPLv3/commercial dual license; FFmpeg and SQLite will be introduced in later milestones
with their licenses documented before release. All processing is Otoha's own independent
implementation — no proprietary codecs, algorithms, or assets.

## Privacy

Otoha is local-first: no accounts, no telemetry, no cloud. The microphone is only
accessed while the app window is open and recording is possible.
