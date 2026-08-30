#pragma once

#include "ParamIDs.h"

namespace pt
{

/** Builds the complete parameter tree. Keep every parameter's range and
    default here, in one place, so the processor, the presets and the GUI
    never disagree about what a given ID means. */
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

//==============================================================================
/** Raw atomic pointers, resolved once so the audio thread never does a string
    lookup into the APVTS. */
struct ParamPtrs
{
    std::atomic<float>* leftFreq  = nullptr;
    std::atomic<float>* rightFreq = nullptr;
    std::atomic<float>* beatFreq  = nullptr;
    std::atomic<float>* duration  = nullptr;
    std::atomic<float>* fadeIn    = nullptr;
    std::atomic<float>* fadeOut   = nullptr;
    std::atomic<float>* mode      = nullptr;
    std::atomic<float>* mix       = nullptr;
    std::atomic<float>* output    = nullptr;
    std::atomic<float>* bypass    = nullptr;

    void bind (juce::AudioProcessorValueTreeState& apvts);
};

} // namespace pt
