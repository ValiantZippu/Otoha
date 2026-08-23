#pragma once

#include <memory>
#include <vector>

#include "Core/OtohaDspCore.h"
#include "Core/Processors.h"
#include "ProcessingState.h"

/*
    DspChain — Studio's view of the Otoha DSP Core.

    This is a thin FACADE: it composes core processors in one EXPLICIT,
    code-defined order (never UI-driven) and preserves the pre-M7 API so
    DspPreviewSource and AudioExporter are untouched.

    Studio chain order (deterministic by design):

        NoiseReduction -> EQ -> Bass -> Clarity -> Compressor -> Limiter -> Meter

    Otoha Sound will compose the same core differently without changing any
    processor.
*/
namespace otoha
{
class DspChain
{
public:
    DspChain();

    void prepare (double sampleRate, int numChannels);
    void reset();

    /** Message thread. Publishes the full state; each processor takes its slice. */
    void setParameters (const ProcessingState& p);

    /** Real-time safe. Channels must match the prepared channel count. */
    void process (float* const* channels, int numFrames);

    bool isPrepared() const { return prepared; }

    struct Meters
    {
        float peak = 0.0f;
        float rms = 0.0f;
        float limiterReductionDb = 0.0f;
    };

    /** Safe snapshot for UI polling (atomics underneath). */
    Meters getMeters() const;

private:
    std::vector<std::unique_ptr<dsp::DspProcessor>> chain;
    dsp::MeterProcessor* meterTap = nullptr;   // non-owning; owned by `chain`
    bool prepared = false;
    int preparedChannelCount = 0;
};
} // namespace otoha
