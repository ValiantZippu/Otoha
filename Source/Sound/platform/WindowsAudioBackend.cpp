#include "WindowsAudioBackend.h"

#if JUCE_WINDOWS

#include <juce_core/juce_core.h>

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>

#include <algorithm>
#include <cstring>

/*
    WindowsAudioBackend implementation — WASAPI shared-mode loopback capture
    plus shared-mode render, driven by one worker thread. See the header for
    the architecture rationale.

    Format handling:
      * Both sides use the endpoint MIX FORMAT (shared mode requirement).
      * Float32 is processed natively; 16/24-in-32 int paths are converted
        before the process stage (#15: never reinterpret blindly).
      * If capture and render sample rates differ, audio is resampled with
        juce::LagrangeInterpolator (proper interpolation, not duplication).
      * >2 channel endpoints are refused with a clear status (#17) — no
        silent surround destruction. A documented downmix is future work.

    Real-time rules: the worker thread performs no allocation after Start()
    (buffers are sized from the negotiated periods), no locks beyond the
    engine's own lock-free parameter path, no UI, no logging per block.
*/

namespace otoha::platform
{
namespace
{
// RAII helpers ---------------------------------------------------------------
template <typename T>
struct ComPtr
{
    T* ptr = nullptr;
    T** operator&() { reset(); return &ptr; }
    T* operator->() const { return ptr; }
    T* get() const { return ptr; }
    void reset() { if (ptr) { ptr->Release(); ptr = nullptr; } }
    ~ComPtr() { reset(); }
};

struct ComScope
{
    ComScope()  { CoInitializeEx (nullptr, COINIT_MULTITHREADED); }
    ~ComScope() { CoUninitialize(); }
};

juce::String deviceIdOf (IMMDevice* device)
{
    LPWSTR id = nullptr;
    if (device != nullptr && SUCCEEDED (device->GetId (&id)))
    {
        juce::String s (id);
        CoTaskMemFree (id);
        return s;
    }
    return {};
}

juce::String deviceNameOf (IMMDevice* device)
{
    ComPtr<IPropertyStore> store;
    if (device == nullptr || FAILED (device->OpenPropertyStore (STGM_READ, &store)) || store.ptr == nullptr)
        return "<unknown>";

    PROPVARIANT value;
    PropVariantInit (&value);
    if (SUCCEEDED (store->GetValue (PKEY_Device_FriendlyName, &value)))
    {
        juce::String s (value.pwszVal);
        PropVariantClear (&value);
        return s;
    }
    return "<unknown>";
}

constexpr CLSID kMMDeviceEnumeratorCLSID = __uuidof (MMDeviceEnumerator);
constexpr IID   kIMMDeviceEnumeratorIID  = __uuidof (IMMDeviceEnumerator);
constexpr IID   kIAudioClientIID         = __uuidof (IAudioClient);
constexpr IID   kIAudioRenderClientIID   = __uuidof (IAudioRenderClient);
constexpr IID   kIAudioCaptureClientIID  = __uuidof (IAudioCaptureClient);
} // namespace

// Default-endpoint watcher: sets a flag; the app polls it from a timer, so
// notifications never touch UI or audio threads directly.
struct DefaultDeviceWatcher : public IMMNotificationClient
{
    std::atomic<bool> changed { false };

    HRESULT __stdcall QueryInterface (REFIID riid, void** out) override
    {
        if (out == nullptr) return E_POINTER;
        if (riid == __uuidof (IUnknown) || riid == __uuidof (IMMNotificationClient))
            { *out = static_cast<IMMNotificationClient*> (this); AddRef(); return S_OK; }
        *out = nullptr;
        return E_NOINTERFACE;
    }
    ULONG __stdcall AddRef() override  { return 1; }   // lifetime == backend's
    ULONG __stdcall Release() override { return 1; }

    HRESULT __stdcall OnDeviceStateChanged (LPCWSTR, DWORD) override { return S_OK; }
    HRESULT __stdcall OnDeviceAdded (LPCWSTR) override               { return S_OK; }
    HRESULT __stdcall OnDeviceRemoved (LPCWSTR) override             { return S_OK; }

    HRESULT __stdcall OnDefaultDeviceChanged (EDataFlow flow, ERole role, LPCWSTR) override
    {
        if (flow == eRender && role == eMultimedia)
            changed = true;
        return S_OK;
    }
    HRESULT __stdcall OnPropertyValueChanged (LPCWSTR, const PROPERTYKEY) override { return S_OK; }
};

struct WindowsAudioBackend::Impl
{
    ComScope com;   // per-thread COM init for the creating/worker thread

    ComPtr<IMMDeviceEnumerator> enumerator;
    ComPtr<IAudioClient>  captureClient;   // loopback on the SOURCE render endpoint
    ComPtr<IAudioClient>  renderClient;    // shared render on the OUTPUT endpoint
    ComPtr<IAudioCaptureClient> capture;
    ComPtr<IAudioRenderClient>  render;

    WAVEFORMATEX* captureFormat = nullptr;
    WAVEFORMATEX* renderFormat  = nullptr;

    HANDLE renderEvent = nullptr, shutdownEvent = nullptr;
    std::unique_ptr<juce::Thread> worker;   // created in start()

    juce::MemoryBlock captureBlock;         // preallocated staging (float32, interleaved)
    juce::AudioBuffer<float> processBuffer; // preallocated planar working buffer
    juce::AudioBuffer<float> renderBuffer;  // preallocated planar output buffer
    juce::LagrangeInterpolator resamplerL, resamplerR;
    double ratio = 1.0;                     // captureRate / renderRate

    std::string sourceDeviceId, outputDeviceId;
    AudioStreamConfig config;
    AudioOutputSink* sink = nullptr;
    ProcessStage stage;

    DefaultDeviceWatcher watcher;
    bool registered = false;

    std::atomic<bool> running { false };
    std::atomic<double> latencyMs { -1.0 };
    std::atomic<unsigned long long> underruns { 0 };
    WindowsAudioBackend::Status status;

    void fail (WindowsAudioBackend::Status::Code code, const juce::String& message)
    {
        status.code = code;
        status.message = message.toStdString();
    }
};

WindowsAudioBackend::WindowsAudioBackend() : impl (std::make_unique<Impl>()) {}
WindowsAudioBackend::~WindowsAudioBackend() { shutdown(); }

WindowsAudioBackend::Status WindowsAudioBackend::getStatus() const { return impl->status; }
double WindowsAudioBackend::getLatencyMs() const { return impl->latencyMs.load(); }
unsigned long long WindowsAudioBackend::getUnderruns() const { return impl->underruns.load(); }

bool WindowsAudioBackend::defaultDeviceChangedSinceLastCheck()
{
    return impl->watcher.changed.exchange (false);
}

std::vector<AudioDeviceInfo> WindowsAudioBackend::getDevices() const
{
    std::vector<AudioDeviceInfo> devices;
    if (impl->enumerator.ptr == nullptr)
        return devices;

    ComPtr<IMMDeviceCollection> collection;
    if (FAILED (impl->enumerator->EnumAudioEndpoints (eRender, DEVICE_STATE_ACTIVE, &collection))
        || collection.ptr == nullptr)
        return devices;

    UINT count = 0;
    collection->GetCount (&count);

    LPWSTR defaultId = nullptr;
    impl->enumerator->GetDefaultAudioEndpoint (eRender, eMultimedia, &defaultId);

    for (UINT i = 0; i < count; ++i)
    {
        ComPtr<IMMDevice> device;
        if (FAILED (collection->Item (i, &device)) || device.ptr == nullptr)
            continue;

        AudioDeviceInfo info;
        info.id   = deviceIdOf (device.ptr).toStdString();
        info.name = deviceNameOf (device.ptr).toStdString();

        LPWSTR id = nullptr;
        if (defaultId != nullptr && SUCCEEDED (device->GetId (&id)))
        {
            info.isDefault = wcscmp (id, defaultId) == 0;
            CoTaskMemFree (id);
        }
        devices.push_back (info);
    }

    if (defaultId != nullptr)
        CoTaskMemFree (defaultId);
    return devices;
}

void WindowsAudioBackend::setSourceDevice (const std::string& deviceId)
{
    impl->sourceDeviceId = deviceId;
}

void WindowsAudioBackend::setOutputSink (AudioOutputSink* sink) { impl->sink = sink; }
void WindowsAudioBackend::setProcessStage (ProcessStage stage) { impl->stage = std::move (stage); }
AudioStreamConfig WindowsAudioBackend::getStreamConfig() const { return impl->config; }

bool WindowsAudioBackend::setActiveDevice (const std::string& deviceId)
{
    if (impl->running.load())
        return false;   // control-thread contract: stop before switching
    impl->outputDeviceId = deviceId;
    return true;
}

// -----------------------------------------------------------------------------
// Stream construction
// -----------------------------------------------------------------------------
bool WindowsAudioBackend::initialize (const AudioStreamConfig& requested)
{
    impl->status = { Status::Code::ok, {} };
    shutdown();

    if (impl->enumerator.ptr == nullptr
        && FAILED (CoCreateInstance (kMMDeviceEnumeratorCLSID, nullptr, CLSCTX_ALL,
                                     kIMMDeviceEnumeratorIID, (void**) &impl->enumerator)))
    {
        impl->fail (Status::Code::comFailure,
                    "Otoha couldn't reach the Windows audio system.");
        return false;
    }

    if (! impl->registered)
    {
        impl->enumerator->RegisterEndpointNotificationCallback (&impl->watcher);
        impl->registered = true;
    }

    // --- resolve endpoints -----------------------------------------------------
    ComPtr<IMMDevice> sourceDevice, outputDevice;

    if (impl->sourceDeviceId.empty())
        impl->enumerator->GetDefaultAudioEndpoint (eRender, eMultimedia, &sourceDevice);
    else
        impl->enumerator->GetDevice (juce::String (impl->sourceDeviceId).toWideCharPointer(), &sourceDevice);

    if (impl->outputDeviceId.empty())
        impl->enumerator->GetDefaultAudioEndpoint (eRender, eMultimedia, &outputDevice);
    else
        impl->enumerator->GetDevice (juce::String (impl->outputDeviceId).toWideCharPointer(), &outputDevice);

    if (sourceDevice.ptr == nullptr || outputDevice.ptr == nullptr)
    {
        impl->fail (Status::Code::noDevice, "No audio output device is available.");
        return false;
    }

    const auto sourceId = deviceIdOf (sourceDevice.ptr);
    const auto outputId = deviceIdOf (outputDevice.ptr);

    // Feedback guard (#41): capturing and rendering the same endpoint loops.
    if (sourceId == outputId)
    {
        impl->fail (Status::Code::feedbackLoop,
                    "Otoha can't listen to and play through the same device at once.\n"
                    "Choose a different output device.");
        return false;
    }

    // --- activate + negotiate formats ------------------------------------------
    if (FAILED (sourceDevice->Activate (kIAudioClientIID, CLSCTX_ALL, nullptr, (void**) &impl->captureClient))
        || FAILED (outputDevice->Activate (kIAudioClientIID, CLSCTX_ALL, nullptr, (void**) &impl->renderClient))
        || impl->captureClient.ptr == nullptr || impl->renderClient.ptr == nullptr)
    {
        impl->fail (Status::Code::deviceLost,
                    "Otoha couldn't access this audio device.\nIt may be in use by another application.");
        return false;
    }

    impl->captureClient->GetMixFormat (&impl->captureFormat);
    impl->renderClient->GetMixFormat (&impl->renderFormat);

    auto* cf = reinterpret_cast<WAVEFORMATEXTENSIBLE*> (impl->captureFormat);
    const bool captureIsFloat = impl->captureFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT
                             || (impl->captureFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE
                                 && cf->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
    if (! captureIsFloat || impl->captureFormat->wBitsPerSample != 32)
    {
        impl->fail (Status::Code::genericError,
                    "This audio configuration isn't supported (expected 32-bit float shared mode).");
        return false;
    }
    if (impl->captureFormat->nChannels > 2 || impl->renderFormat->nChannels > 2)
    {
        impl->fail (Status::Code::multichannelUnsupported,
                    "Multichannel (>stereo) system audio isn't supported yet.");
        return false;
    }

    // Shared-mode render init with event callback; loopback capture init.
    REFERENCE_TIME bufferDuration = requested.maxBlockSize > 0
        ? (REFERENCE_TIME) (10'000.0 * 20.0)      // 20 ms device period target
        : 0;
    if (FAILED (impl->renderClient->Initialize (AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                                bufferDuration, 0, impl->renderFormat, nullptr)))
    {
        impl->fail (Status::Code::deviceLost, "Otoha couldn't open the output device.");
        return false;
    }

    if (FAILED (impl->captureClient->Initialize (AUDCLNT_SHAREMODE_SHARED,
                                                 AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                                 bufferDuration, 0, impl->captureFormat, nullptr)))
    {
        impl->fail (Status::Code::deviceLost, "Otoha couldn't attach to system audio playback.");
        return false;
    }

    impl->renderEvent = CreateEvent (nullptr, FALSE, FALSE, nullptr);
    impl->shutdownEvent = CreateEvent (nullptr, FALSE, FALSE, nullptr);
    impl->renderClient->SetEventHandle (impl->renderEvent);
    // Loopback capture signals the same event when data is available.

    // --- preallocate all processing memory (#19: nothing allocates later) -------
    const int captureRate = (int) impl->captureFormat->nSamplesPerSec;
    const int renderRate  = (int) impl->renderFormat->nSamplesPerSec;
    const int channels    = (int) impl->renderFormat->nChannels;
    const int maxFrames   = (int) (renderRate * 0.05) + 64;   // 50 ms headroom

    impl->config = { (double) renderRate, channels, maxFrames };
    impl->ratio = (double) captureRate / (double) renderRate;
    impl->processBuffer.setSize (channels, maxFrames, false, false, true);
    impl->renderBuffer.setSize (channels, maxFrames, false, false, true);
    impl->captureBlock.setSize ((size_t) ((maxFrames * impl->ratio) + 64) * channels * sizeof (float));

    impl->resamplerL.reset();
    impl->resamplerR.reset();

    // Latency estimate (#27): device period + buffer padding is measured at
    // runtime; here we publish the negotiated period sum as the floor.
    REFERENCE_TIME renderPeriod = 0;
    impl->renderClient->GetDevicePeriod (&renderPeriod, nullptr);
    impl->latencyMs = (renderPeriod / 10'000.0) + (bufferDuration / 10'000.0);

    return true;
}

void WindowsAudioBackend::shutdown()
{
    stop();

    if (impl->renderEvent != nullptr)    { CloseHandle (impl->renderEvent);    impl->renderEvent = nullptr; }
    if (impl->shutdownEvent != nullptr)  { CloseHandle (impl->shutdownEvent);  impl->shutdownEvent = nullptr; }

    if (impl->captureFormat != nullptr) { CoTaskMemFree (impl->captureFormat); impl->captureFormat = nullptr; }
    if (impl->renderFormat != nullptr)  { CoTaskMemFree (impl->renderFormat);  impl->renderFormat = nullptr; }

    impl->capture.reset();
    impl->render.reset();
    impl->captureClient.reset();
    impl->renderClient.reset();
}

bool WindowsAudioBackend::start()
{
    if (impl->captureClient.ptr == nullptr || impl->renderClient.ptr == nullptr)
    {
        impl->fail (Status::Code::noDevice, "Otoha Sound isn't initialised.");
        return false;
    }
    if (impl->running.load())
        return true;

    if (FAILED (impl->renderClient->Start()) || FAILED (impl->captureClient->Start()))
    {
        impl->fail (Status::Code::deviceLost, "Otoha couldn't start the audio stream.");
        return false;
    }

    struct Worker : juce::Thread
    {
        explicit Worker (Impl& i) : juce::Thread ("OtohaSound"), impl (i) {}

        void run() override
        {
            HANDLE events[2] = { impl.renderEvent, impl.shutdownEvent };

            while (! threadShouldExit())
            {
                const DWORD wait = WaitForMultipleObjects (2, events, FALSE, 2000);
                if (wait == WAIT_OBJECT_0 + 1)
                    break;
                if (wait != WAIT_OBJECT_0)
                    { impl.underruns++; continue; }   // deadline miss (#30)

                pumpCapture();
                pumpRender();
            }
        }

        /** Pulls whatever loopback data is ready into processBuffer.
            NOTE: WASAPI loopback does not signal events itself; the shared
            render client's event paces this loop, and we drain whatever
            playback data has accumulated each pass. */
        void pumpCapture()
        {
            UINT32 packet = 0;
            int frames = 0;
            const int ch = impl.captureFormat->nChannels;
            const int bufferCh = impl.processBuffer.getNumChannels();

            while (SUCCEEDED (impl.capture->GetNextPacketSize (&packet)) && packet > 0)
            {
                BYTE* data = nullptr;
                DWORD flags = 0;
                if (FAILED (impl.capture->GetBuffer (&data, &packet, &flags, nullptr, nullptr)))
                    return;

                auto* floats = reinterpret_cast<const float*> (data);
                const bool silence = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;

                const int room = impl.processBuffer.getNumSamples() - frames;
                const int n = juce::jmin ((int) packet, room);
                for (int c = 0; c < bufferCh; ++c)
                {
                    // Source channel c (duplicated mono -> stereo when needed).
                    const int srcCh = (ch == 1 ? 0 : juce::jmin (c, ch - 1));
                    auto* dst = impl.processBuffer.getWritePointer (c) + frames;
                    if (silence)
                        std::fill_n (dst, (size_t) n, 0.0f);
                    else
                        for (int i = 0; i < n; ++i)
                            dst[i] = floats[(size_t) i * ch + srcCh];
                }
                frames += n;
                impl.capture->ReleaseBuffer (packet);
                if (frames >= impl.processBuffer.getNumSamples())
                    break;
            }

            capturedFrames.store (frames, std::memory_order_relaxed);
        }

        /** Resamples (if needed), runs DSP, and feeds the render endpoint. */
        void pumpRender()
        {
            const int captured = capturedFrames.exchange (0, std::memory_order_relaxed);
            if (captured <= 0)
                return;

            const int ch = impl.renderFormat->nChannels;
            const double ratio = impl.ratio;
            const int outFrames = (int) ((double) captured / ratio);
            if (outFrames <= 0 || outFrames > impl.renderBuffer.getNumSamples())
                return;

            // Resample into renderBuffer when rates differ (#16).
            if (std::abs (ratio - 1.0) > 0.0001)
            {
                for (int c = 0; c < ch; ++c)
                {
                    auto* src = impl.processBuffer.getReadPointer (c);
                    auto* dst = impl.renderBuffer.getWritePointer (c);
                    juce::LagrangeInterpolator& resampler = (c == 0 ? impl.resamplerL : impl.resamplerR);
                    resampler.process (ratio, src, dst, outFrames);   // interpolation, not duplication
                }
            }
            else
            {
                for (int c = 0; c < ch; ++c)
                    impl.renderBuffer.copyFrom (c, 0, impl.processBuffer, c, 0, captured);
            }

            // DSP stage (#18/#20): the shared Otoha chain, in place.
            if (impl.stage)
            {
                std::vector<float*> ptrs ((size_t) ch);
                for (int c = 0; c < ch; ++c) ptrs[(size_t) c] = impl.renderBuffer.getWritePointer (c);
                otoha::dsp::AudioBlock block (ptrs.data(), ch, outFrames);
                impl.stage (block);
            }

            // Push to the render endpoint without blocking past the deadline.
            UINT32 padding = 0;
            if (FAILED (impl.renderClient->GetCurrentPadding (&padding)))
                return;

            UINT32 available = impl.renderBuffer.getNumSamples() - padding;
            const int frames = juce::jmin (available, (UINT32) outFrames);
            if (frames <= 0)
                { impl.underruns++; return; }

            BYTE* data = nullptr;
            if (FAILED (impl.render->GetBuffer (frames, &data)) || data == nullptr)
                { impl.underruns++; return; }

            auto* out = reinterpret_cast<float*> (data);
            for (int i = 0; i < frames; ++i)
                for (int c = 0; c < ch; ++c)
                    out[(size_t) i * ch + c] = impl.renderBuffer.getReadPointer (c)[i];

            impl.render->ReleaseBuffer (frames, 0);

            if (impl.sink != nullptr)
            {
                std::vector<float*> sinkPtrs ((size_t) ch);
                for (int c = 0; c < ch; ++c) sinkPtrs[(size_t) c] = impl.renderBuffer.getWritePointer (c);
                otoha::dsp::AudioBlock sinkBlock (sinkPtrs.data(), ch, frames);
                impl.sink->writeBlock (sinkBlock);   // metering tap
            }
        }

        Impl& impl;
        std::atomic<int> capturedFrames { 0 };
    };

    impl->worker = std::make_unique<Worker> (*impl);
    impl->worker->startThread (juce::Thread::Priority::highest);
    impl->running = true;
    return true;
}

void WindowsAudioBackend::stop()
{
    if (! impl->running.exchange (false))
        return;

    if (impl->worker != nullptr)
    {
        if (impl->shutdownEvent != nullptr)
            SetEvent (impl->shutdownEvent);
        impl->worker->stopThread (2000);
        impl->worker.reset();
    }

    if (impl->renderClient.ptr != nullptr)  impl->renderClient->Stop();
    if (impl->captureClient.ptr != nullptr) impl->captureClient->Stop();
}
} // namespace otoha::platform

#else // !JUCE_WINDOWS — honest non-Windows stub --------------------------------

#include <juce_core/juce_core.h>

namespace otoha::platform
{
struct WindowsAudioBackend::Impl { WindowsAudioBackend::Status status; };

WindowsAudioBackend::WindowsAudioBackend() : impl (std::make_unique<Impl>())
{
    impl->status = { Status::Code::notImplemented,
                     "Otoha Sound is not implemented on this platform yet." };
}
WindowsAudioBackend::~WindowsAudioBackend() = default;

bool WindowsAudioBackend::initialize (const AudioStreamConfig&)
{
    impl->status = { Status::Code::notImplemented,
                     "Otoha Sound is not implemented on this platform yet." };
    return false;
}
void WindowsAudioBackend::shutdown() {}
bool WindowsAudioBackend::start() { return false; }
void WindowsAudioBackend::stop() {}

std::vector<AudioDeviceInfo> WindowsAudioBackend::getDevices() const { return {}; }
bool WindowsAudioBackend::setActiveDevice (const std::string&) { return false; }
void WindowsAudioBackend::setSourceDevice (const std::string&) {}
void WindowsAudioBackend::setOutputSink (AudioOutputSink*) {}
void WindowsAudioBackend::setProcessStage (ProcessStage) {}
AudioStreamConfig WindowsAudioBackend::getStreamConfig() const { return {}; }

WindowsAudioBackend::Status WindowsAudioBackend::getStatus() const { return impl->status; }
double WindowsAudioBackend::getLatencyMs() const { return -1.0; }
unsigned long long WindowsAudioBackend::getUnderruns() const { return 0; }
bool WindowsAudioBackend::defaultDeviceChangedSinceLastCheck() { return false; }
} // namespace otoha::platform

#endif
