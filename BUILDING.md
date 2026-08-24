# Building Otoha

CMake ≥ 3.22, C++20. JUCE 8.0.8 is fetched automatically on first configure.

## Build matrix (M13 #56 — record actual versions when you build)

| Platform | Toolchain | Status |
|---|---|---|
| Windows | Visual Studio 2022 (MSVC), Windows SDK per VS | validated through M9/M10 process |
| Linux | GCC ≥ 11 or Clang ≥ 13 | headless suites + GUI app; CI runs the core suites |
| macOS | Xcode / Apple Clang | expected to build (portable code + JUCE); **NOT TESTED** in CI yet — report actual result, never assume |
| Android | Android NDK via JUCE's Gradle exporter | architecture ready; app export not set up yet — **NOT TESTED** |
| iOS | Apple toolchain | core modules are portable by design — **NOT TESTED** |

Do not claim a platform builds unless you built it.

## Linux dependencies

```sh
sudo apt install pkg-config libsqlite3-dev libasound2-dev libfreetype6-dev \
  libfontconfig1-dev libx11-dev libxext-dev libxrandr-dev libxi-dev \
  libgl1-mesa-dev libcurl4-openssl-dev
```

SQLite note: if no system SQLite dev package is found (the normal case on
Windows), CMake automatically downloads the official amalgamation from
sqlite.org and builds it — no manual step is required anywhere.

## Configure & build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j          # Windows: add --config Release
```

The WASAPI Sound backend compiles only on Windows (`if(WIN32)` gate); every
other platform builds Studio plus the honest unsupported-Sound stub.

## Tests

Headless suites (no audio device needed) — these are the CI gate:

```sh
ctest --test-dir build \
  -R "wav_round_trip|state_machine|support|edit_engine|dsp_engine|dsp_core|cross_platform" \
  --output-on-failure
```

Suites needing a database (`library`, `export_system`, `release_hardening`,
`sound_engine`) run locally/CI-with-deps; see CMakeLists.txt for the full list.

## Release (Windows)

See `docs/release.md` and `scripts/release.sh` — installer, checksums, and the
RC checklist live there.

## FFmpeg note

Compressed export shells out to a user-installed `ffmpeg` binary. Nothing
bundles it. Without FFmpeg, WAV/FLAC still work everywhere.
