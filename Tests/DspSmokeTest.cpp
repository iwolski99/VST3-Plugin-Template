/*  Offline DSP smoke test: runs random full-scale noise and silence through
    every combination of Drive and Tone at the extremes, and fails loudly if
    the processor ever produces a NaN, an Inf, or a sample outside a sane
    range. Cheap insurance against the class of bug that only shows up once a
    host actually calls prepareToPlay() with a real sample rate/block size.

    Build with -DTEMPLATE_BUILD_TESTS=ON, then run the TemplateDspTest binary.
    Exit code 0 means clean; non-zero means a failure was printed. */

#include "../Source/PluginProcessor.h"
#include <iostream>

namespace
{
    bool isFinite (float x) noexcept
    {
        return std::isfinite (x);
    }

    int runPass (double sampleRate, int blockSize, int numBlocks,
                float driveNorm, float toneNorm, bool useNoise)
    {
        PluginTemplateAudioProcessor proc;
        proc.prepareToPlay (sampleRate, blockSize);

        auto& apvts = proc.getAPVTS();
        apvts.getParameter (pt::pid::drive)->setValueNotifyingHost (driveNorm);
        apvts.getParameter (pt::pid::tone)->setValueNotifyingHost (toneNorm);
        apvts.getParameter (pt::pid::mix)->setValueNotifyingHost (1.0f);

        juce::Random rng (1234);
        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        int failures = 0;

        for (int b = 0; b < numBlocks; ++b)
        {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                auto* d = buffer.getWritePointer (ch);
                for (int i = 0; i < blockSize; ++i)
                    d[i] = useNoise ? rng.nextFloat() * 2.0f - 1.0f : 0.0f;
            }

            proc.processBlock (buffer, midi);

            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                auto* d = buffer.getReadPointer (ch);
                for (int i = 0; i < blockSize; ++i)
                {
                    if (! isFinite (d[i]) || std::abs (d[i]) > 100.0f)
                    {
                        std::cerr << "FAIL: non-finite or wild sample at block " << b
                                  << " ch " << ch << " sample " << i
                                  << " = " << d[i] << " (drive=" << driveNorm
                                  << " tone=" << toneNorm << ")\n";
                        return 1;
                    }
                }
            }
        }

        return failures;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    int failures = 0;
    const double rates[] { 44100.0, 48000.0, 96000.0 };
    const int blocks[] { 32, 512, 4096 };

    for (double sr : rates)
        for (int bs : blocks)
            for (float drive : { 0.0f, 0.5f, 1.0f })
                for (float tone : { 0.0f, 0.5f, 1.0f })
                    for (bool noise : { false, true })
                        failures += runPass (sr, bs, 8, drive, tone, noise);

    if (failures == 0)
        std::cout << "DspSmokeTest: all passes clean.\n";

    return failures == 0 ? 0 : 1;
}
