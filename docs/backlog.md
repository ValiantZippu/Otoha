# Post-v1 Backlog

Deliberately excluded from v1. Prioritize by **frequency × severity × user
impact ÷ cost** once real usage data exists — not by coolness (#39).

## Labels (GitHub)

`bug` `crash` `audio` `dsp` `export` `recording` `windows` `android` `macos`
`linux` `performance` `ux` `documentation` `security` `good-first-issue`

## UX
- Recording-complete screen ("Recording saved" → Play / Edit) polish (M14 leftover)
- Theme adoption across Library/Sound/Record internals (OtohaTheme pass 2)
- Visual regression reference screenshots per screen
- Mobile-adapted navigation (top nav + bottom actions) once Android ships

## DSP / Audio
- Streaming/large-file decode path for hour-length recordings (remove the
  whole-file-in-memory boundary)
- Per-platform DSP determinism harness with defined tolerances
- Loudness-normalized Enhance preset evaluation

## Recording
- Android foreground-service background recording + hardware matrix run
- Device hot-swap mid-take behavior tuning on real hardware

## Editing / Library
- Non-sequential multi-selection batch operations
- Library "copy into Otoha" import policy option (today: reference-only)
- Missing-source relink flow UI

## Export / Packaging
- Bundled FFmpeg decision (LGPL build analysis required before bundling)
- macOS .app bundle + Developer ID signing/notarization pipeline
- Windows code-signing certificate acquisition
- AppImage/deb evaluation if Linux audience materializes
- Portable ZIP for Windows only if a real need appears

## System Audio (Windows Sound)
- Virtual-endpoint/APO transparent insertion path (driver-signing story first)
- Latency-mode selector backed by measured numbers
- Multichannel (>2ch) endpoint support

## Infrastructure
- CI: Windows + macOS runners in the test gate; release pipeline dry-runs
- Crash diagnostics export hardening (opt-in, privacy-preserving)
