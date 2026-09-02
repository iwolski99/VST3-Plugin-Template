#pragma once

#include "Params/ParameterLayout.h"
#include "Core/ConfigStore.h"
#include "Core/JobWorker.h"
#include "Core/LibraryDatabase.h"
#include "Core/PreviewEngine.h"

//==============================================================================
class FoundryAudioProcessor : public juce::AudioProcessor,
                              public juce::ChangeBroadcaster
{
public:
    FoundryAudioProcessor();
    ~FoundryAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Foundry"; }
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
    foundry::ConfigStore&       getConfig()  noexcept { return config; }
    foundry::LibraryDatabase&   getLibrary() noexcept { return library; }
    foundry::PreviewEngine&     getPreview() noexcept { return preview; }
    foundry::JobWorker&         getWorker()  noexcept { return worker; }

    foundry::DrumPattern&       getDrumPattern() noexcept { return drumPattern; }
    foundry::SampleLock&        getLock (int row) noexcept { return locks[(size_t) juce::jlimit (0, 3, row)]; }

    double getHostBpm() const noexcept { return hostBpm.load(); }
    double getSampleRate() const noexcept { return currentSr.load(); }

    bool persistConfig (juce::String& error);
    bool reopenDatabase (juce::String& error);

    bool previewItem (const foundry::LibraryItem& item, juce::String& error);
    bool previewFile (const juce::File& file, bool loop, juce::String& error);

    juce::String startLibraryScan();
    juce::String startDrumGenerate();
    juce::String startBassGenerate (int sourceCount, foundry::Variation variation);
    juce::String startSpliceGenerate (const std::vector<juce::File>& sources,
                                      const juce::String& parentIds,
                                      int parentDepth,
                                      foundry::SliceRes slice,
                                      foundry::SpliceMode mode,
                                      foundry::Variation variation,
                                      int quantity, int bars);

    foundry::Genre getUiGenre() const noexcept { return uiGenre; }
    void setUiGenre (foundry::Genre g);

    int getUiBars() const noexcept { return uiBars; }
    void setUiBars (int bars);

    int getUiQuantity() const noexcept { return uiQuantity; }
    void setUiQuantity (int q);

    bool renderFull = true;
    bool renderStems = true;

private:
    juce::AudioProcessorValueTreeState apvts;
    pt::ParamPtrs params;

    foundry::ConfigStore config;
    foundry::LibraryDatabase library;
    foundry::PreviewEngine preview;
    foundry::JobWorker worker;

    foundry::DrumPattern drumPattern;
    foundry::SampleLock locks[4];
    foundry::Genre uiGenre = foundry::Genre::Riddim;
    int uiBars = 2;
    int uiQuantity = 10;

    std::atomic<double> hostBpm { 0.0 };
    std::atomic<double> currentSr { 48000.0 };

    juce::SmoothedValue<float> smoothedPreviewGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FoundryAudioProcessor)
};
