# Otoha

**A simple place to record, enhance, edit, and keep your audio and video.**

Otoha is a free, open-source, cross-platform recording app for desktop and mobile.
It is deliberately **not** a DAW, not Audacity, and not a multitrack editor. The whole
product is one loop:

> **Record something → make it sound good → make small edits → organize it → export it.**

---

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
