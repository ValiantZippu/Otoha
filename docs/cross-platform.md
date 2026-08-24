# Cross-Platform Architecture (Milestone 12)

Otoha separates **shared core** from **platform layers**. Studio does not know
which operating system it is running on.

```
                 OTOHA
                   │
        ┌──────────┴──────────┐
        │                     │
   Otoha Core            Platform Layer
   ┌────┴─────────┐      ┌─────────┼─────────┐
   │              │      │         │         │
 Studio        Otoha   Windows   Android   macOS/Linux/iOS
 (record/edit/ DSP     WASAPI    JUCE AAudio   shared Studio
  enhance/     Core    Sound     mic input     foundation builds;
  export)              backend   + Share       no faked backends
```

## Shared modules (platform-independent)

| Module | Location | Notes |
|---|---|---|
| Timeline / undo / selection | `Source/Editor/AudioDocument.*` | clip-list model, non-destructive |
| Project format | `Source/Editor/ProjectFormat.*` | `.otoha` container, versioned |
| Recording transport + phases | `Source/Audio/RecordingState.h`, `RecorderPhase.h` | one enum + transition tables |
| DSP chain | `Source/Dsp/*` | identical algorithms on every platform (#33) |
| Renderer / export | `Source/Export/*` | WAV/FLAC native; M4A/Opus/MP3 via external FFmpeg where available |
| Library model | `Source/Library/*` | SQLite metadata only — never decoded audio (#10) |
| Waveform core | `Source/Library/WaveformCache.*` | multiresolution peaks, regenerable (#42/#43) |
| Presets / settings | `Source/Dsp/Presets.*`, `Source/Core/AppSettings.h` | portable JSON |
| Error model | `Source/Core/OtohaError.h` | categories → user text (#64/#65) |
| Capabilities | `Source/Core/PlatformCapabilities.h` | UI shows only what is real (#50/#51) |
| Logical paths | `Source/Core/AppPaths.h` | appData / recordings / cache / exports (#9/#11) |
| Logging | `Source/Core/OtohaLog.h` | leveled; never logs audio or secrets (#66) |

No file above includes a Windows header. Windows-only code stays confined to
`Source/Sound/platform/WindowsAudioBackend.*`.

## Capability matrix

| Capability | Windows | macOS | Linux | Android | iOS |
|---|---|---|---|---|---|
| MicrophoneRecording | ✅ | ✅ | ✅ | ✅ | ✅ |
| BatchExport | ✅ | ✅ | ✅ | ✅ | ✅ |
| SystemWideOutputProcessing | ✅ (M8 backend) | ❌ honest | ❌ honest | ❌ impossible on stock Android | ❌ impossible by policy |
| SystemTray / StartupWithOS | ✅ | planned | planned | ❌ n/a | ❌ n/a |
| BackgroundRecording | ✅ desktop semantics | desktop semantics | desktop semantics | **not claimed** until foreground-service flow is hardware-tested (#21) | ❌ OS suspends apps |

Nothing is declared `true` speculatively. A future capability becomes true only
when its implementation exists and its tests have run.

## Android recording architecture

```
Android microphone
   ↓
JUCE android audio device layer (AAudio primary, OpenSL ES fallback)
   ↓
audioDeviceIOCallback (float buffers, device-native sample rate)
   ↓
otoha::RecorderPhase lifecycle        ← shared, headless-tested
   ↓
crash-safe background writer (same as Windows: streamed WAV + flush())
   ↓
lossless source → timeline → DSP chain → render → export
```

Key decisions:

* **API**: JUCE's Android support wraps AAudio/OpenSL ES behind the existing
  audio-callback shape the Windows recorder already consumes — the recorder and
  state machine are literally the same code.
* **Permissions (#17/#18)**: microphone permission is requested only at the
  moment the user presses Record, never at launch. Denial shows
  "Microphone access is required to record." with Try Again / Open Settings —
  mapped through `ErrorCategory::permissionDenied`.
* **Interruptions (#23/#24)**: phone calls, focus loss, device changes land in
  a defined `RecorderPhase` (paused or error +
  `ErrorCategory::audioInterrupted`). The writer's periodic flush means an
  interrupted take is a valid shorter file, never corrupt audio.
* **Background (#21/#22)**: recording continues under a foreground service with
  a visible notification, per Android policy — invisible recording is not a
  goal and not implemented. Until that flow is hardware-tested,
  `backgroundRecording` stays false in the capability table.
* **Storage/share/import (#25–#27)**: app-private storage (no broad filesystem
  permission); export reaches the user through the system share sheet;
  import uses the system document picker. Both are capability-gated.

The mobile editor reuses the shared timeline; it exposes play/cut/delete/
undo/enhance/export first and defers everything else to "More" (#28–#32).

## Project format (`.otoha`)

A project is a directory, documented in `Source/Editor/ProjectFormat.h`:

```
My Recording.otoha/
    project.json     formatVersion, title, timestamps, document payload
    audio/           optional imported copies (reference-by-default policy, #63/#64)
    waveform/        regenerable peak cache (#43)
```

* `formatVersion: 1` today; `migrateToCurrent()` is the single migration seam
  so future bumps are non-destructive (#8).
* Raw audio is never embedded in JSON (#6). Sources are referenced; missing
  sources surface as `fileUnavailable`, not crashes (#59/#66).
* Newer-format files are refused with an explanation instead of mis-parsed.

## DSP portability & determinism (#33/#34)

One implementation, compiled everywhere. Floating-point results may differ
across compilers/CPUs within normal tolerance; byte-identical output across
platforms is explicitly NOT required or claimed. `Tests/CrossPlatformTests.cpp`
verifies the *models*; signal-level determinism checks live in the existing DSP
suites and should be run per target build.

## Testing

* `otoha_xplat_tests` (`Tests/CrossPlatformTests.cpp`) — recorder phase table,
  capability declarations, error translation, `.otoha` save/load roundtrip,
  newer-format refusal, corrupt-file handling. Runs anywhere CMake runs.
* All pre-existing suites (state machine, edit engine, DSP, sound engine,
  export, library, release hardening) remain the shared-core regression gate.
* Android hardware tests (permission flow, interruption matrix, process death,
  share/import, thermal behavior #53–#58) require a real device — they are
  specified here and reported NOT TESTED until executed.
