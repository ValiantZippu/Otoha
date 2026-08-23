# Audio Backends (Otoha Sound)

## Interface

`Source/Platform/AudioBackend.h` defines the only seam between Otoha and OS
audio: device enumeration, stream negotiation (`AudioStreamConfig`), a
process stage, and an output sink. The DSP Core never sees a platform API.

## Planned implementations

| Platform | Backend | Notes |
|----------|---------|-------|
| Windows  | WASAPI  | For system-wide capture a virtual-audio-device/routing layer is required. Otoha will implement its own independent solution; no third-party driver code is copied. |
| macOS    | Core Audio | Capture/routing model differs fundamentally from Windows virtual devices; design separately behind the same interface. |
| Linux    | PipeWire (preferred), ALSA (low-level fallback) | PipeWire graph filters are the modern path for processing system audio. |
| Android  | Platform audio-effect/session APIs | Capabilities vary per device/OEM; investigate before promising system-wide enhancement. |
| iOS      | none for system audio | System-wide processing of other apps is not possible; Studio-style mic → DSP → export only. |
| Tests    | `MockAudioBackend` | Implemented now; pull-driven block delivery, zero platform deps. |

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
