#include "ParameterLayout.h"

namespace pt
{

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto hzString = [] (float v, int)
    {
        return juce::String (v, v >= 100.0f ? 1 : 2);
    };

    auto secString = [] (float v, int)
    {
        if (v >= 60.0f)
        {
            const int minutes = (int) (v / 60.0f);
            const float secs  = v - (float) minutes * 60.0f;
            return juce::String (minutes) + ":"
                 + juce::String (secs, 1).paddedLeft ('0', 4);
        }

        return juce::String (v, 1);
    };

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pid::leftFreq, 1 }, "Left Freq",
        juce::NormalisableRange<float> (20.0f, 20000.0f, 0.1f, 0.3f), 200.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("Hz")
            .withStringFromValueFunction (hzString)));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pid::rightFreq, 1 }, "Right Freq",
        juce::NormalisableRange<float> (20.0f, 20000.0f, 0.1f, 0.3f), 210.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("Hz")
            .withStringFromValueFunction (hzString)));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pid::beatFreq, 1 }, "Beat Freq",
        juce::NormalisableRange<float> (0.0f, 60.0f, 0.01f), 10.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("Hz")
            .withStringFromValueFunction ([] (float v, int)
                                          { return juce::String (v, 2); })));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pid::duration, 1 }, "Duration",
        juce::NormalisableRange<float> (0.5f, 3600.0f, 0.1f, 0.35f), 60.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("s")
            .withStringFromValueFunction (secString)));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pid::fadeIn, 1 }, "Fade In",
        juce::NormalisableRange<float> (0.0f, 60.0f, 0.1f), 2.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("s")
            .withStringFromValueFunction ([] (float v, int)
                                          { return juce::String (v, 1); })));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pid::fadeOut, 1 }, "Fade Out",
        juce::NormalisableRange<float> (0.0f, 60.0f, 0.1f), 2.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("s")
            .withStringFromValueFunction ([] (float v, int)
                                          { return juce::String (v, 1); })));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { pid::mode, 1 }, "Mode",
        juce::StringArray { "Pure Tone", "Binaural", "Both" }, 1));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pid::mix, 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("%")
            .withStringFromValueFunction ([] (float v, int)
                                          { return juce::String (v, 0); })));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pid::output, 1 }, "Output",
        juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), -6.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("dB")
            .withStringFromValueFunction ([] (float v, int)
                                          { return juce::String (v, 1); })));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { pid::bypass, 1 }, "Bypass", false));

    return { params.begin(), params.end() };
}

void ParamPtrs::bind (juce::AudioProcessorValueTreeState& apvts)
{
    leftFreq  = apvts.getRawParameterValue (pid::leftFreq);
    rightFreq = apvts.getRawParameterValue (pid::rightFreq);
    beatFreq  = apvts.getRawParameterValue (pid::beatFreq);
    duration  = apvts.getRawParameterValue (pid::duration);
    fadeIn    = apvts.getRawParameterValue (pid::fadeIn);
    fadeOut   = apvts.getRawParameterValue (pid::fadeOut);
    mode      = apvts.getRawParameterValue (pid::mode);
    mix       = apvts.getRawParameterValue (pid::mix);
    output    = apvts.getRawParameterValue (pid::output);
    bypass    = apvts.getRawParameterValue (pid::bypass);
}

} // namespace pt
