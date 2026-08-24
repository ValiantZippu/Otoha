# Otoha Release Matrix (M16 #2/#52/#66)

Filled with actual values. Nothing here is claimed without evidence; every
row carries its honest status. Legend: READY / RELEASE WITH LIMITATIONS /
EXPERIMENTAL / TECHNICALLY PREPARED, NOT DISTRIBUTED / NOT RELEASED.

| Platform | Arch | Min OS | Toolchain | Package | Signing | Status |
|---|---|---|---|---|---|---|
| Windows | x64 | Windows 10 | MSVC (VS 2022) | Inno Setup installer | **NOT SIGNED** (pipeline ready; credentials not configured) | **RELEASE WITH LIMITATIONS** — installer/upgrade/uninstall logic code-complete and scripted, but clean-machine/upgrade runs still require execution on real hardware (docs/release.md checklist). SmartScreen warnings expected (#11). |
| Linux | x64 | any modern distro | GCC ≥ 11 / Clang ≥ 13 | tarball + .desktop + icon | n/a (unsigned by convention) | **EXPERIMENTAL** — packaging script + CI job exist; runtime validation pending. |
| Android | ARM64 | Android 8 (API 26) | Gradle/NDK via JUCE exporter | APK | self-signed keystore (never committed) | **TECHNICALLY PREPARED, NOT DISTRIBUTED** — see docs/android-release.md; no exporter configured yet. |
| macOS | ARM64/x64 | macOS 12 | Xcode/AppleClang | .app / DMG | none (would need Developer ID + notarization) | **EXPERIMENTAL** — code is portable and expected to build; no bundle/packaging configured, NOT TESTED. |
| iOS | ARM64 | iOS 15 | Xcode | IPA | n/a | **NOT RELEASED** — core is portable; no distribution plan for v1. |

## Versioning (#3)

`MAJOR.MINOR.PATCH` — single source: `project(Otoha VERSION …)` in
CMakeLists.txt. Propagated to: app metadata, About screen, installer
(`OTOHA_RELEASE_VERSION`), artifact names, diagnostics, release notes, and
the git tag (`v<version>`, created only after the full release gate passes).

## Build metadata (#4)

`Source/Core/BuildInfo.h` — version, git commit, build date, build type.
Injected at compile time (CMake); release tooling pins the date. Shown in
About; safe for diagnostics export (no machine/user data).

## Debug symbols (#7)

Release packages ship without PDBs/dSYMs. Symbols are retained as CI
artifacts for crash triage, never distributed to users.

## Portable Windows ZIP (#14)

Deliberately NOT provided for v1: settings/profile paths and update behavior
would need a second supported configuration for marginal benefit. Revisit
only with a concrete user need.
