/*
    QaStressTests — Milestone 15: deliberately try to break Otoha, headlessly.

    Covers:
      #5-#9   timeline invariants, property-based random edit sequences,
              1000-op undo/redo stress, undo branching
      #10-#12 buffer sizes 1..4096, sample rates 8k..96k, mono/stereo
      #13-#16 silence / impulse / sine behaviour through the full chain
      #17     extreme parameter boundaries (finite output or bust)
      #18     DSP state reset isolation between "recordings"
      #60/#61 unicode project paths/titles, damaged project payloads
*/
#include "../Source/Dsp/DspChain.h"
#include "../Source/Editor/AudioDocument.h"
#include "../Source/Editor/ProjectFormat.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <random>

namespace
{
bool expect (bool condition, const char* message)
{
    if (! condition)
        std::printf ("FAIL: %s\n", message);
    return condition;
}

// --- #6 timeline invariants -------------------------------------------------
/** Every legal AudioDocument state must satisfy these, after EVERY op. */
bool invariantsHold (const otoha::AudioDocument& doc)
{
    juce::int64 consumed = 0;
    for (const auto& c : doc.getClips())
    {
        if (c.length <= 0)                       return false;   // no negative/zero clips
        if (c.sourceStart < 0)                   return false;   // no invalid offsets
        consumed += c.length;
        if (consumed < 0)                        return false;   // overflow
    }
    if (consumed != doc.totalSamples())          return false;   // contiguous timeline
    if (! doc.getClips().empty() && consumed == 0) return false;  // non-empty but silent
    const auto sel = doc.getSelection();
    if (sel.start < 0 || sel.end > doc.totalSamples()) return false; // impossible selection
    return true;
}

void fillSine (juce::AudioBuffer<float>& b, double rate, float amp = 0.25f)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            b.setSample (ch, i, amp * std::sin (2.0 * juce::MathConstants<double>::pi
                                                * 440.0 * i / rate));
}

template <int N>
bool allFinite (float* const* chans, int channels)
{
    for (int ch = 0; ch < channels; ++ch)
        for (int i = 0; i < N; ++i)
            if (! std::isfinite (chans[ch][i]))
                return false;
    return true;
}

bool allFiniteUpTo (float* const* chans, int channels, int count)
{
    for (int ch = 0; ch < channels; ++ch)
        for (int i = 0; i < count; ++i)
            if (! std::isfinite (chans[ch][i]))
                return false;
    return true;
}
} // namespace

int main()
{
    bool ok = true;

    // ========================================================================
    // #10/#11/#12 — buffer sizes × sample rates × channels
    // ========================================================================
    {
        const int sizes[]  = { 1, 2, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096 };
        const double rates[] = { 8000.0, 16000.0, 22050.0, 44100.0, 48000.0, 88200.0, 96000.0 };

        for (double rate : rates)
        {
            for (int channels = 1; channels <= 2; ++channels)
            {
                otoha::DspChain chain;
                chain.prepare (rate, channels);
                chain.setParameters ({});

                for (int size : sizes)
                {
                    juce::AudioBuffer<float> buf (channels, size);
                    fillSine (buf, rate);
                    float* chans[2] = { buf.getWritePointer (0),
                                        channels > 1 ? buf.getWritePointer (1) : nullptr };
                    // mono must not touch channel 1
                    chain.process (chans, size);
                    if (! allFiniteUpTo (chans, channels, size))
                    {
                        ok = expect (false, "non-finite DSP output in matrix");
                        break;
                    }
                }
                if (! ok) break;
            }
            if (! ok) break;
        }
        ok = expect (ok, "DSP matrix (rates × sizes × channels) produced finite audio");
    }

    // ========================================================================
    // #13 — silence stays silence
    // ========================================================================
    {
        otoha::DspChain chain;
        chain.prepare (48000.0, 2);
        auto st = otoha::ProcessingState{};
        st.enabled = true;                       // full chain active
        st.bassAmount = 0.7f;
        st.clarityAmount = 0.7f;
        chain.setParameters (st);

        juce::AudioBuffer<float> quiet (2, 512); // -80 dBFS noise floor, NOT digital zero:
        for (int ch = 0; ch < 2; ++ch)           // expanders may gate true silence anyway,
            for (int i = 0; i < 512; ++i)        // so assert no DC/noise AMPLIFICATION.
                quiet.setSample (ch, i, (i % 2 == 0 ? 1e-4f : -1e-4f));

        float* chans[2] = { quiet.getWritePointer (0), quiet.getWritePointer (1) };
        chain.process (chans, 512);

        double peak = 0.0;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
                peak = juce::jmax (peak, (double) std::abs (chans[ch][i]));
        ok &= expect (peak < 0.05f, "quiet input is not amplified into audible noise");
    }

    // ========================================================================
    // #14/#15 — impulse and sine sanity
    // ========================================================================
    {
        otoha::DspChain chain;
        chain.prepare (48000.0, 1);
        chain.setParameters ({});

        float impulse[64] = {};
        impulse[0] = 1.0f;
        float* chans[1] = { impulse };
        chain.process (chans, 64);
        ok &= expect (allFinite<64> (chans, 1), "impulse yields finite output");

        float peakAfter = 0.0f;
        for (float s : impulse) peakAfter = std::max (peakAfter, std::abs (s));
        ok &= expect (peakAfter <= 1.0f + 1e-3f, "impulse does not explode the limiter");
    }
    {
        otoha::DspChain chain;
        chain.prepare (48000.0, 1);
        auto st = otoha::ProcessingState{};
        st.inputGainDb = 6.0f;                   // known +6 dB
        chain.setParameters (st);

        juce::AudioBuffer<float> sine (1, 4800);
        fillSine (sine, 48000.0);
        const float inRms = sine.getRMSLevel (0, 0, 4800);

        float* chans[1] = { sine.getWritePointer (0) };
        chain.process (chans, 4800);
        const float outRms = sine.getRMSLevel (0, 0, 4800);

        const float expectedGain = std::pow (10.0f, 6.0f / 20.0f);
        const float measured     = outRms / inRms;
        ok &= expect (std::abs (measured - expectedGain) / expectedGain < 0.35f,
                      "+6 dB input gain measures within tolerance of the ideal");
    }

    // ========================================================================
    // #17 — extreme parameters stay finite (the NaN guard backstops this)
    // ========================================================================
    {
        otoha::DspChain chain;
        chain.prepare (44100.0, 2);
        auto extreme = otoha::ProcessingState{};
        extreme.enabled = true;
        for (float& g : extreme.eq.gainsDb) g = 24.0f;
        extreme.compressor.enabled = true;
        extreme.compressor.thresholdDb = -60.0f;
        extreme.compressor.ratio = 20.0f;
        extreme.compressor.makeupGainDb = 24.0f;
        extreme.limiter.ceilingDb = 0.0f;
        extreme.noiseReduction.strength = 1.0f;
        extreme.bassAmount = 1.0f;
        extreme.clarityAmount = 1.0f;
        extreme.stereoWidth = 1.0f;
        extreme.inputGainDb = 24.0f;
        extreme.outputGainDb = 12.0f;
        chain.setParameters (extreme);

        juce::AudioBuffer<float> hot (2, 2048);
        fillSine (hot, 44100.0, 0.98f);
        float* chans[2] = { hot.getWritePointer (0), hot.getWritePointer (1) };
        chain.process (chans, 2048);
        ok &= expect (allFinite<2048> (chans, 2),
                      "extreme parameters produce finite output");
        ok &= expect (chain.invalidSampleCount() == 0,
                      "no samples had to be rescued by the NaN guard at parameter limits");
    }

    // ========================================================================
    // #18 — reset isolation: recording B must not inherit A's envelopes
    // ========================================================================
    {
        auto loudPass = [&] (otoha::DspChain& c)
        {
            juce::AudioBuffer<float> loud (1, 4800);
            fillSine (loud, 48000.0, 0.95f);
            float* p[1] = { loud.getWritePointer (0) };
            c.process (p, 4800);
            return loud.getRMSLevel (0, 0, 4800);
        };

        otoha::DspChain reused;
        reused.prepare (48000.0, 1);
        auto st = otoha::ProcessingState{};
        st.compressor.enabled = true;
        st.compressor.makeupGainDb = 12.0f;
        reused.setParameters (st);
        loudPass (reused);                       // "recording A": pumps the compressor
        reused.reset();

        otoha::DspChain fresh;
        fresh.prepare (48000.0, 1);
        fresh.setParameters (st);

        // Feed SILENCE to both after reset: a leaked envelope would differ.
        juce::AudioBuffer<float> silence (1, 1024);   // zeros
        float* pr[1] = { silence.getWritePointer (0) };
        reused.process (pr, 1024);
        const float r = silence.getRMSLevel (0, 0, 1024);

        juce::AudioBuffer<float> silence2 (1, 1024);
        float* pf[1] = { silence2.getWritePointer (0) };
        fresh.process (pf, 1024);
        const float f = silence2.getRMSLevel (0, 0, 1024);

        ok &= expect (std::abs (r - f) < 1e-6f,
                      "reset chain behaves identically to a fresh chain on new material");
    }

    // ========================================================================
    // #7/#8/#9 — property-based timeline stress + 1000-op undo/redo
    // ========================================================================
    {
        std::mt19937 rng { 20260824 };           // deterministic: reproducible failures
        juce::AudioBuffer<float> src (1, 200000);
        for (int i = 0; i < 200000; ++i)
            src.setSample (0, i, ((i / 1000) % 2 == 0) ? 0.25f : -0.25f);

        otoha::AudioDocument doc (src, 48000.0);
        otoha::AudioClipboard clip;
        juce::String pasteError;   // reused lvalue for pasteAt's error out-param
        std::uniform_int_distribution<juce::int64> pick (0, (juce::int64) doc.totalSamples());

        const int totalOps = 1000;
        int performed = 0;
        for (int op = 0; op < totalOps; ++op)
        {
            switch (rng() % 6)
            {
                case 0:  // delete random range
                {
                    auto a = pick (rng), b = pick (rng);
                    doc.rippleDelete (std::min (a, b), std::abs (a - b) % 5000);
                    ++performed;
                    break;
                }
                case 1:  // paste at playhead-ish position (refill clipboard occasionally)
                {
                    if (clip.isEmpty())
                        doc.copySelectedRange (clip);
                    if (clip.isEmpty())
                    {
                        doc.setSelection (pick (rng) / 2, pick (rng));
                        doc.copySelectedRange (clip);
                    }
                    doc.pasteAt (pick (rng) % (doc.totalSamples() + 1), clip, pasteError);
                    ++performed;
                    break;
                }
                case 2:  // trim to random selection
                    doc.setSelection (pick (rng) % 100000, pick (rng));
                    doc.trimToSelection();
                    ++performed;
                    break;
                case 3:  doc.undo();  break;
                case 4:  doc.redo();  break;
                default: // selection churn only
                    doc.setSelection (pick (rng), pick (rng));
                    break;
            }
            if (! invariantsHold (doc))
            {
                ok = expect (false, "timeline invariant violated during fuzzing");
                break;
            }
        }
        ok &= expect (performed > 500, "fuzzer actually edited (not all ops were no-ops)");
        ok &= expect (doc.totalSamples() > 0, "timeline never became empty during fuzzing");
    }

    // Undo ALL / redo ALL round trip (#8).
    {
        juce::AudioBuffer<float> src (1, 120000);
        for (int i = 0; i < 120000; ++i)
            src.setSample (0, i, (i % 2 == 0) ? 0.3f : -0.3f);
        otoha::AudioDocument doc (src, 48000.0);
        const juce::int64 original = doc.totalSamples();

        for (int i = 0; i < 1000; ++i)
            doc.rippleDelete (i * 60, 30);
        const juce::int64 edited = doc.totalSamples();

        int undos = 0;
        while (doc.canUndo()) { doc.undo(); ++undos; }
        ok &= expect (undos == 1000 && doc.totalSamples() == original,
                      "undo-all restores the exact original timeline");

        int redos = 0;
        while (doc.canRedo()) { doc.redo(); ++redos; }
        ok &= expect (redos == 1000 && doc.totalSamples() == edited,
                      "redo-all replays every edit exactly");
        ok &= expect (invariantsHold (doc), "invariants hold after full undo/redo cycle");
    }

    // Branching (#9): A, B, undo, C -> redo branch is dead.
    {
        juce::AudioBuffer<float> src (1, 9000);
        for (int i = 0; i < 9000; ++i)
            src.setSample (0, i, 0.2f);
        otoha::AudioDocument doc (src, 48000.0);
        doc.rippleDelete (0, 1000);              // A
        doc.rippleDelete (0, 1000);              // B
        doc.undo();                              // back to after-A
        doc.rippleDelete (4000, 1000);           // C branches here
        ok &= expect (! doc.canRedo(), "new edit kills the stale redo branch (#9)");
    }

    // ========================================================================
    // #61/#60 — unicode projects & damaged payloads
    // ========================================================================
    {
        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                             .getChildFile (juce::String::fromUTF8 ("\xd0\x98\xd0\xbd\xd1\x82\xd0\xb5\xd1\x80\xd0\xb2\xd1\x8c\xd1\x8e \xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e \xf0\x9f\x8e\x99\xef\xb8\x8f.otoha"));
        dir.deleteRecursively();

        juce::AudioBuffer<float> buffer (1, 8000);
        fillSine (buffer, 48000.0);
        otoha::AudioDocument original (buffer, 48000.0);
        original.rippleDelete (100, 200);

        juce::String err;
        ok &= expect (otoha::project::saveProject (original, dir,
                                                   "\xf0\x9f\x8e\x99\xef\xb8\x8f \xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e \"quote\"", err),
                      "unicode title/path project saves");
        otoha::AudioDocument loaded (juce::AudioBuffer<float> (1, 8000), 48000.0);
        ok &= expect (otoha::project::loadProject (dir, loaded, err)
                          && loaded.totalSamples() == original.totalSamples(),
                      "unicode project loads with identical timeline");
        dir.deleteRecursively();
    }

    // Damaged payload variants (#60): envelope valid, document broken.
    {
        auto makeDoc = [] (int n)
        {
            juce::AudioBuffer<float> b (1, n);
            fillSine (b, 48000.0);
            return otoha::AudioDocument (b, 48000.0);
        };

        // clips referencing beyond the source length
        {
            auto* root = new juce::DynamicObject();
            root->setProperty ("formatVersion", otoha::project::currentFormatVersion);
            auto* docObj = new juce::DynamicObject();
            juce::Array<juce::var> clips;
            auto* badClip = new juce::DynamicObject();
            badClip->setProperty ("s", 999999.0);
            badClip->setProperty ("l", 500.0);
            clips.add (juce::var (badClip));
            docObj->setProperty ("clips", clips);
            root->setProperty ("document", juce::var (docObj));

            auto victim = makeDoc (8000);
            juce::String e2;
            ok &= expect (! otoha::project::applyProjectJSON (victim, juce::var (root), e2)
                              && ! e2.isEmpty(),
                          "out-of-range clips are refused with an explanation");
        }

        // document payload missing entirely
        {
            auto* root = new juce::DynamicObject();
            root->setProperty ("formatVersion", otoha::project::currentFormatVersion);
            auto victim = makeDoc (8000);
            juce::String e3;
            ok &= expect (! otoha::project::applyProjectJSON (victim, juce::var (root), e3),
                          "missing document payload refused");
        }
    }

    if (ok) std::printf ("PASS: QA stress suite\n");
    return ok ? 0 : 1;
}
