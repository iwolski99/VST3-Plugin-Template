#pragma once

#include "Common.h"
#include "Params/ParameterLayout.h"
#include "DSP/ToneRenderer.h"
#include "DSP/WavExporter.h"
#include "Presets/PresetManager.h"

//==============================================================================
class BinauralBeatsAudioProcessor : public juce::AudioProcessor
{
public:
    BinauralBeatsAudioProcessor();
    ~BinauralBeatsAudioProcessor() override;

    //== AudioProcessor ========================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "BinauralBeats"; }
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

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    pt::PresetManager& getPresetManager() noexcept { return presets; }

    //== Playback ==============================================================
    void startPlayback();
    void stopPlayback();
    bool isPlaying() const noexcept { return playing.load (std::memory_order_relaxed); }
    double getPlaybackSeconds() const noexcept { return playbackSeconds.load (std::memory_order_relaxed); }

    /** Snapshot of the current knobs, safe to call from the message thread. */
    pt::RenderSpec currentSpec() const;

    juce::File renderTempWav (pt::ExportKind kind, juce::String& error);
    bool renderWavToFile (const juce::File& dest, pt::ExportKind kind, juce::String& error);

private:
    juce::AudioProcessorValueTreeState apvts;
    pt::ParamPtrs params;
    pt::PresetManager presets;

    pt::ToneRenderer renderer;

    juce::SmoothedValue<float> smoothedOutput, smoothedMix, smoothedBypass;
    juce::AudioBuffer<float> rampBuffer;
    juce::AudioBuffer<float> genBuffer;

    std::atomic<bool>   playing { false };
    std::atomic<bool>   retrigger { false };
    std::atomic<double> playbackSeconds { 0.0 };

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BinauralBeatsAudioProcessor)
};
