#pragma once

#include <juce_core/juce_core.h>

#ifndef OTOHA_VERSION
 #define OTOHA_VERSION "dev"
#endif

/*
    BuildInfo — the single source of build identity (M16 #3/#4).

    Version comes from CMake's project() (OTOHA_VERSION). Commit and build
    date are injected by the release script / CI at compile time; local dev
    builds honestly report "dev" rather than a fake value.

    Shown in About + diagnostics exports. Never includes machine names,
    paths, or user data.
*/

namespace otoha::build
{
inline juce::String version()     { return juce::String (OTOHA_VERSION); }

inline juce::String commit()
{
#ifdef OTOHA_GIT_COMMIT
    return juce::String (OTOHA_GIT_COMMIT);
#else
    return "dev";
#endif
}

inline juce::String buildDate()
{
#ifdef OTOHA_BUILD_DATE
    return juce::String (OTOHA_BUILD_DATE);
#else
    return "dev";
#endif
}

inline juce::String buildType()
{
#ifdef JUCE_DEBUG
    return "Debug";
#else
    return "Release";
#endif
}

/** One-line identity for logs/diagnostics: version | commit | date | type. */
inline juce::String summary()
{
    return "Otoha " + version() + " (" + buildType() + ", " + commit() + ", " + buildDate() + ")";
}
} // namespace otoha::build
