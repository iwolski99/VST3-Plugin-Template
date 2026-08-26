#include "ParameterLayout.h"

namespace pt
{

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto pct = [] { return juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f); };

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pid::input, 1 }, "Input",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("dB")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1); })));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pid::drive, 1 }, "Drive",
        pct(), 20.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 0); })));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pid::tone, 1 }, "Tone",
        juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 0); })));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pid::mix, 1 }, "Mix",
        pct(), 100.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 0); })));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pid::output, 1 }, "Output",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("dB")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1); })));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { pid::bypass, 1 }, "Bypass", false));

    return { params.begin(), params.end() };
}

void ParamPtrs::bind (juce::AudioProcessorValueTreeState& apvts)
{
    input  = apvts.getRawParameterValue (pid::input);
    drive  = apvts.getRawParameterValue (pid::drive);
    tone   = apvts.getRawParameterValue (pid::tone);
    mix    = apvts.getRawParameterValue (pid::mix);
    output = apvts.getRawParameterValue (pid::output);
    bypass = apvts.getRawParameterValue (pid::bypass);
}

} // namespace pt
