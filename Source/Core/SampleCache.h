#pragma once

#include "AudioIO.h"

namespace foundry
{

/** A decoded sample as the Riff Maker sees it: stereo, at the file's own rate,
    capped at kMaxSeconds (a slot only ever plays the start of a file). */
struct SourceSample
{
    juce::AudioBuffer<float> audio;
    double sampleRate = 44100.0;
    double fileSeconds = 0.0;       // full file length, even when decoding was capped

    double seconds() const noexcept
    {
        return sampleRate > 0.0 ? (double) audio.getNumSamples() / sampleRate : 0.0;
    }
};

using SampleBufferPtr = std::shared_ptr<const juce::AudioBuffer<float>>;

/** Decodes each file once and keeps it, plus a copy resampled to the rate the
    player runs at. Safe to call from the message thread and from the host's
    prepareToPlay thread; never call it from the audio thread. */
class SampleCache
{
public:
    static constexpr double kMaxSeconds = 30.0;

    std::shared_ptr<const SourceSample> get (const juce::File& file, juce::String& error);

    /** The sample as stereo at `sampleRate`, ready for the player. */
    SampleBufferPtr getAtRate (const juce::File& file, double sampleRate, juce::String& error);

    void clear();

private:
    struct Entry
    {
        std::shared_ptr<const SourceSample> source;
        juce::Time modified;
        std::map<int, SampleBufferPtr> resampled;   // key: rounded sample rate
        juce::uint32 lastUsed = 0;
    };

    Entry* findOrLoad (const juce::File& file, juce::String& error);
    void evictIfNeeded();

    static constexpr size_t kMaxEntries = 64;

    juce::CriticalSection lock;
    std::map<juce::String, Entry> entries;
    AudioIO io;
    juce::uint32 useCounter = 0;
};

} // namespace foundry
