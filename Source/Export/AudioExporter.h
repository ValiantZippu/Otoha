#pragma once

#include <atomic>
#include <functional>
#include <memory>

#include <juce_audio_formats/juce_audio_formats.h>

#include "../Dsp/DspChain.h"
#include "../Dsp/ProcessingState.h"
#include "ExportTypes.h"
#include "../Editor/AudioDocument.h"

/*
    AudioExporter — THE export engine (single source of truth; no
    SingleRenderer/BatchRenderer splits).

        document (timeline) -> DspChain -> [resample/downmix] -> encoder -> file

    WAV/FLAC stream straight to JUCE writers. Compressed formats render to a
    unique temporary WAV first (correctness-first per the streaming fallback
    rule), then FFmpeg encodes and the result is moved atomically into place.
*/
namespace otoha
{
struct AudioExportRequest
{
    std::shared_ptr<const AudioDocument> document;
    const ProcessingState* dsp = nullptr;      // only applied when dsp->enabled
    ExportFormat format = ExportFormat::wav;
    ExportQuality quality = ExportQuality::standard;
    int sampleRateOverride = 0;                // 0 = keep source rate
    int channelOverride = 0;                   // 0 = keep source channels
    juce::String titleMetadata;
};

class AudioExporter
{
public:
    /** Blocking; background thread only. Returns false with a user-facing
        errorOut on failure/cancellation. Never touches `destination` on failure. */
    static bool exportAudio (const AudioExportRequest& request,
                             const juce::File& destination,
                             const juce::File& ffmpegExecutable,   // used when required
                             const std::atomic<bool>& cancelFlag,
                             const std::function<void (float progress01)>& progress,
                             juce::String& errorOut);
};
} // namespace otoha
