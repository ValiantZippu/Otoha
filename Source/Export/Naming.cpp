#include "Naming.h"

namespace otoha
{
juce::String sanitizeBaseName (juce::String name)
{
    // Strip characters that are illegal on Windows and awkward elsewhere.
    name = name.replaceCharacters ("\\/:*?\"<>|", "________").trim();

    // Defensive length cap (bytes) so long titles can't break path limits.
    constexpr int maxChars = 180;
    if (name.length() > maxChars)
        name = name.substring (0, maxChars);

    return name;
}

juce::File resolveDestination (const juce::File& directory,
                               const juce::String& baseName,
                               ExportFormat format,
                               CollisionPolicy policy)
{
    const auto caps = capabilitiesFor (format);
    const auto clean = sanitizeBaseName (baseName);

    auto destination = directory.getChildFile (clean + "." + caps.extension);

    if (! destination.existsAsFile() || policy == CollisionPolicy::replace)
        return destination;

    if (policy == CollisionPolicy::skip)
        return {};   // caller reports the job as skipped

    // keepBoth
    int suffix = 1;
    for (;;)
    {
        destination = directory.getChildFile (clean + " (" + juce::String (suffix++) + ")."
                                              + caps.extension);
        if (! destination.existsAsFile())
            return destination;
    }
}
} // namespace otoha
