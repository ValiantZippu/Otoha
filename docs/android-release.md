# Android Release Plan (M16 #15–#21)

**Status: TECHNICALLY PREPARED, NOT DISTRIBUTED.**

The shared Studio core (recorder phases, timeline, DSP, project format,
export) is platform-independent and the Android audio input path is
architected (docs/cross-platform.md), but **no Android exporter/Gradle
project is configured yet**, so no APK exists. Nothing on this page is a
claim of a working build.

## What must exist before an Android artifact is real

1. JUCE Android exporter settings in a CMake/Projucer-generated Gradle
   project: application id `app.otoha.android`, `versionCode`/`versionName`
   derived from the single CMake version (#3).
2. Permissions: `RECORD_AUDIO` only, requested at record time (#17/#18).
   No contacts/location/SMS/camera/storage-broad — exports reach users via
   the system share sheet.
3. Icons from `packaging/icons/otoha.svg` rendered to Android mipmap sizes.
4. A release keystore generated locally by the maintainer; passwords live in
   a local `keystore.properties` that is **git-ignored** (#16). Backup of the
   keystore is the maintainer's responsibility — losing it means losing
   update identity.
5. Artifacts: `Otoha-<ver>-Android-arm64.apk` for sideloading; an AAB only if
   Play distribution is actually pursued (#17) — Play readiness (listing,
   data-safety form, testing track) is its own checklist and is NOT claimed.

## Validation gate before calling it READY

launch → grant mic → record → background/lock/resume → stop → edit →
enhance → export WAV/M4A → share, repeated on at least low/mid/modern
devices (#20/#21), plus process-death recovery (#49 of M12 spec). Until that
run is recorded, Android stays out of every release announcement.
