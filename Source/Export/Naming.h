#pragma once

#include <juce_core/juce_core.h>

#include "ExportTypes.h"

/*
    Naming — robust export output naming.

    "My Recording" + m4a -> "My Recording.m4a"
    conflict + keepBoth  -> "My Recording (1).m4a", "(2)", ...
    conflict + skip      -> returns {} (caller reports skipped)
    conflict + replace   -> returns the existing path (caller overwrites)

    Names are sanitized for all desktop filesystems and truncated defensively
    so long titles cannot break destination paths.
*/
namespace otoha
{
juce::String sanitizeBaseName (juce::String name);

/** Applies the collision policy; an empty result means "skip". */
juce::File resolveDestination (const juce::File& directory,
                               const juce::String& baseName,
                               ExportFormat format,
                               CollisionPolicy policy);
} // namespace otoha
