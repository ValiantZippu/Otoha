#pragma once

#include <atomic>
#include <functional>

#include <juce_core/juce_core.h>

#include "ExportTypes.h"

/*
    FfmpegSupport — locating and driving an external FFmpeg binary.

    Strategy (documented, never a blind PATH assumption):
      1. user-configured binary  (stored in the Otoha settings properties)
      2. bundled binary          (sibling of the executable: ./ffmpeg[.exe])
      3. PATH                    (validated before use — never trusted blindly)

    Supported range: FFmpeg 4.x–7.x with libmp3lame + libopus + AAC enabled
    (the standard GPL builds). Anything else reports Unsupported/Unavailable
    and the UI falls back to WAV/FLAC — compressed export is optional.

    Licensing note for distribution: bundling FFmpeg binaries triggers
    LGPL/GPL obligations depending on the build's codecs. This module is the
    ONLY place that touches FFmpeg so a distribution strategy can change
    (bundle / download at runtime / rely on system installs) without ripples.
*/
namespace otoha
{
enum class EncoderStatus { unavailable, available, unsupported, error };

struct FfmpegInfo
{
    juce::String path;         // empty when unavailable
    juce::String versionText;  // e.g. "ffmpeg version 6.1.1 ..."
};

class FfmpegLocator
{
public:
    /** Locates and validates (runs `-version`). Result is cached. */
    EncoderStatus locate (FfmpegInfo& out);

    /** Overrides discovery (used by the future Settings screen). */
    static void setCustomPath (const juce::File& otohaBaseDirectory, const juce::String& path);
    static juce::String getCustomPath (const juce::File& otohaBaseDirectory);

private:
    EncoderStatus probePath (const juce::String& executable, FfmpegInfo& out) const;

    bool cached = false;
    EncoderStatus cachedStatus = EncoderStatus::unavailable;
    FfmpegInfo cachedInfo;
};

/** Encodes one finished intermediate audio file into `destination`.
    The PCM render itself stays in AudioExporter; this is purely process I/O. */
class FfmpegEncoder
{
public:
    struct Request
    {
        ExportFormat format = ExportFormat::m4a;
        int bitrateKbps = 160;
        double sourceSampleRate = 48000.0;
        int channels = 2;
        juce::double_sec durationSeconds { 0.0 };
        juce::String titleMetadata;   // -metadata title=... where supported
    };

    /** Blocking; call from a background thread only. Checks cancelFlag between
        wait polls and terminates FFmpeg cleanly on cancellation. */
    static bool encode (const juce::File& ffmpegExecutable,
                        const juce::File& intermediateAudio,
                        const juce::File& destination,
                        const Request& request,
                        const std::atomic<bool>& cancelFlag,
                        std::function<void (float progress01)> progress,
                        juce::String& errorOut);
};
} // namespace otoha
