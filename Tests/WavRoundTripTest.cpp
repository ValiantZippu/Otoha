/*
    WavRoundTripTest — headless sanity test for the core audio path:
    write a known sine wave as 24-bit WAV, read it back, verify everything.
*/
#include <juce_audio_formats/juce_audio_formats.h>

#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
bool expect (bool condition, const char* message)
{
    if (! condition)
        std::printf ("FAIL: %s\n", message);
    return condition;
}
} // namespace

int main()
{
    juce::WavAudioFormat wavFormat;

    const auto file = juce::File::createTempFile ("otoha_roundtrip.wav");

    constexpr double sampleRate = 48000.0;
    constexpr int numChannels = 2;
    constexpr int numSamples = 48000; // one second
    constexpr int bitDepth = 24;
    constexpr float frequencyHz = 440.0f;

    // --- write ---------------------------------------------------------------
    {
        auto stream = file.createOutputStream();
        if (! expect (stream != nullptr, "could not open temp file for writing"))
            return 1;

        std::unique_ptr<juce::AudioFormatWriter> writer (
            wavFormat.createWriterFor (stream.release(),
                                       sampleRate,
                                       numChannels,
                                       bitDepth,
                                       {}, 0));
        if (! expect (writer != nullptr, "could not create WAV writer"))
            return 1;

        juce::AudioBuffer<float> buffer (numChannels, numSamples);
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                buffer.setSample (ch, i, 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                           * frequencyHz * (float) i / (float) sampleRate));

        if (! expect (writer->writeFromAudioSampleBuffer (buffer, 0, numSamples),
                      "writer rejected the sample buffer"))
            return 1;

        writer.reset(); // finalises the header
    }

    // --- raw RIFF/WAV structure ---------------------------------------------
    {
        juce::MemoryBlock contents;
        file.loadFileAsData (contents);
        const auto* bytes = (const unsigned char*) contents.getData();
        const size_t size = contents.getSize();

        auto tagIs = [&] (size_t offset, const char* tag)
        {
            return offset + 4 <= size
                && std::memcmp (bytes + offset, tag, 4) == 0;
        };
        // data payload for 1 s of 24-bit stereo:
        constexpr juce::uint32 expectedDataBytes = numSamples * numChannels * (bitDepth / 8);

        bool ok = true;
        ok &= expect (tagIs (0, "RIFF"), "missing RIFF header");
        ok &= expect (tagIs (8, "WAVE"), "missing WAVE form type");
        ok &= expect (tagIs (12, "fmt "), "missing fmt chunk");

        // Walk chunks to find 'data' and validate its declared size.
        bool foundData = false;
        for (size_t pos = 12; pos + 8 <= size; )
        {
            const juce::uint32 chunkSize = (juce::uint32) bytes[pos + 4] | ((juce::uint32) bytes[pos + 5] << 8)
                                         | ((juce::uint32) bytes[pos + 6] << 16) | ((juce::uint32) bytes[pos + 7] << 24);
            if (tagIs (pos, "data"))
            {
                foundData = true;
                ok &= expect (chunkSize == expectedDataBytes,
                              "data chunk size does not match duration/bit depth/channels");
                break;
            }
            pos += 8u + chunkSize + (chunkSize & 1u);
        }
        ok &= expect (foundData, "missing data chunk");

        if (! ok) return 1;
    }

    // --- read back -----------------------------------------------------------
    {
        // AudioFormat::createReaderFor takes a raw stream + delete-on-fail flag.
        juce::AudioFormatReader* rawReader
            = wavFormat.createReaderFor (file.createInputStream().release(), true);
        if (! expect (rawReader != nullptr, "WAV could not be re-opened"))
            return 1;
        std::unique_ptr<juce::AudioFormatReader> reader (rawReader);

        bool ok = true;
        ok &= expect (reader->sampleRate == sampleRate, "sample rate mismatch");
        ok &= expect ((int) reader->numChannels == numChannels, "channel count mismatch");
        ok &= expect (reader->lengthInSamples == numSamples, "sample count mismatch");
        ok &= expect ((int) reader->bitsPerSample == bitDepth, "bit depth not preserved");

        if (! ok)
            return 1;

        juce::AudioBuffer<float> buffer (numChannels, (int) reader->lengthInSamples);
        reader->read (&buffer, 0, (int) reader->lengthInSamples, 0, true, true);

        float maxError = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                maxError = std::max (maxError,
                                     std::abs (buffer.getSample (ch, i)
                                               - 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                                   * frequencyHz * (float) i / (float) sampleRate)));

        // 24-bit quantisation leaves plenty of headroom under this tolerance.
        if (! expect (maxError < 0.001f, "round-tripped samples deviate too much"))
            return 1;
    }

    file.deleteFile();
    std::printf ("PASS: wav round trip (%d-bit, %.0f kHz)\n", bitDepth, sampleRate / 1000.0);
    return 0;
}
