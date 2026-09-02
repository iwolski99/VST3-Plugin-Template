#pragma once

#include "AudioIO.h"

namespace foundry
{

/** Lock-free-ish preview player. Files are decoded on the message/worker
    thread and swapped in under a spinlock; process() never allocates. */
class PreviewEngine
{
public:
    void prepare (double sampleRate, int samplesPerBlock);
    void reset();

    bool loadAndPlay (const juce::File& file, bool loop, juce::String& error);
    void play();
    void stop();
    void retrigger();

    void setVolume (float linear) noexcept { volume.store (juce::jlimit (0.0f, 1.5f, linear)); }
    float getVolume() const noexcept { return volume.load(); }

    bool isPlaying() const noexcept { return playing.load(); }
    juce::String currentName() const;
    juce::File   currentFile() const { return file; }
    bool         currentIsLoop() const noexcept { return looping.load(); }

    /** Mixes preview into `buffer` (does not replace the dry signal). */
    void mixInto (juce::AudioBuffer<float>& buffer) noexcept;

    void copyPeaks (std::vector<float>& out, int numBins) const;

private:
    struct Clip
    {
        juce::AudioBuffer<float> audio;
        bool loop = false;
    };

    AudioIO io;
    double hostSr = 44100.0;

    mutable juce::SpinLock swapLock;
    std::unique_ptr<Clip> clip;
    std::atomic<int>  position { 0 };
    std::atomic<bool> playing  { false };
    std::atomic<bool> looping  { false };
    std::atomic<float> volume  { 0.80f };

    juce::File file;
    juce::String name;
};

} // namespace foundry
