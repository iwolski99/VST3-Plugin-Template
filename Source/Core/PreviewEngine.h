#pragma once

#include "AudioIO.h"

namespace foundry
{

/** Loop / one-shot player. Files are decoded on the message thread and swapped
    in under a spinlock; render() never allocates and never blocks.

    The player is the plugin's only sound source: the PLAY button, MIDI notes
    from the piano roll and the host transport all drive this one voice, so
    whatever loop is loaded plays back at its recorded pitch no matter how it
    was triggered. */
class PreviewEngine
{
public:
    void prepare (double sampleRate, int samplesPerBlock);
    void reset();

    /** Decodes `file` and starts it. Returns false and fills `error` on failure. */
    bool loadAndPlay (const juce::File& file, bool loop, juce::String& error);

    /** Decodes `file` and arms it without starting playback. */
    bool load (const juce::File& file, bool loop, juce::String& error);

    void play();        // resume from the current position
    void stop();        // ramped stop
    void retrigger();   // restart from the top

    /** True once a clip is decoded, whether or not the file is still on disk.
        The transport is gated on this rather than on the file so a loaded loop
        stays playable after its source is moved or deleted. */
    bool hasClip() const noexcept { return clipLoaded.load(); }

    void setVolume (float linear) noexcept { volume.store (juce::jlimit (0.0f, 1.5f, linear)); }
    float getVolume() const noexcept { return volume.load(); }

    bool isPlaying() const noexcept { return playing.load(); }
    juce::String currentName() const;
    juce::File   currentFile() const;
    bool         currentIsLoop() const noexcept { return looping.load(); }

    int    clipLengthSamples() const noexcept { return clipSamples.load(); }
    double clipLengthSeconds() const noexcept;
    int    positionSamples() const noexcept { return position.load(); }

    /** Aligns playback to the host timeline so a loop stays locked to the bar.
        `ppqPosition` is the host's quarter-note position. */
    void alignToHost (double ppqPosition, double bpm) noexcept;

    /** Mixes the voice into `buffer` (adds; does not replace the dry signal). */
    void render (juce::AudioBuffer<float>& buffer) noexcept;

    /** Peaks are computed once per load, so the UI never scans the clip while
        the audio thread is trying to read it. */
    void copyPeaks (std::vector<float>& out, int numBins) const;

    static constexpr int kPeakBins = 96;

private:
    struct Clip
    {
        juce::AudioBuffer<float> audio;
        bool loop = false;
        std::vector<float> peaks;
    };

    bool loadInternal (const juce::File& file, bool loop, bool startNow, juce::String& error);
    static std::vector<float> computePeaks (const juce::AudioBuffer<float>& audio, int numBins);

    AudioIO io;
    double hostSr = 44100.0;

    mutable juce::SpinLock swapLock;
    std::unique_ptr<Clip> clip;

    std::atomic<int>  position    { 0 };
    std::atomic<bool> playing     { false };
    std::atomic<bool> looping     { false };
    std::atomic<bool> clipLoaded  { false };
    std::atomic<bool> restart     { false };
    std::atomic<int>  clipSamples { 0 };
    std::atomic<float> volume     { 0.80f };

    // Audio-thread only.
    float envelope = 0.0f;
    float envStep = 1.0f;

    mutable juce::CriticalSection nameLock;
    juce::File file;
    juce::String name;
};

} // namespace foundry
