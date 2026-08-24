# Otoha Architecture

Read this before changing anything structural. Otoha is **not** a DAW: one
loop — record → edit → enhance → export.

## Dependency direction (enforced by convention, reviewed in audits)

```
        UI  (Source/UI, Source/App views)
         │   may use ↓ only
    Application layer  (AppShell, AppState, AppLifecycle, services)
         │
   ┌─────┼──────────┬─────────────┐
   ▼     ▼          ▼             ▼
Project  DSP      Export       Library      (Source/Editor, Source/Dsp,
                                          Source/Export, Source/Library)
   └─────┴──────────┴─────────────┘
                  ▼
                Core  (Source/Core, Source/Audio models)
                  │
        Platform interfaces  (Source/Platform/AudioBackend.h,
                              Source/Core/PlatformCapabilities.h)
                  │
     ┌────────────┼────────────────┐
     ▼            ▼                ▼
  Windows      Android        macOS/Linux/iOS
(Sound/WASAPI) (Studio input) (Studio foundation; honest stubs)
```

**Rules**

* Platform APIs never appear above the dashed line. Windows code lives ONLY in
  `Source/Sound/platform/` (+ the WIN32-gated CMake section). M13 audit found
  zero leaks outside those files.
* UI never touches raw audio buffers directly; it goes through the document /
  service APIs (`AudioDocument`, `LibraryService`, `ExportManager`, `DspChain`).
* The audio callback path is: backend audio thread → prepared `DspChain`
  (`process()` only). See docs/dsp.md real-time rules.

## Where things happen

```
Microphone / file
      ↓ decode            Source/Library + JUCE AudioFormatManager
Timeline (clip list)       Source/Editor/AudioDocument.*   (non-destructive)
      ↓
DSP chain                  Source/Dsp/DspChain  (fixed order:
                           NR→EQ→Bass→Clarity→Comp→Limiter→Meter;
                           NaN/Inf guarded at output)
      ↓
Renderer                   Source/Editor/TimelineRenderer
Encoder                    Source/Export (WAV/FLAC native; FFmpeg subprocess
                           for M4A/Opus/MP3 when installed)
```

## Thread ownership (M13 #8)

| Thread | Allowed | Never |
|---|---|---|
| **UI** | widget updates, `AppState` reads, message-thread service calls | blocking I/O, encoder waits |
| **Audio** (device callbacks) | `DspChain::process`, preallocated buffers, lock-free atomics | allocation, filesystem, logging, locks, UI |
| **Render worker** | offline render/export, waveform generation | UI calls (report via events instead) |
| **File worker** | recording writer flushes, autosave sidecars | audio-thread work |

Cross-thread communication: atomics (meters, invalid-sample counter), the
recorder state machine, and `EventBroadcaster` (message thread).

## Key seams

* `Source/Core/OtohaError.h` — every failure crosses layers as a category.
* `Source/Core/PlatformCapabilities.h` — UI shows only declared capabilities.
* `Source/Core/AppPaths.h` — logical locations, mapped per platform.
* `Source/Editor/ProjectFormat.*` — versioned `.otoha` container.
* `docs/dsp.md`, `docs/audio-backends.md`, `docs/cross-platform.md`,
  `docs/project-format` details in `Source/Editor/ProjectFormat.h`.

## Deliberate limitations (v1)

* AudioDocument decodes its source fully into memory — fine for voice-note
  lengths, documented as the scaling boundary for very long recordings.
* One timeline, mono/stereo. No multitrack/MIDI/plugins by design.
