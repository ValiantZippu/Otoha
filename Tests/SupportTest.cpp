/*
    SupportTest — headless verification of naming, save-location and duration
    helpers (device independent).
*/
#include "../Source/Core/RecordingSupport.h"

#include <cstdio>

namespace
{
bool expect (bool condition, const char* message)
{
    if (! condition)
        std::printf ("FAIL: %s\n", message);
    return condition;
}

/** "YYYY-MM-DD HH-MM-SS.wav" with digits in the right places (timezone-safe). */
bool looksLikeRecordingName (const juce::String& name)
{
    const auto pattern = "0000-00-00 00-00-00.wav";
    if (name.length() != juce::String (pattern).length())
        return false;

    for (int i = 0; i < name.length(); ++i)
    {
        const auto expectedDigit = pattern[i] == '0';
        const bool isDigit = name[i] >= '0' && name[i] <= '9';
        if (expectedDigit != isDigit)
            return false;
    }
    return true;
}
} // namespace

int main()
{
    using namespace otoha;
    bool ok = true;

    // --- filename generation -------------------------------------------------
    ok &= expect (looksLikeRecordingName (makeRecordingFileName (juce::Time::getCurrentTime())),
                  "filename must look like YYYY-MM-DD HH-MM-SS.wav");

    // Windows-illegal characters never appear.
    const auto name = makeRecordingFileName (juce::Time::getCurrentTime());
    ok &= expect (! name.containsAnyOf ("\\/:*?\"<>|"), "filename contains a platform-illegal character");

    // --- uniqueness ----------------------------------------------------------
    const auto dir = juce::File::createTempFile ("otoha_naming_dir");
    dir.createDirectory();
    const auto t = juce::Time::getCurrentTime();

    const auto first  = uniqueRecordingFile (dir, t);
    first.create();
    const auto second = uniqueRecordingFile (dir, t);

    ok &= expect (first != second, "uniqueRecordingFile must not collide with an existing file");
    ok &= expect (second.getFileName().contains ("("), "collision suffix missing");
    dir.deleteRecursively();

    // --- duration math -------------------------------------------------------
    ok &= expect (samplesToSeconds (48000, 48000) == 1.0, "one second of samples at 48 kHz");
    ok &= expect (samplesToSeconds (0, 48000) == 0.0, "zero samples is zero seconds");
    ok &= expect (samplesToSeconds (48000, 0.0) == 0.0, "invalid rate must yield 0, not divide by zero");
    ok &= expect (samplesToSeconds (-5, 48000) == 0.0, "negative samples must be rejected");

    // --- formatting ------------------------------------------------------------
    ok &= expect (formatDuration (0.0) == "00:00.0", "zero formats as 00:00.0");
    ok &= expect (formatDuration (1.24) == "00:01.2", "1.24 s formats as 00:01.2");
    ok &= expect (formatDuration (65.0) == "01:05.0", "65 s formats as 01:05.0");
    ok &= expect (formatDuration (-2.0) == "-00:02.0", "negative durations keep their sign");

    if (! ok) return 1;
    std::printf ("PASS: naming, duration math and formatting\n");
    return 0;
}
