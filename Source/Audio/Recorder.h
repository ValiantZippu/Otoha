#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include <atomic>
#include <thread>

#include "RecordingState.h"

/*
    Recorder — captures input from the shared AudioDeviceManager into a WAV file.

    Ownership and threading:
      - The Recorder OWNS the recording state (otoha::TransportState). The UI
        observes it and requests transitions; it never manipulates the callback.
      - The audio callback only meters, optionally monitors, and copies samples
        into a lock-free FIFO. No allocations, locks, I/O or logging there.
      - A background writer thread drains the FIFO to the AudioFormatWriter and
        feeds an AudioThumbnail for the live waveform.
      - Abnormal endings (device lost, write failure) are recorded as a
        FailureReason that the UI consumes on its own timer.
*/
class Recorder : private juce::AudioIODeviceCallback
{
public:
    explicit Recorder (juce::AudioDeviceManager& deviceManager);
    ~Recorder() override;

    /** idle -> recording. Fails (with a user-facing message) if a take is already open,
        no input exists, or the file cannot be created. */
    bool startRecording (const juce::File& targetFile, int bitDepth, juce::String& errorOut);

    /** recording -> paused. Returns false if not recording. */
    bool pauseRecording();

    /** paused -> recording. Returns false if not paused. */
    bool resumeRecording();

    /** recording|paused -> idle. Drains the FIFO and finalises the WAV header.
        Safe to call from any state; safe to call repeatedly. */
    void stopRecording();

    otoha::TransportState getState() const { return state.load (std::memory_order_relaxed); }

    juce::File getCurrentFile() const  { return currentFile; }

    /** Route input straight to output (latency of one buffer). */
    void setMonitoring (bool shouldBeOn)  { monitoring.store (shouldBeOn, std::memory_order_relaxed); }

    // Metering (updated on the audio thread, read from the UI)
    float getLevelRms() const   { return levelRms.load (std::memory_order_relaxed); }
    float getLevelPeak() const  { return levelPeak.load (std::memory_order_relaxed); }
    bool  hasClipped() const    { return clipped.load (std::memory_order_relaxed); }
    void  clearClipIndicator()  { clipped.store (false, std::memory_order_relaxed); }

    /** True when the current device exposes at least one input channel. */
    bool hasInput() const { return deviceNumInputs > 0 && deviceSampleRate > 0.0; }

    juce::int64 getTotalSamples() const { return totalSamples.load (std::memory_order_relaxed); }
    double getSampleRate() const        { return deviceSampleRate; }
    int getNumInputChannels() const     { return deviceNumInputs; }

    /** Returns and clears the pending failure reason (if any). */
    otoha::FailureReason consumeFailure()
    {
        return failureReason.exchange (otoha::FailureReason::none, std::memory_order_relaxed);
    }

    juce::AudioThumbnail& getThumbnail()  { return thumbnail; }

private:
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                           float* const* outputChannelData, int numOutputChannels,
                                           int numFrames,
                                           const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceStopped() override;
    void audioDeviceError (const juce::String&) override;

    void writerThreadLoop();

    juce::AudioDeviceManager& deviceManager;

    juce::AudioFormatManager formatManager;
    juce::TimeSliceThread thumbnailThread { "Otoha Thumbnail" };
    juce::AudioThumbnailCache thumbnailCache { 8 };
    juce::AudioThumbnail thumbnail;

    // Shared with the audio callback
    static constexpr int fifoCapacity = 1 << 17;              // frames per channel (~2.7 s @ 48 kHz)
    juce::AudioBuffer<float> fifoBuffer { 2, fifoCapacity };
    juce::AbstractFifo fifo { fifoCapacity };
    std::atomic<otoha::TransportState> state { otoha::TransportState::idle };
    std::atomic<bool> monitoring { false };
    std::atomic<float> levelRms { 0.0f };
    std::atomic<float> levelPeak { 0.0f };
    std::atomic<bool> clipped { false };
    std::atomic<juce::int64> totalSamples { 0 };

    // Owned by the writer thread / message thread
    std::atomic<bool> threadRunning { false };
    std::thread writer;
    std::unique_ptr<juce::AudioFormatWriter> activeWriter;
    juce::CriticalSection writerLock;
    juce::File currentFile;
    double deviceSampleRate = 0.0;
    int deviceNumInputs = 0;
    double writerSampleRate = 0.0;       // what the open take was created with
    int writerNumInputs = 0;
    juce::int64 thumbnailPosition = 0;

    std::atomic<otoha::FailureReason> failureReason { otoha::FailureReason::none };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Recorder)
};
