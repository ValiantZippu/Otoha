#pragma once

#include <juce_core/juce_core.h>

/*
    otoha::log — structured, privacy-preserving logging (M12 #66).

    Levels: error < warning < info < debug. Debug lines compile out of Release
    builds entirely. Absolute rules, enforced by convention and reviewed in the
    token-security audits:

      * never log raw audio or microphone content
      * never log tokens/secrets (see the Phase-9-style audit list)
      * never log full file contents — names and sizes only

    Output goes through JUCE's Logger so each platform can route it to its own
    facility (OutputDebugString / syslog / logcat) without shared code caring.
*/

namespace otoha::log
{
enum class Level { error, warning, info, debug };

inline void write (Level level, const juce::String& message)
{
    if (level == Level::debug)
    {
#ifndef JUCE_DEBUG
        return;   // debug detail never ships in Release builds
#endif
    }

    const char* tag = "E";
    switch (level)
    {
        case Level::error:   tag = "E"; break;
        case Level::warning: tag = "W"; break;
        case Level::info:    tag = "I"; break;
        case Level::debug:   tag = "D"; break;
    }

    if (auto* logger = juce::Logger::getCurrentLogger())
        logger->writeToLog ("[otoha " + juce::String (tag) + "] " + message);
}

inline void error   (const juce::String& m) { write (Level::error,   m); }
inline void warning (const juce::String& m) { write (Level::warning, m); }
inline void info    (const juce::String& m) { write (Level::info,    m); }
inline void debug   (const juce::String& m) { write (Level::debug,   m); }
} // namespace otoha::log
