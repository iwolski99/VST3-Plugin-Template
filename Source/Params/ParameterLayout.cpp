#include "ParameterLayout.h"

namespace pt
{

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { pid::previewVolume, 1 }, "Preview",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 80.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 0); })));

    return { params.begin(), params.end() };
}

void ParamPtrs::bind (juce::AudioProcessorValueTreeState& apvts)
{
    previewVolume = apvts.getRawParameterValue (pid::previewVolume);
}

} // namespace pt
