#include "RecordingSupport.h"

#include <cmath>

namespace otoha
{
juce::File recordingsDirectory()
{
    // Milestone 3 layout: <Music>/Otoha/Library/Audio.
    // Files saved before Milestone 3 live in the base folder; the library's
    // startup scan adopts them from there.
    auto base = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Otoha");
    if (base.getFullPathName().isEmpty())
        base = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Otoha");
    return base.getChildFile ("Library").getChildFile ("Audio");
}

juce::String makeRecordingFileName (const juce::Time& t)
{
    // Colons are illegal on Windows; dashes keep the stamp readable and portable.
    return t.toString ("%Y-%m-%d %H-%M-%S") + ".wav";
}

juce::File uniqueRecordingFile (const juce::File& directory, const juce::Time& t)
{
    const auto stem = makeRecordingFileName (t).upToLastOccurrenceOf (".", false, false);

    auto candidate = directory.getChildFile (makeRecordingFileName (t));
    int suffix = 2;

    while (candidate.existsAsFile())
        candidate = directory.getChildFile (stem + " (" + juce::String (suffix++) + ").wav");

    return candidate;
}

double samplesToSeconds (juce::int64 samples, double sampleRate)
{
    if (sampleRate <= 0.0 || samples < 0)
        return 0.0;
    return (double) samples / sampleRate;
}

juce::String formatDuration (double seconds)
{
    const bool negative = seconds < 0.0;
    seconds = std::abs (seconds);

    const int totalTenths = (int) std::lround (seconds * 10.0);
    char buffer[32];
    std::snprintf (buffer, sizeof (buffer), "%s%02d:%04.1f",
                   negative ? "-" : "",
                   totalTenths / 600,
                   (totalTenths % 600) / 10.0f);
    return buffer;
}
} // namespace otoha
