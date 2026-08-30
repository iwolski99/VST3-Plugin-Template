#pragma once

#include "../Common.h"

namespace pt
{

enum class OutputMode : int
{
    pureTone = 0,
    binaural = 1,
    both     = 2
};

enum class ExportKind : int
{
    binaural = 0,
    pureTone = 1
};

//==============================================================================
struct RenderSpec
{
    double sampleRate = 44100.0;
    float  leftHz     = 200.0f;
    float  rightHz    = 210.0f;
    float  durationSec = 60.0f;
    float  fadeInSec  = 2.0f;
    float  fadeOutSec = 2.0f;
    OutputMode mode   = OutputMode::binaural;
    float  gain       = 0.35f;   // headroom so a full-scale sine never clips
};

//==============================================================================
inline float envelopeAt (double t, float duration, float fadeIn, float fadeOut) noexcept
{
    if (duration <= 0.0f || t < 0.0 || t >= (double) duration)
        return 0.0f;

    fadeIn  = juce::jlimit (0.0f, duration, fadeIn);
    fadeOut = juce::jlimit (0.0f, duration, fadeOut);

    if (fadeIn + fadeOut > duration)
    {
        const float scale = duration / juce::jmax (0.0001f, fadeIn + fadeOut);
        fadeIn  *= scale;
        fadeOut *= scale;
    }

    float env = 1.0f;

    if (fadeIn > 0.0f && t < (double) fadeIn)
    {
        const float x = (float) (t / (double) fadeIn);
        env *= 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * x));
    }

    if (fadeOut > 0.0f && t > (double) (duration - fadeOut))
    {
        const float x = (float) (((double) duration - t) / (double) fadeOut);
        env *= 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi
                                        * juce::jlimit (0.0f, 1.0f, x)));
    }

    return env;
}

inline const char* bandNameForBeat (float beatHz) noexcept
{
    if (beatHz < 0.5f)  return "Isochronic / unison";
    if (beatHz < 4.0f)  return "Delta";
    if (beatHz < 8.0f)  return "Theta";
    if (beatHz < 13.0f) return "Alpha";
    if (beatHz < 30.0f) return "Beta";
    return "Gamma";
}

//==============================================================================
/** Offline- and realtime-safe sine renderer. One instance per thread. */
class ToneRenderer
{
public:
    void prepare (double sr)
    {
        spec.sampleRate = sr;
        reset();
    }

    void setSpec (const RenderSpec& s) noexcept { spec = s; }
    const RenderSpec& getSpec() const noexcept { return spec; }

    void reset() noexcept
    {
        phaseL = phaseR = phaseT = 0.0;
        sampleIndex = 0;
    }

    /** Writes one stereo pair. Returns false once duration has elapsed. */
    bool processSample (float& left, float& right) noexcept
    {
        const double sr = juce::jmax (1.0, spec.sampleRate);
        const double t  = (double) sampleIndex / sr;

        if (t >= (double) spec.durationSec)
        {
            left = right = 0.0f;
            return false;
        }

        const float env = envelopeAt (t, spec.durationSec, spec.fadeInSec, spec.fadeOutSec)
                          * spec.gain;

        const double twoPi = juce::MathConstants<double>::twoPi;
        const double incL  = twoPi * (double) spec.leftHz  / sr;
        const double incR  = twoPi * (double) spec.rightHz / sr;
        const double incT  = incL; // pure tone uses the left / carrier frequency

        const float sL = (float) std::sin (phaseL);
        const float sR = (float) std::sin (phaseR);
        const float sT = (float) std::sin (phaseT);

        switch (spec.mode)
        {
            case OutputMode::pureTone:
                left = right = sT * env;
                break;

            case OutputMode::binaural:
                left  = sL * env;
                right = sR * env;
                break;

            case OutputMode::both:
                // Equal-power mix of the mono carrier into both ears plus the
                // binaural pair, so previewing "Both" is audible as both things
                // without clipping.
                left  = (sL + sT) * (0.5f * env);
                right = (sR + sT) * (0.5f * env);
                break;
        }

        phaseL += incL;
        phaseR += incR;
        phaseT += incT;

        if (phaseL > twoPi) phaseL -= twoPi;
        if (phaseR > twoPi) phaseR -= twoPi;
        if (phaseT > twoPi) phaseT -= twoPi;

        ++sampleIndex;
        return true;
    }

    void render (juce::AudioBuffer<float>& buffer, int numChannels)
    {
        const int n = buffer.getNumSamples();
        auto* l = buffer.getWritePointer (0);
        float* r = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

        for (int i = 0; i < n; ++i)
        {
            float left = 0.0f, right = 0.0f;
            processSample (left, right);
            l[i] = left;
            if (r != nullptr)
                r[i] = right;
        }

        for (int ch = 2; ch < numChannels; ++ch)
            buffer.clear (ch, 0, n);
    }

    double getTimeSeconds() const noexcept
    {
        return (double) sampleIndex / juce::jmax (1.0, spec.sampleRate);
    }

private:
    RenderSpec spec;
    double phaseL = 0.0, phaseR = 0.0, phaseT = 0.0;
    juce::int64 sampleIndex = 0;
};

} // namespace pt
