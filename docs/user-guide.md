# Otoha User Guide

Everything Otoha does, in one page.

## Getting started

1. Open Otoha. You'll see **Record** — press it (choose a countdown first if
   you like a breath before the take).
2. Press **Stop** when done. Your recording is saved to the Library.
3. Click it to **Edit**, or hit **Export** to save it as a file.

## Recording

* **Microphone** — pick your input from the dropdown; friendly names only.
* **Input level** — aim for the "Good" zone. If you see *Clipping detected*,
  move back from the mic or lower the input gain in Windows sound settings.
* **Pause/Resume** mid-take if you need to think.
* Nothing is lost if Otoha crashes mid-recording: audio is written to disk
  continuously, so you get whatever was safely recorded up to that moment.

## Editing

* **Drag on the waveform** to select a range; the readout shows exactly what's
  selected.
* **Cut / Copy / Paste** work like any editor. **Delete** removes the
  selection and closes the gap automatically.
* **Undo/Redo** everything — experiment freely, edits never touch the
  original recording until you Save.
* **Spacebar** plays/pauses; click the waveform to move the playhead.

## Enhance

One tap: **✨ Enhance** applies a clean preset (less background noise, more
even volume). Toggle **Original/Enhanced** to compare instantly. Want more?
The panel has EQ, compressor, limiter and noise reduction with plain-language
controls.

## Exporting

Choose a format and quality:
* **WAV / FLAC** — lossless, perfect copies.
* **M4A / Opus / MP3** — small files for sharing (requires FFmpeg installed
  on your system).
Batch export: select several recordings in the Library → Export.

## Windows Sound

Otoha can enhance *all* system audio (music, videos, calls) — not just
recordings. Open the **Sound** tab, pick your output device, flip it ON, and
try Original vs Enhanced while music plays. Profiles remember per-device
settings.

## Troubleshooting

| Problem | Try this |
|---|---|
| Microphone not detected | Check it works in Windows sound settings; restart Otoha; try another input in the dropdown |
| Recording interrupted | A call or other app took the mic — your take is saved up to the interruption point |
| Export failed (compressed formats) | Install FFmpeg (`ffmpeg.org`), or export WAV/FLAC which need nothing extra |
| Export failed | Check the destination folder exists and isn't read-only |
| "Source unavailable" in editor | The original file was moved/deleted — relocate it or open a different recording |
| Windows Sound shows Unavailable | Select a different output device, then flip ON again; check no other app holds exclusive control of the output |
| Android permission denied | Settings → Apps → Otoha → Permissions → Microphone → Allow |

Still stuck? Open an issue with your version (About screen), OS, and what you
did — see CONTRIBUTING.md.
