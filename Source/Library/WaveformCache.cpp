#include "WaveformCache.h"

namespace
{
constexpr juce::uint32 cacheMagic = 0x4F574631u; // 'OWF1' (host byte order; local-only cache)

juce::File cacheFileFor (const juce::File& dir, juce::int64 itemId)
{
    return dir.getChildFile ("wave-" + juce::String (itemId) + ".owf");
}

bool readCache (const juce::File& f, juce::int64 sourceMtimeMs,
                int expectedBuckets, std::vector<float>& peaksOut)
{
    juce::MemoryBlock data;
    if (! f.existsAsFile() || ! f.loadFileAsData (data))
        return false;

    const size_t headerBytes = sizeof (juce::uint32) + sizeof (int) + sizeof (juce::int64);
    if (data.getSize() < (juce::uint64) headerBytes)
        return false;

    const auto* bytes = static_cast<const char*> (data.getData());

    juce::uint32 magic = 0;
    std::memcpy (&magic, bytes, sizeof (magic));
    if (magic != cacheMagic)
        return false;

    int buckets = 0;
    std::memcpy (&buckets, bytes + sizeof (magic), sizeof (buckets));

    juce::int64 mtime = 0;
    std::memcpy (&mtime, bytes + sizeof (magic) + sizeof (buckets), sizeof (mtime));

    if (buckets != expectedBuckets || mtime != sourceMtimeMs
        || data.getSize() != (juce::uint64) (headerBytes + (size_t) expectedBuckets * sizeof (float)))
        return false;

    peaksOut.resize ((size_t) expectedBuckets);
    std::memcpy (peaksOut.data(), bytes + headerBytes, (size_t) expectedBuckets * sizeof (float));
    return true;
}
} // namespace

// =============================================================================
WaveformCache::WaveformCache (const juce::File& cacheDirectory)
    : cacheDirectory (cacheDirectory)
{
    cacheDirectory.createDirectory();
    formatManager.registerBasicFormats();
}

WaveformCache::~WaveformCache()
{
    cancelPendingJobs();
}

void WaveformCache::cancelPendingJobs()
{
    pool.removeAllJobs (true, 5000);
    const juce::ScopedLock sl (pendingLock);
    pendingIds.clear();
}

int WaveformCache::pendingCount() const
{
    return pool.getNumJobs();
}

bool WaveformCache::getPeaks (const otoha::MediaItem& item, std::vector<float>& peaksOut)
{
    if (item.type != otoha::MediaType::audio || ! item.file.existsAsFile())
        return false;

    const auto mtime = item.file.getLastModificationTime().toMilliseconds();

    if (readCache (cacheFileFor (cacheDirectory, item.id), mtime, numBuckets, peaksOut))
        return true;

    requestGeneration (item);
    return false;
}

void WaveformCache::requestGeneration (const otoha::MediaItem& item)
{
    {
        const juce::ScopedLock sl (pendingLock);
        if (! pendingIds.insert (item.id).second)
            return;   // already queued
    }

    pool.addJob ([this, item] { generate (item); });
}

void WaveformCache::generate (otoha::MediaItem item)
{
    struct ScopedRemove
    {
        explicit ScopedRemove (WaveformCache& c, otoha::MediaItem i) : cache (c), id (i.id) {}
        ~ScopedRemove() { const juce::ScopedLock sl (cache.pendingLock); cache.pendingIds.erase (id); }
        WaveformCache& cache;
        juce::int64 id;
    } scopedRemove { *this, item };

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (item.file));
    if (reader == nullptr || reader->sampleRate <= 0 || reader->lengthInSamples <= 0)
        return;   // corrupted/unreadable — row keeps its placeholder, nothing crashes

    // Peak aggregation: a fixed bucket count no matter how long the recording is.
    std::vector<float> magnitudes ((size_t) numBuckets, 0.0f);
    const juce::int64 totalFrames = reader->lengthInSamples;
    constexpr int framesPerChunk = 1 << 16;

    juce::AudioBuffer<int> chunk ((int) juce::jmax ((juce::int64) 1, reader->numChannels), framesPerChunk);

    for (juce::int64 pos = 0; pos < totalFrames; pos += framesPerChunk)
    {
        const int frames = (int) juce::jmin ((juce::int64) framesPerChunk, totalFrames - pos);
        if (! reader->read (&chunk, 0, frames, pos, true, true))
            return;

        for (int ch = 0; ch < chunk.getNumChannels(); ++ch)
        {
            const auto* samples = chunk.getReadPointer (ch);
            for (int i = 0; i < frames; ++i)
            {
                const size_t bucket = (size_t) juce::jlimit (0, numBuckets - 1,
                    (int) (((pos + i) * (juce::int64) numBuckets) / totalFrames));
                magnitudes[bucket] = juce::jmax (magnitudes[bucket],
                                                 std::abs ((float) samples[i] / 2147483648.0f));
            }
        }
    }

    // Write: magic, buckets, source mtime (staleness key), magnitudes.
    const auto outFile = cacheFileFor (cacheDirectory, item.id);
    juce::FileOutputStream stream (outFile);
    if (! stream.openedOk())
        return;

    const juce::uint32 magic = cacheMagic;
    const int buckets = numBuckets;
    const juce::int64 mtimeMs = item.file.getLastModificationTime().toMilliseconds();

    // Raw fixed-width writes only — the reader mirrors these exactly.
    bool ok = stream.write (&magic, sizeof (magic))
           && stream.write (&buckets, sizeof (buckets))
           && stream.write (&mtimeMs, sizeof (mtimeMs));

    for (int i = 0; ok && i < numBuckets; ++i)
        ok = stream.write (&magnitudes[(size_t) i], sizeof (float));

    stream.flush();

    if (! ok)
        outFile.deleteFile();   // never leave a half-written cache behind
}
