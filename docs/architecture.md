# Otoha Architecture

## Products

```
        OTOHA
          │
   ┌──────┴──────┐
Otoha Studio  Otoha Sound (future)
Recording /    System playback
Editing /      enhancement
Export
   │              │
   └──────┬───────┘
    Otoha DSP Core     ← Source/Dsp/Core (portable, UI-free)
          │
    Audio Engine / Platform adapters
```

## Module map (this repository)

| Directory            | Contents | Depends on |
|----------------------|----------|------------|
| `Source/Dsp/Core`    | Otoha DSP Core: processor interface, context, block model, all processors, metering | juce_core, juce_audio_basics only |
| `Source/Dsp`         | Studio facade (`DspChain`), `ProcessingState`, presets, preview source, resampler | Dsp/Core |
| `Source/Audio`       | Recorder, Player (device I/O for Studio) | JUCE audio devices, Dsp |
| `Source/Editor`      | AudioDocument (timeline), TimelineSource, renderer | Dsp |
| `Source/Library`     | SQLite metadata, LibraryService, waveform cache | sqlite3 |
| `Source/Export`      | ExportManager, naming, FFmpeg adapter | Editor, Dsp |
| `Source/UI`          | JUCE components; builds requests, never implements DSP | everything above |
| `Source/Platform`    | `AudioBackend` interface, MockAudioBackend, DeviceProfiles — the seam where OS audio APIs will live | Dsp/Core only |

Dependency rule: **nothing below `Source/UI` may depend on anything above it**, and
`Source/Dsp/Core` depends on no other project module.

## Rendering pipeline (Studio)

```
File -> AudioDocument (timeline clips) -> DspChain (DSP Core) -> PCM Renderer -> Encoder (JUCE WAV/FLAC or FFmpeg) -> Output
```

The same `DspChain` instance type serves real-time preview (`DspPreviewSource`)
and offline export (`AudioExporter`); there is exactly one DSP implementation.

## Live-audio pipeline (future Otoha Sound)

```
OS audio capture -> AudioBackend -> DspChain -> AudioOutputSink
```

Proven today by `MockAudioBackend` in `Tests/DspCoreTests.cpp`. Real backends
(WASAPI / Core Audio / PipeWire) implement the same interface later.

## Platform separation

Platform-specific code is confined to `Source/Platform/<os>` adapters behind
the `AudioBackend` and encoder abstractions. The DSP Core never includes a
platform header.
