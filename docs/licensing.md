# Licensing

**Status: informational, not legal advice. Flag anything uncertain rather
than assuming compatibility.**

## Otoha's own code

License choice is pending a product decision (recommendation to make before
any public release). Until then, treat the repository as all-rights-reserved.

## Current dependencies

| Dependency | License | How used | Distribution notes |
|------------|---------|----------|-------------------|
| JUCE 8     | Dual: AGPLv3 / commercial | App framework, audio I/O, DSP containers | A commercial JUCE license or AGPL compliance is required for distribution; decide with the product license. |
| SQLite     | Public domain | Library metadata | No obligations. |
| FFmpeg     | LGPL/GPL depending on build flags | Compressed export subprocess | See below. |

## FFmpeg considerations

* Otoha currently invokes **FFmpeg as an external process** (`Source/Export/FfmpegSupport.cpp`),
  which keeps it at arm's length: no FFmpeg code is linked.
* If Otoha later **bundles an FFmpeg binary**, the binary's build determines
  obligations: an LGPL build (no GPL codecs such as `libx264`) generally works
  with proprietary apps given dynamic-linking compliance; GPL builds require
  GPL-compatible distribution. Codec availability also varies by build —
  validate `-version`/encoders at runtime (already implemented).
* MP3 encoding via `libmp3lame` historically carried patent concerns that have
  expired (2017); no action needed today, but re-verify before shipping in
  jurisdictions where this matters for other codecs.

## Explicitly NOT copied

* **FxSound** — studied as product inspiration only; no source or driver code
  from FxSound is included or linked. The future Windows virtual-device layer
  will be an independent implementation.
* **ViPER4Android** — concepts only; no code, no dependency.

## Rule going forward

Any new third-party dependency must be added to the table above with its
license and distribution implications reviewed *before* it enters the build.
