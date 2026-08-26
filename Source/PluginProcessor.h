#pragma once

#include "Common.h"
#include "Params/ParameterLayout.h"
#include "DSP/ExampleEffect.h"
#include "Presets/PresetManager.h"

//==============================================================================
class PluginTemplateAudioProcessor : public juce::AudioProcessor
{
public:
    PluginTemplateAudioProcessor();
    ~PluginTemplateAudioProcessor() override;

    //== AudioProcessor ========================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "PluginTemplate"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //== Template-specific ======================================================
    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    pt::PresetManager& getPresetManager() noexcept { return presets; }

private:
    juce::AudioProcessorValueTreeState apvts;
    pt::ParamPtrs params;
    pt::PresetManager presets;

    pt::ExampleEffect effect[2];

    // Every user-facing gain and mix control is smoothed rather than applied
    // as a raw sample-and-hold read of the parameter - the zipper noise a
    // knob produces without this is one of the fastest ways an otherwise
    // fine-sounding plugin reads as unfinished.
    juce::SmoothedValue<float> smoothedInput, smoothedOutput, smoothedMix, smoothedBypass;

    // Scratch space for the per-sample ramps below, sized once in
    // prepareToPlay() rather than allocated inside processBlock().
    juce::AudioBuffer<float> rampBuffer;

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginTemplateAudioProcessor)
};
