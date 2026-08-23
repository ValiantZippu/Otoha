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

    // --- M10: parity with the Windows backend's extensions so the Sound UI
    // compiles against either backend; every value is the honest "none".
    void setSourceDevice (const std::string&) {}

    struct Status
    {
        enum class Code { ok, notImplemented, noDevice, feedbackLoop,
                          multichannelUnsupported, deviceLost, comFailure, genericError };
        Code code = Code::notImplemented;
        std::string message;
    };

    Status getStatus() const
    {
        Status s;
        s.message = reason;
        return s;
    }

    double getLatencyMs() const { return -1.0; }                 // unknown (#27)
    unsigned long long getUnderruns() const { return 0; }
    bool defaultDeviceChangedSinceLastCheck() { return false; }

    const std::string& getUnsupportedReason() const { return reason; }

private:
    std::string reason;
};
} // namespace otoha::platform
