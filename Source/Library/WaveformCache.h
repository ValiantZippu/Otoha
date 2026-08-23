#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

#include <set>
#include <vector>

#include "LibraryModel.h"

/*
    WaveformCache — compact visual summaries of audio files, generated in the
    background and stored under Cache/Waveforms/.

    File format ("OWF1", tiny by design):
        uint32  magic 'O','W','F','1'
        int32   numBuckets
        int64   source modification time (ms)   <- staleness key
        numBuckets * (float minPeak, float maxPeak)

    The UI asks getPeaks(); if the cache is missing or stale it returns false,
    queues a background job once, and the row draws a placeholder until done.
*/
class WaveformCache
{
public:
    explicit WaveformCache (const juce::File& cacheDirectory);
    ~WaveformCache();

    /** Fills peaksOut (normalised 0..1 magnitudes, fixed resolution).
        Returns false if the cache is stale/missing (generation is then queued). */
    bool getPeaks (const otoha::MediaItem& item, std::vector<float>& peaksOut);

    /** Number of queued/running generation jobs (UI uses this to repaint). */
    int pendingCount() const;

    void cancelPendingJobs();

private:
    void requestGeneration (const otoha::MediaItem& item);
    void generate (otoha::MediaItem item);          // runs on the worker pool

    juce::File cacheDirectory;
    juce::AudioFormatManager formatManager;
    juce::ThreadPool pool { 1 };                    // one worker keeps disk/audio IO gentle

    juce::CriticalSection pendingLock;
    std::set<juce::int64> pendingIds;

    static constexpr int numBuckets = 256;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformCache)
};
