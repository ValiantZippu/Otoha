#pragma once

#include "../../Platform/AudioBackend.h"

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

/*
    WindowsAudioBackend — M8's production backend for Otoha Sound on Windows.

    Architecture chosen (user-mode, no driver required):

        Applications -> Windows audio session (shared mode)
                             |
                     WASAPI LOOPBACK CAPTURE          (source render endpoint)
                             |
                      Otoha DSP Core                  (process stage)
                             |
                     WASAPI SHARED RENDER             (chosen output endpoint)

    Why this shape:
      * Shared mode keeps the user's system usable (no exclusive steal).
      * Loopback capture receives whatever plays on a render endpoint without
        a virtual audio device or kernel driver (#14: prefer user-mode).
      * A virtual-device/APO architecture remains possible later; it would be
        an entirely separate platform component behind this same interface
        (#11/#12) and would need driver signing — out of scope for M8.

    IMPORTANT routing constraint (#41): capturing endpoint X while rendering
    to X creates a feedback loop. initialize() refuses that combination.

    Everything COM/WASAPI lives in the .cpp guarded by JUCE_WINDOWS; on other
    platforms every call reports a clean error ("not implemented").
*/
namespace otoha::platform
{
class WindowsAudioBackend : public AudioBackend
{
public:
    WindowsAudioBackend();
    ~WindowsAudioBackend() override;

    // -- AudioBackend (control thread unless noted) ---------------------------
    bool initialize (const AudioStreamConfig& config) override;
    void shutdown() override;
    bool start() override;
    void stop() override;

    /** Render (output) endpoints. `id` is the WASAPI device id string. */
    std::vector<AudioDeviceInfo> getDevices() const override;

    /**
        Selects the physical OUTPUT device. The capture SOURCE is selected
        separately with setSourceDevice(); empty id = system default output.
    */
    bool setActiveDevice (const std::string& deviceId) override;

    /** Chooses where audio is captured FROM. Empty = follow system default. */
    void setSourceDevice (const std::string& deviceId);

    void setOutputSink (AudioOutputSink* sink) override;
    void setProcessStage (ProcessStage stage) override;
    AudioStreamConfig getStreamConfig() const override;

    // -- M8 extras -------------------------------------------------------------

    struct Status
    {
        enum class Code { ok, notImplemented, noDevice, feedbackLoop,
                          multichannelUnsupported, deviceLost, comFailure, genericError };

        Code code = Code::ok;
        std::string message;      // human-readable; never raw HRESULT text alone
    };
    Status getStatus() const;

    /** Approximate one-way latency in ms; negative when unknown (#27: never fake). */
    double getLatencyMs() const;

    /** Underrun/deadline-miss counters since start (#30) — diagnostics only. */
    unsigned long long getUnderruns() const;

    /** True when Windows reports the default render endpoint changed. */
    bool defaultDeviceChangedSinceLastCheck();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace otoha::platform
