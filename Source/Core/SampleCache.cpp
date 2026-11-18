#include "SampleCache.h"

namespace foundry
{

SampleCache::Entry* SampleCache::findOrLoad (const juce::File& file, juce::String& error)
{
    const auto key = file.getFullPathName();
    const auto modified = file.getLastModificationTime();

    auto it = entries.find (key);
    if (it != entries.end() && it->second.modified == modified)
    {
        it->second.lastUsed = ++useCounter;
        return &it->second;
    }

    juce::AudioBuffer<float> raw;
    double sr = 0.0;
    if (! io.loadWav (file, raw, sr, error, kMaxSeconds))
        return nullptr;

    auto source = std::make_shared<SourceSample>();
    source->audio = AudioIO::toStereo (raw);
    source->sampleRate = sr;
    source->fileSeconds = source->seconds();

    // The decode may have been capped; ask the reader for the real length so
    // the UI can say "this is a long file" rather than pretend it ends at 30 s.
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        if (std::unique_ptr<juce::AudioFormatReader> reader { fm.createReaderFor (file) })
            if (reader->sampleRate > 0.0)
                source->fileSeconds = (double) reader->lengthInSamples / reader->sampleRate;
    }

    Entry e;
    e.source = std::move (source);
    e.modified = modified;
    e.lastUsed = ++useCounter;

    entries[key] = std::move (e);
    evictIfNeeded();

    auto found = entries.find (key);
    return found != entries.end() ? &found->second : nullptr;
}

void SampleCache::evictIfNeeded()
{
    while (entries.size() > kMaxEntries)
    {
        auto oldest = entries.begin();
        for (auto it = entries.begin(); it != entries.end(); ++it)
            if (it->second.lastUsed < oldest->second.lastUsed)
                oldest = it;

        // Only the cache's own reference goes; a snapshot still playing the
        // sample keeps its shared buffer alive.
        entries.erase (oldest);
    }
}

std::shared_ptr<const SourceSample> SampleCache::get (const juce::File& file, juce::String& error)
{
    const juce::ScopedLock sl (lock);
    if (auto* e = findOrLoad (file, error))
        return e->source;
    return {};
}

SampleBufferPtr SampleCache::getAtRate (const juce::File& file, double sampleRate, juce::String& error)
{
    const juce::ScopedLock sl (lock);
    auto* e = findOrLoad (file, error);
    if (e == nullptr)
        return {};

    const int key = juce::roundToInt (sampleRate);
    if (auto it = e->resampled.find (key); it != e->resampled.end())
        return it->second;

    auto buffer = std::make_shared<juce::AudioBuffer<float>> (
        AudioIO::resample (e->source->audio, e->source->sampleRate, sampleRate));
    e->resampled[key] = buffer;
    return buffer;
}

void SampleCache::clear()
{
    const juce::ScopedLock sl (lock);
    entries.clear();
}

} // namespace foundry
