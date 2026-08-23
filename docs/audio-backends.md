# Audio Backends (Otoha Sound)

## Interface

`Source/Platform/AudioBackend.h` defines the only seam between Otoha and OS
audio: device enumeration, stream negotiation (`AudioStreamConfig`), a
process stage, and an output sink. The DSP Core never sees a platform API.

## Planned implementations

| Platform | Backend | Notes |
|----------|---------|-------|
| Windows  | `WindowsAudioBackend` (WASAPI, **M8: implemented**) | User-mode shared-mode loopback capture → DSP → shared render. No driver required for the M8 prototype. A virtual-device/APO layer remains possible later as a separate platform component behind this same interface. |
| macOS    | Core Audio — *not implemented* | Capture/routing model differs fundamentally from Windows virtual devices; design separately behind the same interface. Reports "not implemented" honestly. |
| Linux    | PipeWire (preferred), ALSA fallback — *not implemented* | PipeWire graph filters are the modern path for processing system audio. Reports "not implemented" honestly. |
| Android  | Platform audio-effect/session APIs — *not implemented* | Capabilities vary per device/OEM; investigate before promising system-wide enhancement. |
| iOS      | none for system audio | System-wide processing of other apps is not possible; Studio-style mic → DSP → export only. |
| Tests    | `MockAudioBackend` | Implemented (M7); pull-driven block delivery, zero platform deps. Drives all Otoha Sound engine tests. |

## Windows architecture (M8)

```
Applications -> Windows audio session (shared)
      |
WASAPI loopback capture   (source render endpoint, mix format)
      |
Otoha SoundEngine         (wet/dry Enhance + DspChain)
      |
WASAPI shared render      (chosen output endpoint)
```

Chosen because it needs no kernel driver and no exclusive-mode steal:
reliable enough for the prototype, low-latency in shared mode, maintainable,
secure, and trivially distributable (#14 priority order). Constraints handled
explicitly in code:

* capturing and rendering the same endpoint is refused (feedback loop);
* capture/render sample-rate mismatches go through a Lagrange resampler;
* >2-channel endpoints fail with an understandable message (no silent downmix);
* default-device changes arrive via `IMMNotificationClient` and surface as a
  polled flag so notifications never touch audio or UI threads.

Driver signing / virtual-endpoint considerations for a future fully
transparent insertion path are documented in Milestone 8's report, not
implemented here.

## System-wide routing reality check

The eventual shape is:

```
Applications -> OS audio -> Otoha capture / virtual device -> Otoha DSP -> physical output
```

Every arrow except "Otoha DSP" is platform-specific and NOT implemented in
Milestone 7. Nothing in this repository pretends otherwise: there are no
virtual-device stubs masquerading as working code.

## Threading contract

* `initialize/start/stop/setActiveDevice/setOutputSink` — control thread.
* The backend invokes the process stage on its own audio thread; the stage
  must be real-time safe (i.e., a prepared `DspChain`).

## Verification without hardware

`Tests/DspCoreTests.cpp` drives `MockAudioBackend` through
generate → process → capture and asserts sample counts, finiteness, applied
gain, and stereo integrity — the same contract real backends must satisfy.
