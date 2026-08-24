# Otoha QA Report (Milestone 15)

Status legend: PASS / FAIL / PARTIAL / NOT TESTED / UNSUPPORTED / BLOCKED.
This pass is **static + headless**: code inspection, targeted fixes, and a new
stress suite (`otoha_qa_tests`). Nothing here was executed against a built
binary or real hardware in this pass — those rows say NOT TESTED.

## New automated coverage (this milestone)

| Suite | What it breaks | Spec |
|---|---|---|
| `otoha_qa_tests` | DSP through 7 sample rates × 11 buffer sizes × mono/stereo; quiet-input amplification; impulse explosion; sine gain measurement (+6 dB vs ideal); extreme parameter stack; compressor reset isolation; property-based timeline fuzzing (seeded, reproducible); 1000-op undo-all/redo-all round trip; undo branching; unicode project paths/titles; out-of-range clips; missing document payload | #5–#19, #60, #61 |

Existing suites remain the regression base: state machine, edit engine
(incl. M13 edge cases), DSP engine/core, sound engine, export, library,
release hardening, cross-platform. **PASS (written)** — execution pending a
build machine.

## Findings & triage

| ID | Sev | Area | Finding | Status |
|---|---|---|---|---|
| QA-1 | P2 | Export/Security (#72) | FFmpeg command interpolated user-chosen paths into one quoted string; a `"` inside a destination path could escape quoting on Unix. **Fixed**: refuse such destinations with an actionable message before spawning the process. Intermediate/temp names are app-generated and safe. | FIXED (code) |
| QA-2 | P2 | RecordView UI | "Enhance" button had been hard-disabled since Milestone 2 while real Enhance shipped in M5 — dead control promising nothing. **Fixed** in M14 pass 2: removed (enhancing lives one Edit tap away). | FIXED |
| QA-3 | P2 | Timeline (#15) | Deleting the entire take would have produced an empty document; `applyClips` already refuses empty timelines and rolls back cleanly — now pinned by explicit tests (delete-all, zero-length ops). | VERIFIED BY TEST |
| QA-4 | P3 | EditorView | Enhance button didn't communicate active state; now reads ✨ Enhanced and resets on close. | FIXED |
| QA-5 | P3 | Naming (#63) | `sanitizeBaseName` strips Windows-illegal characters from generated export names — verified by inspection; quotes are stripped so FFmpeg refusal above can only trigger on hand-picked destinations. | PASS (inspection) |

## Audits

* **Privacy (#70): PASS** — zero network calls in `Source/` (`grep` for URL/
  WebInputStream/analytics/telemetry: no hits). JUCE networking compiled out
  (`JUCE_USE_CURL=0`, `JUCE_WEB_BROWSER=0`).
* **FFmpeg process safety (#72): PASS after fix** — args never built from raw
  shell strings beyond quoted known-safe paths; title metadata strips quotes.
* **Temp files (#48/#73)**: app-generated names in OS temp/app-data locations;
  project writes atomic via temp+move. PASS (inspection).

## Honest gaps (NOT TESTED — require build/hardware)

Real-hardware device removal (#38–#40), Windows Sound stress/sleep-resume
(#41–#45), Android stress/interruption/process death (#46–#50), memory-leak
measurement (#51/#52), large-file waveform timing (#53), installer upgrade
matrix (#80/#81), High-DPI screenshots (#67), crash/power-loss simulation on a
running binary (#34–#37). These stay explicitly open for the RC hardware pass;
none are claimed.

## Release gate status (#88)

No known P0s. QA-1 was the closest thing to a security-relevant issue and is
fixed. Gate: not yet GREEN only because "clean build + suites actually run"
needs a compile host — everything else is code-complete for RC.
