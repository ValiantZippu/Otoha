#pragma once

#include <juce_core/juce_core.h>

/*
    RecordingSupport — small, device-independent pieces of recording logic:
    naming, save location, duration math and time formatting. Kept free of
    audio hardware so tests can exercise them anywhere.
*/
namespace otoha
{
/** Platform-appropriate library location (Music/Otoha, falling back to ~/Otoha). */
juce::File recordingsDirectory();

/** "YYYY-MM-DD HH-MM-S.wav" — valid on Windows, macOS and Linux. */
juce::String makeRecordingFileName (const juce::Time& t);

/** Same as makeRecordingFileName but never collides with an existing file:
    a " (2)"-style suffix is appended when needed. */
juce::File uniqueRecordingFile (const juce::File& directory, const juce::Time& t);

/** Duration of `samples` frames at `sampleRate`; 0 for invalid inputs. */
double samplesToSeconds (juce::int64 samples, double sampleRate);

/** "MM:SS.t" (e.g. "00:42.1"); negative durations get a leading '-'. */
juce::String formatDuration (double seconds);
} // namespace otoha
