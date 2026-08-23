#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <functional>
#include <string>
#include <vector>

#include "../Dsp/Core/OtohaDspCore.h"

/*
    AudioBackend — Otoha Sound's platform-independent live-audio abstraction.

    The DSP Core must never know whether audio comes from WASAPI, Core Audio,
    PipeWire, ALSA, Android audio APIs, or a test harness. Everything
    platform-specific lives BEHIND this interface in platform adapters.

    Intended implementations (see docs/audio-backends.md):
        Windows : WasapiBackend      (future)
        macOS   : CoreAudioBackend   (future)
        Linux   : PipeWireBackend / AlsaBackend (future)
        Android : AndroidAudioBackend (future)
        Tests   : MockAudioBackend   (implemented now)

    Threading contract:
      * start()/stop()/setActiveDevice() are CONTROL-thread calls.
      * processBlock() is called on the PLATFORM AUDIO THREAD. Implementations
        must keep it real-time safe and must not call back into the UI.
*/

namespace otoha::platform
{
struct AudioDeviceInfo
{
    std::string id;             // stable, backend-defined identifier
    std::string name;           // human-readable ("Headphones", "Speakers")
    bool isDefault = false;
};

/** Describes a live stream as negotiated with the backend. */
struct AudioStreamConfig
{
    double sampleRate  = 48000.0;
    int    numChannels = 2;
    int    maxBlockSize = 512;
};

/**
    A sink that receives processed audio from the pipeline.

    For playback-style backends this wraps the physical output device;
    for the MockAudioBackend it simply captures blocks for inspection.
*/
class AudioOutputSink
{
public:
    virtual ~AudioOutputSink() = default;

    /** Real-time safe. Called on the audio thread. */
    virtual void writeBlock (const dsp::AudioBlock& block) = 0;
};

/**
    The backend itself: enumerates devices, negotiates a stream, and pushes
    incoming audio through a caller-supplied processing stage before it
    reaches the output sink.

    Pipeline shape (identical for every future platform adapter):

        backend input -> user processBlock() -> output sink
*/
class AudioBackend
{
public:
    virtual ~AudioBackend() = default;

    /** Control thread. Prepare resources; does not start streaming. */
    virtual bool initialize (const AudioStreamConfig& config) = 0;

    virtual void shutdown() = 0;

    virtual bool start() = 0;
    virtual void stop()  = 0;

    virtual std::vector<AudioDeviceInfo> getDevices() const = 0;

    /**
        Select the active device. May only be called while stopped unless an
        implementation explicitly documents hot-swap support. Returns false
        if the device cannot be used with the current configuration.
    */
    virtual bool setActiveDevice (const std::string& deviceId) = 0;

    /** Register where processed audio goes. Callable before start(). */
    virtual void setOutputSink (AudioOutputSink* sink) = 0;

    /**
        Supply the per-block processing stage (normally an otoha::DspChain
        wrapper). The backend calls it between capture and output; it never
        touches DSP internals itself.
    */
    using ProcessStage = std::function<void (dsp::AudioBlock&)>;
    virtual void setProcessStage (ProcessStage stage) = 0;

    virtual AudioStreamConfig getStreamConfig() const = 0;
};
} // namespace otoha::platform
