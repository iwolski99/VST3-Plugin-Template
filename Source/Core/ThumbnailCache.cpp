#include "ThumbnailCache.h"
#include <algorithm>

namespace foundry
{

namespace
{
    // Long files only get their first stretch drawn: a thumbnail is a hint at
    // the attack and shape, and reading a five-minute file to draw 48 bars
    // would stall the whole queue behind it.
    constexpr double kMaxThumbSeconds = 12.0;
}

ThumbnailCache::ThumbnailCache (LibraryDatabase& db)
    : juce::Thread ("Foundry thumbnails"), database (db)
{
    formats.registerBasicFormats();
    startThread (juce::Thread::Priority::low);
}

ThumbnailCache::~ThumbnailCache()
{
    signalThreadShouldExit();
    wake.signal();
    stopThread (4000);
}

std::shared_ptr<const ThumbnailCache::Thumb> ThumbnailCache::request (const juce::String& path)
{
    const juce::ScopedLock sl (lock);

    if (auto it = done.find (path); it != done.end())
        return it->second;

    if (queued.count (path) != 0)
    {
        // Already waiting: move it to the front of the line.
        auto pos = std::find (queue.begin(), queue.end(), path);
        if (pos != queue.end() && pos + 1 != queue.end())
        {
            queue.erase (pos);
            queue.push_back (path);
        }
        return {};
    }

    queue.push_back (path);
    queued.insert (path);

    // Scrolling fast through a big folder queues far more than will ever be
    // looked at again; drop the oldest requests, they are off screen by now.
    while (queue.size() > kMaxQueue)
    {
        queued.erase (queue.front());
        queue.erase (queue.begin());
    }

    wake.signal();
    return {};
}

void ThumbnailCache::forget (const juce::String& path)
{
    const juce::ScopedLock sl (lock);
    done.erase (path);
}

std::shared_ptr<ThumbnailCache::Thumb> ThumbnailCache::compute (const juce::String& path)
{
    auto thumb = std::make_shared<Thumb>();
    const juce::File file (path);

    if (! file.existsAsFile())
    {
        thumb->failed = true;
        return thumb;
    }

    const auto modTime = file.getLastModificationTime().toMilliseconds();
    const auto size = file.getSize();

    if (database.loadThumb (path, modTime, size, thumb->peaks, thumb->seconds)
        && (int) thumb->peaks.size() == kBins)
        return thumb;

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples <= 0)
    {
        thumb->failed = true;
        return thumb;
    }

    thumb->seconds = (double) reader->lengthInSamples / reader->sampleRate;

    const auto span = juce::jmin (reader->lengthInSamples,
                                  (juce::int64) (kMaxThumbSeconds * reader->sampleRate));
    const auto perBin = juce::jmax ((juce::int64) 1, span / kBins);

    std::vector<float> mags ((size_t) kBins, 0.0f);
    float loudest = 0.0f;

    for (int b = 0; b < kBins; ++b)
    {
        if (threadShouldExit())
            return {};

        const auto start = (juce::int64) b * perBin;
        if (start >= span)
            break;

        juce::Range<float> levels[2];
        const int chans = (int) juce::jlimit (1u, 2u, reader->numChannels);
        reader->readMaxLevels (start, (int) juce::jmin (perBin, span - start), levels, chans);

        float m = 0.0f;
        for (int c = 0; c < chans; ++c)
            m = juce::jmax (m, std::abs (levels[c].getStart()), std::abs (levels[c].getEnd()));

        mags[(size_t) b] = m;
        loudest = juce::jmax (loudest, m);
    }

    // Normalise so quiet samples still show their shape, but keep a little of
    // the real level so a whisper doesn't look identical to a full-scale hit.
    const float norm = loudest > 1.0e-5f ? 1.0f / loudest : 0.0f;
    const float levelHint = juce::jlimit (0.55f, 1.0f, 0.55f + loudest * 0.6f);

    thumb->peaks.resize ((size_t) kBins);
    for (int b = 0; b < kBins; ++b)
        thumb->peaks[(size_t) b] = (juce::uint8) juce::jlimit (0, 255,
            juce::roundToInt (std::sqrt (mags[(size_t) b] * norm) * levelHint * 255.0f));

    database.storeThumb (path, modTime, size, thumb->peaks, thumb->seconds);
    return thumb;
}

void ThumbnailCache::run()
{
    while (! threadShouldExit())
    {
        juce::String next;
        {
            const juce::ScopedLock sl (lock);
            if (! queue.empty())
            {
                next = queue.back();
                queue.pop_back();
                queued.erase (next);
            }
        }

        if (next.isEmpty())
        {
            wake.wait (500);
            continue;
        }

        auto thumb = compute (next);
        if (thumb == nullptr)
            continue;

        {
            const juce::ScopedLock sl (lock);
            if (done.size() > kMaxCached)
                done.clear();       // crude, but bounded; the database still has them
            done[next] = std::move (thumb);
        }
        ++finished;
    }
}

} // namespace foundry
