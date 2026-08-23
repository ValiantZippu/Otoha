# Otoha DSP Core

## Real-time safety rules (mandatory for `process()`)

`DspProcessor::process()` runs on audio threads. It must NOT:

* allocate or free memory
* take locks (POD snapshot copies of parameters are fine)
* touch the filesystem, network, database, or UI
* log per-block in release builds

All resources are created in `prepare()` on a control thread. Parameter
changes arrive via `setParameters()` (message thread) and are smoothed inside
`process()` with `SmoothedFloat`, so slider moves never click or pop.

## Processor interface

```cpp
class DspProcessor {
    void prepare(const ProcessingContext&);  // sampleRate, channels, blockSize
    void process(AudioBlock&);               // real-time safe
    void reset();
    void setParameters(const ProcessingState&); // message thread
};
```

Every processor consumes its slice of the shared `ProcessingState`; the chain
simply publishes the whole state and each processor takes what it owns.

## Implemented processors

| Processor | Responsibility |
|-----------|----------------|
| `NoiseReductionProcessor` | high-pass + downward expander for steady background noise |
| `EqProcessor`             | 5-band biquad EQ (shelf / peak / shelf), neutral at 0 dB |
| `BassProcessor`           | gain-staged low-shelf lift (max +6 dB @ 90 Hz) |
| `ClarityProcessor`        | controlled presence lift (+4.5 dB max @ 3.5 kHz) |
| `CompressorProcessor`     | linked-detector feed-forward compressor |
| `LimiterProcessor`        | final ceiling protection (-1 dBFS default), linked stereo |
| `GainProcessor`           | input/output trim with smoothing |
| `StereoWidthProcessor`    | mid/side width; mono stays mono |
| `MeterProcessor`          | read-only peak/RMS/limiter-reduction tap |

## Chain order (explicit, code-defined — never UI-driven)

Studio: **NoiseReduction → EQ → Bass → Clarity → Compressor → Limiter → Meter**
(see `Source/Dsp/DspChain.cpp`). Otoha Sound will compose the same core in its
own order; nothing else changes.

## Processing context

Processors learn sample rate / channels / block size only from
`ProcessingContext`, handed to `prepare()`. They never query the OS.

## Parameter updates & smoothing

Message-thread `setParameters()` stores plain values; `process()` reads them
and advances one-pole smoothers per sample. Audible discontinuities are
impossible without a bug in a smoother.

## Metering / telemetry

`MeterSnapshot` exposes atomics (peak, RMS, limiter gain reduction) that UI
polls at frame rate. No per-sample logging, no locks.

## Testing

`Tests/DspCoreTests.cpp` exercises the core with no other project module
linked: bypass identity, per-processor behavior, finiteness, stereo integrity,
and the mock-backend end-to-end pipeline. M5 regression coverage remains in
`Tests/DspEngineTests.cpp`.
