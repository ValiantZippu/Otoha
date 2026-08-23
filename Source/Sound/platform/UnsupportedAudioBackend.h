#pragma once

#include "../../Platform/AudioBackend.h"

#include <string>

/*
    UnsupportedAudioBackend — honest placeholder for platforms whose real
    backend is not implemented yet (macOS / Linux / Android / iOS in M8).

    Reports "not implemented" through its status; never fakes functionality.
    The Otoha Sound UI uses getUnsupportedReason() to show exactly that.
*/
namespace otoha::platform
{
class UnsupportedAudioBackend : public AudioBackend
{
public:
    explicit UnsupportedAudioBackend (std::string platformName)
        : reason ("Otoha Sound is not implemented on " + platformName + " yet.") {}

    bool initialize (const AudioStreamConfig&) override { return false; }
    void shutdown() override {}
    bool start() override { return false; }
    void stop() override {}

    std::vector<AudioDeviceInfo> getDevices() const override { return {}; }
    bool setActiveDevice (const std::string&) override { return false; }
    void setOutputSink (AudioOutputSink*) override {}
    void setProcessStage (ProcessStage) override {}
    AudioStreamConfig getStreamConfig() const override { return {}; }

    const std::string& getUnsupportedReason() const { return reason; }

private:
    std::string reason;
};
} // namespace otoha::platform
