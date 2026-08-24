# Otoha Privacy Statement

Short version: **your audio never leaves your device.**

## What Otoha stores

* **Recordings** — as standard audio files in your user data directory
  (Windows: `%APPDATA%\Otoha\Recordings`; Linux/macOS equivalents per
  `Source/Core/AppPaths.h`).
* **Projects** (`*.otoha` folders) wherever you save them; they reference
  your recordings, they are not uploaded anywhere.
* **Settings, presets, device profiles** — small JSON files next to the
  recordings.

## What leaves your device

Nothing. Otoha makes **no network connections**: no telemetry, no analytics,
no crash uploads, no update pings (the update checker in About is disabled by
default and does nothing unless a server is explicitly configured).

The only external process Otoha may run is an FFmpeg binary **you installed
yourself**, and only when you export to M4A/Opus/MP3. It runs locally on
your file.

## Microphone access

Otoha uses the microphone only while you are recording. On Android the
permission is requested at the moment you first press Record, and a system
indicator is always visible during capture.

## Deleting your data

Uninstalling Otoha removes the application but keeps your recordings and
projects. You can delete everything by removing the Otoha folder in your
user data directory (the uninstaller asks before touching it).
