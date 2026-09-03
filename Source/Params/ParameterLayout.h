#pragma once

#include "ParamIDs.h"

namespace pt
{

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

struct ParamPtrs
{
    std::atomic<float>* previewVolume = nullptr;

    void bind (juce::AudioProcessorValueTreeState& apvts);
};

} // namespace pt
