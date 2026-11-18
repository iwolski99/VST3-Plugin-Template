#pragma once

#include "Types.h"

namespace foundry
{

class AudioIO
{
public:
    /** Fade applied to the head of a one-shot placed on the grid: enough to
        kill a DC step, short enough to leave the transient intact. Crossfaded
        slices in the splicer want a much longer fade and pass their own. */
    static constexpr int kOneShotFadeInSamples = 8;

    AudioIO();

    /** Decodes any supported audio file (the name predates AIFF/FLAC support).
        `maxSeconds` > 0 stops decoding after that much audio. */
    bool loadWav (const juce::File& file, juce::AudioBuffer<float>& buffer,
                  double& sampleRate, juce::String& error, double maxSeconds = 0.0);

    bool writeWav (const juce::File& file, const juce::AudioBuffer<float>& buffer,
                   double sampleRate, int bitDepth, juce::String& error);

    static juce::AudioBuffer<float> resample (const juce::AudioBuffer<float>& src,
                                              double srcRate, double dstRate);

    static juce::AudioBuffer<float> toStereo (const juce::AudioBuffer<float>& src);

    static void mixAt (juce::AudioBuffer<float>& dest,
                       const juce::AudioBuffer<float>& src,
                       int destSample,
                       float gain,
                       int fadeInSamples,
                       int extraFadeOutSamples);

    static void fadeLoopEnds (juce::AudioBuffer<float>& buffer, int fadeSamples);
    static float peakAbs (const juce::AudioBuffer<float>& buffer) noexcept;
    static void protectPeaks (juce::AudioBuffer<float>& buffer, float ceiling = 0.99f);

    static int beatsToSamples (double beats, double bpm, double sampleRate) noexcept;
    static int loopLengthSamples (int bars, double bpm, double sampleRate) noexcept;
    static int fadeSamplesFor (double sampleRate) noexcept;

    static juce::File uniqueWavPath (const juce::File& dir, const juce::String& stem);

private:
    juce::AudioFormatManager formats;
};

} // namespace foundry
