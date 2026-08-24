# Changelog

All notable changes to Otoha. Format: Keep a Changelog, semver.

## [1.0.0] — unreleased

Otoha's first release: record → edit → enhance → export.

### Added
- Studio: microphone recording with countdown (off/3/5/10 s), live input
  meter with clipping warning, crash-safe lossless WAV capture.
- Library: SQLite-backed collection with search, sorting, rename,
  duplicate, delete-to-trash, drag-and-drop import by reference.
- Editor: single-timeline waveform editing — selection, cut/copy/paste,
  ripple delete ("Delete"), keep-selection, full undo/redo, zoom, spacebar
  transport, autosave + recovery sidecars.
- Enhance: one-tap DSP presets (Natural/Music/Voice/Vocal/Bass/Clarity) over
  the shared chain — noise reduction, 5-band EQ, bass/clarity lifts,
  compressor, limiter; Original/Enhanced A-B preview; NaN/Inf output guard.
- Export: WAV/FLAC natively; M4A/Opus/MP3 via user-installed FFmpeg;
  quality presets; batch queue with progress and cancellation.
- Windows Sound: system-wide playback enhancement via WASAPI loopback →
  shared render, per-device profiles, ON/OFF bypass, tray-friendly design.
- `.otoha` portable project container (versioned format).
- Cross-platform core: capability-gated UI, shared error model, logical
  paths; honest unsupported-platform messaging.

### Known limitations
- System-wide Sound is Windows-only.
- Whole-file decode on open: very long recordings (hours) are memory-heavy.
- Android/macOS/iOS builds are prepared or experimental, not distributed in v1.
- Compressed export requires a user-installed FFmpeg binary.
