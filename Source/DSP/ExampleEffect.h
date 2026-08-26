#pragma once

#include "../Common.h"

namespace pt
{

//==============================================================================
/** A small placeholder effect - tilt-EQ tone control into a tanh soft
    clipper - that exists purely so the template does something audible out
    of the box and the signal chain (input trim -> effect -> dry/wet -> output
    trim) is wired up correctly.

    This is the one file you're expected to gut. Everything around it
    (parameter handling, presets, the GUI framework) does not know or care
    what happens inside process() - swap this class for your real DSP and
    nothing else in the template needs to change, as long as you keep the
    parameter IDs in ParamIDs.h in sync with what your new DSP actually reads. */
class ExampleEffect
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        reset();
    }

    void reset()
    {
        lowState = 0.0f;
    }

    /** drive01: 0-1. tone: -1..1 (dark to bright). */
    float process (float x, float drive01, float tone) noexcept
    {
        // One-pole low-pass, fixed corner: enough to separate a "low" and
        // "high" band for a tilt without needing a designed filter here.
        constexpr float coeff = 0.12f;
        lowState += coeff * (x - lowState);
        const float high = x - lowState;

        const float tilted = x + tone * (high - lowState) * 0.6f;

        // Soft clip, normalised so unity drive is an audible but not
        // punishing amount of grit, and the output level doesn't visibly
        // jump as Drive is turned up from zero.
        const float driveGain = 1.0f + drive01 * 9.0f;
        return std::tanh (tilted * driveGain) / std::tanh (driveGain);
    }

private:
    double sr = 44100.0;
    float lowState = 0.0f;
};

} // namespace pt
