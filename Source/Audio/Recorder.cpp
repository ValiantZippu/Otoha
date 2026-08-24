#include "Recorder.h"

Recorder::Recorder (juce::AudioDeviceManager& dm)
    : deviceManager (dm),
      thumbnail (1024, formatManager, thumbnailCache)
{
    formatManager.registerBasicFormats();
    thumbnailThread.startThread (juce::Thread::Priority::low);
    deviceManager.addAudioCallback (this);
}

Recorder::~Recorder()
{
    stopRecording();
    deviceManager.removeAudioCallback (this);
    thumbnailThread.stopThread (5000);
}

// =============================================================================
// Device lifecycle
// =============================================================================
void Recorder::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    const double newRate   = device->getCurrentSampleRate();
    const int    newInputs = juce::jmin ((int) device->getActiveInputChannels().countNumberOfSetBits(), 2);

    // A rate/channel change mid-take would corrupt the open WAV; flag it and let
    // the UI end the take gracefully.
    if (state.load (std::memory_order_relaxed) != otoha::TransportState::idle
        && activeWriter != nullptr
        && (newRate != writerSampleRate || newInputs != writerNumInputs))
    {
        failureReason.store (otoha::FailureReason::deviceLost, std::memory_order_relaxed);
    }

    deviceSampleRate = newRate;
    deviceNumInputs  = newInputs;
}

void Recorder::audioDeviceStopped()
{
    if (state.load (std::memory_order_relaxed) != otoha::TransportState::idle)
        failureReason.store (otoha::FailureReason::deviceLost, std::memory_order_relaxed);

    levelRms.store  (0.0f, std::memory_order_relaxed);
    levelPeak.store (0.0f, std::memory_order_relaxed);
}

void Recorder::audioDeviceError (const juce::String&)
{
}

// =============================================================================
// Audio callback — realtime safe: metering + FIFO push only
// =============================================================================
void Recorder::audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                                 float* const* outputChannelData, int numOutputChannels,
                                                 int numFrames,
                                                 const juce::AudioIODeviceCallbackContext&)
{
    // --- metering (always, so the UI shows levels before recording) ----------
    float rmsAccumulator = 0.0f;
    float peak = 0.0f;

    for (int i = 0; i < numInputChannels; ++i)
    {
        if (inputChannelData[i] == nullptr)
            continue;

        float channelSum = 0.0f;
        float channelPeak = 0.0f;

        for (int j = 0; j < numFrames; ++j)
        {
            const auto s = inputChannelData[i][j];
            channelSum += s * s;
            channelPeak = juce::jmax (channelPeak, std::abs (s));
        }

        rmsAccumulator += channelSum / (float) numFrames;
        peak = juce::jmax (peak, channelPeak);
    }

    if (numInputChannels > 0)
    {
        levelRms.store  (std::sqrt (rmsAccumulator / (float) numInputChannels), std::memory_order_relaxed);
        levelPeak.store (peak, std::memory_order_relaxed);

        if (! clipped.load (std::memory_order_relaxed) && peak >= 0.9995f)
            clipped.store (true, std::memory_order_relaxed);
    }
    else
    {
        levelRms.store  (0.0f, std::memory_order_relaxed);
        levelPeak.store (0.0f, std::memory_order_relaxed);
    }

    // --- output silence, plus optional input monitoring ----------------------
    for (int i = 0; i < numOutputChannels; ++i)
        if (outputChannelData[i] != nullptr)
            juce::FloatVectorOperations::clear (outputChannelData[i], numFrames);

    if (monitoring.load (std::memory_order_relaxed))
    {
        for (int out = 0; out < numOutputChannels; ++out)
        {
            const int in = out % juce::jmax (1, numInputChannels);
            if (in < numInputChannels && inputChannelData[in] != nullptr
                && outputChannelData[out] != nullptr)
                juce::FloatVectorOperations::copy (outputChannelData[out], inputChannelData[in], numFrames);
        }
    }

    // --- capture: copy into the lock-free FIFO only --------------------------
    if (state.load (std::memory_order_relaxed) != otoha::TransportState::recording)
        return;

    int start1, size1, start2, size2;
    fifo.prepareToWrite (numFrames, start1, size1, start2, size2);

    const int channels = juce::jmin (deviceNumInputs, fifoBuffer.getNumChannels());
    auto copyIntoFifo = [this, &inputChannelData, channels] (int destStart, int sourceStart, int numSamples)
    {
        for (int ch = 0; ch < channels; ++ch)
        {
            if (inputChannelData[ch] == nullptr)
                continue;
            juce::FloatVectorOperations::copy (fifoBuffer.getWritePointer (ch, destStart),
                                               inputChannelData[ch] + sourceStart, numSamples);
        }
    };

    if (size1 > 0) copyIntoFifo (start1, 0, size1);
    if (size2 > 0) copyIntoFifo (start2, size1, size2);
    fifo.finishedWrite (size1 + size2);
}

// =============================================================================
// State transitions (message thread)
// =============================================================================
bool Recorder::startRecording (const juce::File& targetFile, int bitDepth, juce::String& errorOut)
{
    if (state.load() != otoha::TransportState::idle)
    {
        errorOut = "A recording is already in progress.";
        return false;
    }

    if (deviceSampleRate <= 0.0 || deviceNumInputs <= 0)
    {
        errorOut = "No microphone available.\nConnect a microphone and try again.";
        return false;
    }

    currentFile = targetFile;
    currentFile.getParentDirectory().createDirectory();

    std::unique_ptr<juce::FileOutputStream> stream (currentFile.createOutputStream());
    if (stream == nullptr || stream->failedToOpen())
    {
        errorOut = "Otoha couldn't start saving the recording.\n"
                   "Check that there is enough storage space and the folder is writable:\n"
                   + currentFile.getParentDirectory().getFullPathName();
        return false;
    }

    {
        const juce::ScopedLock sl (writerLock);
        activeWriter.reset (juce::WavAudioFormat().createWriterFor (stream.release(),
                                                                    deviceSampleRate,
                                                                    (unsigned int) deviceNumInputs,
                                                                    (unsigned int) bitDepth,
                                                                    {}, 0));
    }

    if (activeWriter == nullptr)
    {
        errorOut = "Otoha couldn't create the audio file:\n" + currentFile.getFullPathName();
        return false;
    }

    writerSampleRate = deviceSampleRate;
    writerNumInputs  = deviceNumInputs;
    totalSamples.store (0);
    thumbnailPosition = 0;
    failureReason.store (otoha::FailureReason::none);
    thumbnail.reset (juce::jmax (1, deviceNumInputs), deviceSampleRate);

    fifo.reset();
    threadRunning.store (true);
    writer = std::thread ([this] { writerThreadLoop(); });

    state.store (otoha::TransportState::recording);   // last: opens the callback gate
    return true;
}

bool Recorder::pauseRecording()
{
    const auto current = state.load();
    if (current != otoha::TransportState::recording
        || ! otoha::isValidTransition (current, otoha::TransportState::paused))
        return false;

    state.store (otoha::TransportState::paused);
    return true;
}

bool Recorder::resumeRecording()
{
    const auto current = state.load();
    if (current != otoha::TransportState::paused
        || ! otoha::isValidTransition (current, otoha::TransportState::recording))
        return false;

    state.store (otoha::TransportState::recording);
    return true;
}

void Recorder::stopRecording()
{
    const auto current = state.load();

    if (current != otoha::TransportState::idle)
    {
        if (! otoha::isValidTransition (current, otoha::TransportState::idle))
            return; // defensive: never corrupt a take with an invalid transition

        state.store (otoha::TransportState::idle);      // first: closes the callback gate
    }

    threadRunning.store (false);
    if (writer.joinable())
        writer.join();

    const juce::ScopedLock sl (writerLock);
    if (activeWriter != nullptr)
        activeWriter->flush();   // keeps the file valid immediately; header finalised on close
    activeWriter = nullptr;
}

// =============================================================================
// Writer thread — FIFO -> file + live waveform peaks
// =============================================================================
void Recorder::writerThreadLoop()
{
    float* channels[2] = { nullptr, nullptr };
    int start1, size1, start2, size2;

    while (threadRunning.load (std::memory_order_relaxed))
    {
        fifo.prepareToRead (fifoCapacity, start1, size1, start2, size2);

        if (size1 + size2 == 0)
        {
            juce::Thread::sleep (2);
            continue;
        }

        const juce::ScopedLock sl (writerLock);
        if (activeWriter == nullptr)
        {
            fifo.finishedRead (size1 + size2);
            continue;
        }

        const int numChannels = juce::jmin (activeWriter->getNumChannels(), fifoBuffer.getNumChannels());
        auto writeRange = [&] (int start, int numSamples)
        {
            // Write-pointer access on our own FIFO is intentional: the writer
            // thread owns this data between prepareToRead/finishedRead, and
            // AudioFormatWriter::write plus the thumbnail's wrapping buffer
            // both need mutable pointers.
            for (int ch = 0; ch < numChannels; ++ch)
                channels[ch] = fifoBuffer.getWritePointer (ch, start);

            if (! activeWriter->write (channels, numSamples))
                failureReason.store (otoha::FailureReason::diskFull, std::memory_order_relaxed);

            juce::AudioBuffer<float> view (channels, numChannels, numSamples); // wraps, no copy
            thumbnail.addBlock (thumbnailPosition, view, 0, numSamples);
            thumbnailPosition += numSamples;
        };

        if (size1 > 0) writeRange (start1, size1);
        if (size2 > 0) writeRange (start2, size2);

        fifo.finishedRead (size1 + size2);
        totalSamples.fetch_add (size1 + size2, std::memory_order_relaxed);
    }
}
