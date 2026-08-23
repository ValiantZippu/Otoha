#pragma once

#include <juce_core/juce_core.h>

/*
    ExportTypes — formats, qualities, policies and per-format capabilities.

    The UI derives everything it shows from capabilitiesFor(); format options
    are never hardcoded in multiple screens.
*/

namespace otoha
{
enum class ExportFormat { wav, flac, m4a, opus, mp3 };

enum class ExportQuality { small, standard, high };

enum class CollisionPolicy { keepBoth, replace, skip };

struct FormatCapabilities
{
    juce::String displayName;
    const char* extension;
    bool lossless;          // never call a lossy format lossless (#40)
    bool requiresFfmpeg;
};

inline FormatCapabilities capabilitiesFor (ExportFormat f)
{
    switch (f)
    {
        case ExportFormat::wav:  return { "WAV (lossless)",       "wav",  true,  false };
        case ExportFormat::flac: return { "FLAC (lossless)",      "flac", true,  false };
        case ExportFormat::m4a:  return { "M4A (compressed)",     "m4a",  false, true  };
        case ExportFormat::opus: return { "Opus (compressed)",    "opus", false, true  };
        case ExportFormat::mp3:  return { "MP3 (compressed)",     "mp3",  false, true  };
    }
    return { "?", "bin", false, false };
}

/** Guaranteed only when the encoder configuration actually sets them (#41). */
inline int bitrateKbpsFor (ExportFormat f, ExportQuality q)
{
    switch (f)
    {
        case ExportFormat::m4a:
            return q == ExportQuality::small ? 96 : q == ExportQuality::high ? 256 : 160;
        case ExportFormat::opus:
            return q == ExportQuality::small ? 64 : q == ExportQuality::high ? 192 : 128;
        case ExportFormat::mp3:
            return q == ExportQuality::small ? 96 : q == ExportQuality::high ? 320 : 192;
        case ExportFormat::wav:
        case ExportFormat::flac:
            break;   // lossless — bitrate is content-dependent
    }
    return 0;
}

inline juce::String codecNameFor (ExportFormat f)
{
    switch (f)
    {
        case ExportFormat::m4a:  return "AAC";
        case ExportFormat::opus: return "Opus";
        case ExportFormat::mp3:  return "MP3";
        default: break;
    }
    return "PCM";
}

inline juce::String formatToString (ExportFormat f)   { return capabilitiesFor (f).extension; }

/** Human label like "High" or "Standard"; advanced UI can append technicals. */
inline juce::String qualityLabel (ExportFormat f, ExportQuality q)
{
    const char* base = q == ExportQuality::small ? "Small"
                     : q == ExportQuality::high  ? "High" : "Standard";
    if (bitrateKbpsFor (f, q) > 0)
        return juce::String (base) + " (" + juce::String (codecNameFor (f)) + " · "
             + juce::String (bitrateKbpsFor (f, q)) + " kbps)";
    return base;
}
} // namespace otoha
