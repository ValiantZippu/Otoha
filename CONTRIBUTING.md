# Contributing to Otoha

Short version: keep Otoha simple. If a change adds a second way to do
something, it probably shouldn't.

## Setup

See **BUILDING.md** — CMake + JUCE (fetched automatically), one dependency
(SQLite). Any platform with those builds the headless test suites.

## Workflow

1. Branch from `freebuff`.
2. Make the change; add/extend a headless test in `Tests/` when behavior is
   testable without hardware.
3. Run the core suites (`ctest` command in BUILDING.md).
4. Open a PR into `freebuff`. Keep PRs small and single-purpose.

## Conventions

* **Architecture**: UI → application → domain → core; platform APIs only in
  `Source/Sound/platform/` (+ platform-gated CMake). See ARCHITECTURE.md.
* **Real-time safety**: audio-callback rules in docs/dsp.md are non-negotiable.
* **Errors** cross layers as `otoha::ErrorCategory`; user text comes from
  `userMessage()`, never raw codes.
* **UI strings**: plain language, no jargon ("Delete", not "Ripple Delete").
  Style lives in `Source/UI/OtohaTheme.h`.
* **No new dependencies** without a licensing note in docs/licensing.md.
* **No secrets, keys, or machine paths** in commits.

## Commit style

Short imperative subject; body explains *why*. Example:
`FfmpegSupport: refuse quote-containing destination paths`
