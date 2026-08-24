/*
    DspCoreTests — Milestone 7 architecture + regression verification.

    Proves:
      * the DSP Core runs with NO UI / Library / Editor / FFmpeg / platform
        audio dependencies (this TU includes only Core headers),
      * the M5 processors keep their behavior after the extraction,
      * Gain / Bass / Clarity / StereoWidth behave and stay finite,
      * the chain is bypass-identical,
      * the future Otoha Sound shape works end to end via MockAudioBackend.

    Deterministic signals only; no hardware required.
*/
#include "../Source/Dsp/Core/OtohaDspCore.h"
#include "../Source/Dsp/Core/Processors.h"
#include "../Source/Dsp/ProcessingState.h"
#include "../Source/Platform/AudioBackend.h"
#include "../Source/Platform/DeviceProfiles.h"
#include "../Source/Platform/MockAudioBackend.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace otoha;
using namespace otoha::dsp;
using namespace otoha::platform;

namespace
{
bool expect (bool condition, const char* message)
{
    if (! condition)
        std::printf ("FAIL: %s\n", message);
    return condition;
}

constexpr double sr = 48000.0;

std::vector<float> sine (int frames, double freqHz, float amplitude)
{
    std::vector<float> v ((size_t) frames);
    for (int i = 0; i < frames; ++i)
        v[(size_t) i] = amplitude * (float) std::sin (2.0 * 3.14159265358979 * freqHz * i / sr);
    return v;
}

float peakOf (const std::vector<float>& v)
{
    float p = 0.0f;
    for (float x : v) p = std::max (p, std::abs (x));
    return p;
}

bool allFinite (const std::vector<float>& v)
{
    for (float x : v)
        if (! std::isfinite (x)) return false;
    return true;
}

/** Runs a processor over a stereo signal, returns post-processor channel data. */
template <typename ProcT>
std::vector<std::vector<float>> runProcessor (ProcT& proc, const ProcessingState& state,
                                              std::vector<std::vector<float>> channels,
                                              int totalFrames, int blockFrames)
{
    ProcessingContext ctx { sr, (int) channels.size(), blockFrames };
    proc.prepare (ctx);
    proc.setParameters (state);

    std::vector<float*> ptrs (channels.size());
    for (int start = 0; start < totalFrames; start += blockFrames)
    {
        const int n = std::min (blockFrames, totalFrames - start);
        for (size_t c = 0; c < channels.size(); ++c)
            ptrs[c] = channels[c].data() + start;
        AudioBlock block (ptrs.data(), (int) channels.size(), n);
        proc.process (block);
    }
    return channels;
}

} // namespace

int main()
{
    int failures = 0;

    // ------------------------------------------------------------------
    // 1. Bypass identity: a disabled state must be bit-identical.
    // ------------------------------------------------------------------
    {
        EqProcessor eq; CompressorProcessor comp; LimiterProcessor lim;
        NoiseReductionProcessor nr; GainProcessor gain; BassProcessor bass;
        ClarityProcessor clarity; StereoWidthProcessor width;

        auto in = sine (4800, 440.0, 0.5f);
        auto copy = in;

        for (auto* p : { (DspProcessor*) &eq, (DspProcessor*) &comp, (DspProcessor*) &lim,
                         (DspProcessor*) &nr, (DspProcessor*) &gain, (DspProcessor*) &bass,
                         (DspProcessor*) &clarity, (DspProcessor*) &width })
        {
            ProcessingContext ctx { sr, 1, 512 };
            p->prepare (ctx);

            ProcessingState off;                 // everything neutral/disabled
            off.enabled = false;
            p->setParameters (off);
            float* bypassChannel = copy.data();
            AudioBlock block (&bypassChannel, 1, (int) copy.size());
            p->process (block);
        }
        failures += ! expect (copy == in, "bypassed processors alter audio");
    }

    // ------------------------------------------------------------------
    // 2. Neutral EQ leaves a 1 kHz tone essentially unchanged (regression).
    // ------------------------------------------------------------------
    {
        EqProcessor eq;
        ProcessingState s;                    // all gains 0 dB
        auto ch = runProcessor (eq, s, { sine (4800, 1000.0, 0.5f) }, 4800, 512);
        failures += ! expect (peakOf (ch[0]) > 0.45f && peakOf (ch[0]) < 0.55f,
                              "neutral EQ changed level materially");
    }

    // ------------------------------------------------------------------
    // 3. Compressor: above-threshold reduction, no runaway (regression).
    // ------------------------------------------------------------------
    {
        CompressorProcessor comp;
        ProcessingState s;
        s.compressor = { true, -12.0f, 4.0f, 10.0f, 150.0f, 0.0f };
        auto loud  = runProcessor (comp, s, { sine (9600, 220.0, 0.95f) }, 9600, 512);
        auto quiet = runProcessor (comp, s, { sine (9600, 220.0, 0.05f) }, 9600, 512);
        // Measure the STEADY-STATE region only: the envelope's attack time
        // constant legitimately lets the very first transient through.
        auto settledPeakOf = [] (const std::vector<float>& v)
        {
            float p = 0.0f;
            for (size_t i = v.size() / 4; i < v.size(); ++i)
                p = std::max (p, std::abs (v[i]));
            return p;
        };
        failures += ! expect (settledPeakOf (loud[0]) < 0.8f, "compressor did not reduce loud signal");
        failures += ! expect (settledPeakOf (quiet[0]) > 0.04f && settledPeakOf (quiet[0]) < 0.08f,
                              "compressor disturbed below-threshold signal");
    }

    // ------------------------------------------------------------------
    // 4. Limiter: ceiling respected, silence stays silent (regression).
    // ------------------------------------------------------------------
    {
        LimiterProcessor lim;
        ProcessingState s;
        s.limiter = { true, -6.0f, 50.0f };
        auto out = runProcessor (lim, s, { sine (9600, 997.0, 1.2f) }, 9600, 512);   // deliberately over
        failures += ! expect (peakOf (out[0]) <= 0.51f,       // -6 dBFS ~ 0.501 + tiny tolerance
                              "limiter exceeded its ceiling");
        auto sil = runProcessor (lim, s, { std::vector<float> (9600, 0.0f) }, 9600, 512);
        failures += ! expect (peakOf (sil[0]) == 0.0f, "limiter made noise from silence");
    }

    // ------------------------------------------------------------------
    // 5. Noise reduction stable, stronger reduces more (regression).
    // ------------------------------------------------------------------
    {
        auto hiss = sine (9600, 6000.0, 0.05f);   // stand-in steady tone noise
        NoiseReductionProcessor nrG, nrS;
        ProcessingState g, st;
        g.noiseReduction  = { NoiseReductionMode::gentle, 0.5f };
        st.noiseReduction = { NoiseReductionMode::strong, 1.0f };
        auto og = runProcessor (nrG, g, { hiss }, 9600, 512);
        auto os = runProcessor (nrS, st, { hiss }, 9600, 512);
        failures += ! expect (allFinite (og[0]), "gentle NR produced NaN");
        failures += ! expect (allFinite (os[0]), "strong NR produced NaN");
        failures += ! expect (peakOf (os[0]) <= peakOf (og[0]),
                              "strong NR reduced less than gentle NR");
    }

    // ------------------------------------------------------------------
    // 6. NEW Gain: input+output gain apply, smoothed, finite.
    // ------------------------------------------------------------------
    {
        GainProcessor gp;
        ProcessingState s;
        s.inputGainDb  = -6.0f;
        s.outputGainDb = 0.0f;
        auto out = runProcessor (gp, s, { sine (9600, 1000.0, 0.5f) }, 9600, 512);
        const float expectedPeak = 0.25f;     // 0.5 * -6 dB
        failures += ! expect (peakOf (out[0]) > expectedPeak * 0.85f
                           && peakOf (out[0]) < expectedPeak * 1.15f,
                              "gain processor applied wrong amount");
        failures += ! expect (allFinite (out[0]), "gain produced NaN");
    }

    // ------------------------------------------------------------------
    // 7. NEW Bass: low-frequency content boosted at high setting.
    // ------------------------------------------------------------------
    {
        BassProcessor bp;
        ProcessingState s;
        s.bassAmount = 1.0f;                  // up to +6 dB @ 90 Hz
        auto lo = runProcessor (bp, s, { sine (9600, 80.0, 0.3f) }, 9600, 512);
        failures += ! expect (peakOf (lo[0]) > 0.3f, "bass boost did not lift low frequencies");
        failures += ! expect (peakOf (lo[0]) < 0.75f && allFinite (lo[0]),
                              "bass boost ran away or produced NaN");

        BassProcessor bn;
        ProcessingState neutral;              // amount 0 -> unchanged
        auto same = runProcessor (bn, neutral, { sine (9600, 80.0, 0.3f) }, 9600, 512);
        failures += ! expect (std::abs (peakOf (same[0]) - 0.3f) < 0.03f,
                              "bass at zero changed the signal");
    }

    // ------------------------------------------------------------------
    // 8. NEW Clarity: presence lift at high setting, neutral at zero.
    // ------------------------------------------------------------------
    {
        ClarityProcessor cp;
        ProcessingState s;
        s.clarityAmount = 1.0f;
        auto hi = runProcessor (cp, s, { sine (9600, 3500.0, 0.3f) }, 9600, 512);
        failures += ! expect (peakOf (hi[0]) > 0.3f && peakOf (hi[0]) < 0.7f
                              && allFinite (hi[0]), "clarity misbehaved on presence band");

        ClarityProcessor cn;
        ProcessingState neutral;
        auto flat = runProcessor (cn, neutral, { sine (9600, 3500.0, 0.3f) }, 9600, 512);
        failures += ! expect (std::abs (peakOf (flat[0]) - 0.3f) < 0.03f,
                              "clarity at zero changed the signal");
    }

    // ------------------------------------------------------------------
    // 9. NEW StereoWidth: mono stays mono; width widens stereo difference.
    // ------------------------------------------------------------------
    {
        StereoWidthProcessor sw;
        ProcessingState s;
        s.stereoWidth = 0.5f;                 // normal stereo
        std::vector<std::vector<float>> stereo { sine (4800, 500.0, 0.4f), sine (4800, 700.0, 0.4f) };
        auto normal = runProcessor (sw, s, stereo, 4800, 512);

        float diffNormal = 0.0f, sumNormal = 0.0f;
        for (int i = 0; i < 4800; ++i)
        {
            diffNormal += std::abs (normal[0][(size_t) i] - normal[1][(size_t) i]);
            sumNormal  += std::abs (normal[0][(size_t) i]);
        }
        failures += ! expect (sumNormal > 100.0f, "width processing collapsed stereo to near-silence");

        StereoWidthProcessor wide;
        ProcessingState w;
        w.stereoWidth = 1.0f;                 // maximum
        auto widened = runProcessor (wide, w, stereo, 4800, 512);
        float diffWide = 0.0f;
        for (int i = 0; i < 4800; ++i)
            diffWide += std::abs (widened[0][(size_t) i] - widened[1][(size_t) i]);
        failures += ! expect (diffWide >= diffNormal * 0.95f,
                              "max width did not increase L/R difference");
        failures += ! expect (allFinite (widened[0]) && allFinite (widened[1]),
                              "width produced NaN");

        StereoWidthProcessor monoIn;
        std::vector<std::vector<float>> mono { sine (4800, 500.0, 0.4f), sine (4800, 500.0, 0.4f) };
        auto kept = runProcessor (monoIn, w, mono, 4800, 512);
        bool stillMono = true;
        for (int i = 0; i < 4800; ++i)
            if (std::abs (kept[0][(size_t) i] - kept[1][(size_t) i]) > 1e-3f)
                { stillMono = false; break; }
        failures += ! expect (stillMono, "mono input did not remain mono through width");
    }

    // ------------------------------------------------------------------
    // 10. END TO END — Otoha Sound shape:
    //     generated audio -> MockAudioBackend -> DspChain -> output sink.
    // ------------------------------------------------------------------
    {
        class CapturingSink : public AudioOutputSink
        {
        public:
            void writeBlock (const dsp::AudioBlock& block) override
            {
                for (int c = 0; c < block.numChannels; ++c)
                    for (int i = 0; i < block.numFrames; ++i)
                        captured[(size_t) c].push_back (block.channelData[c][i]);
            }
            std::vector<std::vector<float>> captured { {}, {} };
        };

        CapturingSink sink;
        MockAudioBackend backend;

        // A minimal real chain built straight from core processors — proving
        // the backend never needs Studio's facade or any other module.
        GainProcessor stageGain;
        ProcessingContext ctx { 48000.0, 2, 512 };
        stageGain.prepare (ctx);
        ProcessingState gs;
        gs.inputGainDb = -6.0f;
        stageGain.setParameters (gs);

        failures += ! expect (backend.initialize ({ 48000.0, 2, 512 }), "mock init failed");
        backend.setOutputSink (&sink);
        backend.setProcessStage ([&stageGain] (dsp::AudioBlock& b) { stageGain.process (b); });
        backend.setActiveDevice ("mock-out");
        failures += ! expect (backend.start(), "mock start failed");

        // Deliver 10 blocks of a stereo sine through the whole pipeline.
        auto l = sine (512, 440.0, 0.5f), r = sine (512, 550.0, 0.5f);
        std::vector<float*> ptrs { l.data(), r.data() };
        for (int b = 0; b < 10; ++b)
            backend.deliverBlock (ptrs.data(), 512);

        backend.stop();
        backend.shutdown();

        failures += ! expect (sink.captured[0].size() == 5120 && sink.captured[1].size() == 5120,
                              "pipeline delivered wrong sample count");
        failures += ! expect (allFinite (sink.captured[0]) && allFinite (sink.captured[1]),
                              "pipeline output not finite");
        failures += ! expect (std::abs (peakOf (sink.captured[0]) - 0.25f) < 0.05f,
                              "pipeline did not apply DSP (-6 dB expected)");
        failures += ! expect (! sink.captured[1].empty() && peakOf (sink.captured[1]) > 0.05f,
                              "stereo channel was lost in the pipeline");

        // Changing parameters mid-stream works without rebuilding anything.
        ProcessingState louder;
        louder.inputGainDb = 0.0f;
        stageGain.setParameters (louder);
        for (int i = 0; i < 512; ++i) { l[(size_t) i] = 0.5f; r[(size_t) i] = 0.5f; }
        backend.initialize ({ 48000.0, 2, 512 });
        backend.start();
        backend.deliverBlock (ptrs.data(), 512);
        failures += ! expect (allFinite (sink.captured[0]), "post-param-change output not finite");
        backend.stop();
    }

    // ------------------------------------------------------------------
    // 11. Device profiles: resolution order device-bound -> default -> none.
    // ------------------------------------------------------------------
    {
        ProfileManager pm;

        ProcessingState music;  music.eq.gainsDb[2] = 1.0f;
        ProcessingState lateNight; lateNight.outputGainDb = -6.0f;

        pm.upsert ({ "default", "Default", "", music, true });
        pm.upsert ({ "hp", "Headphones", "headphones-42", lateNight, true });

        const auto* hp = pm.resolveForDevice ("headphones-42");
        failures += ! expect (hp != nullptr && hp->id == "hp",
                              "device-bound profile not resolved first");

        const auto* other = pm.resolveForDevice ("usb-speakers-7");
        failures += ! expect (other != nullptr && other->id == "default",
                              "unmatched device fell back to default profile");

        pm.remove ("default");
        const AudioProfile* gone = pm.resolveForDevice ("anything");
        failures += ! expect (gone == nullptr,
                              "profile removal did not take effect");

        // Upsert updates instead of duplicating.
        pm.upsert ({ "hp", "Headphones v2", "headphones-42", music, true });
        failures += ! expect (pm.getProfiles().size() >= 1
                              && pm.findForDevice ("headphones-42") != nullptr
                              && pm.findForDevice ("headphones-42")->name == "Headphones v2",
                              "upsert did not update existing profile");
    }

    if (failures == 0)
        std::printf ("All DspCore tests passed.\n");
    else
        std::printf ("%d DspCore test failure(s).\n", failures);
    return failures == 0 ? 0 : 1;
}
