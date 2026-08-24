# Otoha 1.0.0

Otoha is a simple, open-source recording and audio-enhancement app.

**Record. Edit. Enhance. Export.**

## What it does

- **Record** your microphone losslessly — countdown optional, live level
  meter, clipping warning, crash-safe writing so an interrupted take still
  gives you its audio.
- **Edit** on one clear waveform: select, cut/copy/paste, delete-with-gap-
  close, full undo/redo, autosave recovery.
- **Enhance** with one tap: noise reduction, EQ, bass/clarity, compressor,
  limiter — compare Original vs Enhanced instantly.
- **Export**: WAV/FLAC lossless; M4A/Opus/MP3 via your installed FFmpeg;
  batch export many recordings at once.
- **Windows Sound**: enhance *all* system playback in real time with
  per-device profiles (Windows only).

## Platforms in this release

| Platform | Status |
|---|---|
| Windows 10+ (x64) installer | Released (unsigned — SmartScreen may warn; see below) |
| Linux x64 tarball | Experimental |
| macOS / Android / iOS | Not part of this release |

## Installation

1. Download `Otoha-1.0.0-Windows-x64.exe` (or the Linux tarball) below.
2. Verify the SHA-256 checksum against `checksums.txt`.
3. Install, launch, press Record.

Full guide: docs/user-guide.md in the repository.

## Known limitations

- Windows builds are **not code-signed**; SmartScreen may show a warning —
  choose "More info → Run anyway" if you trust the source.
- System-wide Sound is Windows-only.
- Compressed export needs FFmpeg installed separately (never bundled).
- Very long recordings (multiple hours) are memory-heavy by design.
- Editing loads the whole recording into memory — voice-note to ~30-minute
  material is the sweet spot.

## Privacy

Your audio never leaves your device. No telemetry, no analytics, no network
connections. See docs/privacy.md.
