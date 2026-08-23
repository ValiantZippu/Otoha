#include "DspChain.h"

namespace otoha
{
DspChain::DspChain()
{
    // Explicit, code-defined order — UI layout must never determine this.
    chain.push_back (std::make_unique<dsp::NoiseReductionProcessor>());
    chain.push_back (std::make_unique<dsp::EqProcessor>());
    chain.push_back (std::make_unique<dsp::BassProcessor>());
    chain.push_back (std::make_unique<dsp::ClarityProcessor>());
    chain.push_back (std::make_unique<dsp::CompressorProcessor>());
    chain.push_back (std::make_unique<dsp::LimiterProcessor>());

    auto meter = std::make_unique<dsp::MeterProcessor>();
    meterTap = meter.get();
    chain.push_back (std::move (meter));
}

void DspChain::prepare (double sampleRate, int numChannels)
{
    preparedChannelCount = juce::jmax (1, juce::jmin (2, numChannels));

    const dsp::ProcessingContext context { juce::jmax (8000.0, sampleRate),
                                           preparedChannelCount,
                                           4096 };

    for (auto& processor : chain)
        processor->prepare (context);

    prepared = true;
}

void DspChain::reset()
{
    for (auto& processor : chain)
        processor->reset();
}

void DspChain::setParameters (const ProcessingState& p)
{
    for (auto& processor : chain)      // message thread only
        processor->setParameters (p);
}

void DspChain::process (float* const* channels, int numFrames)
{
    if (! prepared || numFrames <= 0 || channels == nullptr)
        return;

    dsp::AudioBlock block (channels, preparedChannelCount, numFrames);

    for (auto& processor : chain)
        processor->process (block);
}

DspChain::Meters DspChain::getMeters() const
{
    Meters m;
    if (meterTap != nullptr)
    {
        m.peak                = meterTap->meters.peak.load();
        m.rms                 = meterTap->meters.rms.load();
        m.limiterReductionDb  = meterTap->meters.limiterReductionDb.load();
    }
    return m;
}
} // namespace otoha
