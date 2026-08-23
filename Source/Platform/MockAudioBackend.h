#pragma once

#include "AudioBackend.h"

#include <atomic>
#include <string>
#include <vector>

/*
    MockAudioBackend — proves the Otoha Sound pipeline shape with zero
    platform dependencies:

        generated/captured audio -> processStage -> output sink

    It needs no virtual drivers, no OS routing, and no hardware, so the
    end-to-end architecture test (Tests/DspCoreTests.cpp) can run anywhere.

    The mock is pull-driven: `deliverBlock()` simulates the platform audio
    thread handing in a block of captured audio. The backend runs it through
    the registered process stage (normally an otoha::DspChain wrapper) and
    forwards the result to the output sink — exactly the path a real WASAPI /
    Core Audio / PipeWire adapter will take.
*/

namespace otoha::platform
{
class MockAudioBackend : public AudioBackend
{
public:
    // -- AudioBackend -------------------------------------------------------
    bool initialize (const AudioStreamConfig& newConfig) override
    {
        config = newConfig;
        initialized = true;
        return true;
    }

    void shutdown() override { stop(); }

    bool start() override
    {
        if (! initialized)
            return false;
        running = true;
        return true;
    }

    void stop() override { running = false; }

    std::vector<AudioDeviceInfo> getDevices() const override
    {
        return { { "mock-out", "Otoha Mock Output", true } };
    }

    bool setActiveDevice (const std::string& deviceId) override
    {
        activeDevice = deviceId;
        return deviceId == "mock-out";
    }

    void setOutputSink (AudioOutputSink* sink) override { output = sink; }

    void setProcessStage (ProcessStage stage) override { process = std::move (stage); }

    AudioStreamConfig getStreamConfig() const override { return config; }

    // -- Test harness ---------------------------------------------------------

    /** Simulates one platform-audio-thread callback with caller-filled data. */
    void deliverBlock (float* const* channels, int numFrames)
    {
        if (process)
        {
            dsp::AudioBlock block (channels, config.numChannels, numFrames);
            process (block);
        }
        if (output != nullptr)
        {
            dsp::AudioBlock block (channels, config.numChannels, numFrames);
            output->writeBlock (block);
        }
    }

private:
    AudioStreamConfig config;
    ProcessStage process;
    AudioOutputSink* output = nullptr;
    std::string activeDevice;
    std::atomic<bool> initialized { false };
    std::atomic<bool> running { false };
};
} // namespace otoha::platform
