#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PluginTemplateAudioProcessor::PluginTemplateAudioProcessor()
    : AudioProcessor (BusesProperties()
                           .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", pt::createParameterLayout()),
      presets (apvts)
{
    params.bind (apvts);
}

PluginTemplateAudioProcessor::~PluginTemplateAudioProcessor() = default;

//==============================================================================
bool PluginTemplateAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mono = juce::AudioChannelSet::mono();
    const auto stereo = juce::AudioChannelSet::stereo();
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (in != out)
        return false;

    return in == mono || in == stereo;
}

void PluginTemplateAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    for (auto& e : effect)
        e.prepare (sampleRate);

    constexpr double smoothingSeconds = 0.02;
    smoothedInput.reset (sampleRate, smoothingSeconds);
    smoothedOutput.reset (sampleRate, smoothingSeconds);
    smoothedMix.reset (sampleRate, smoothingSeconds);
    smoothedBypass.reset (sampleRate, smoothingSeconds);

    smoothedInput.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.input->load()));
    smoothedOutput.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.output->load()));
    smoothedMix.setCurrentAndTargetValue (params.mix->load() * 0.01f);
    smoothedBypass.setCurrentAndTargetValue (params.bypass->load() > 0.5f ? 1.0f : 0.0f);

    // 4 rows: input gain, output gain, mix, bypass - one ramp each, computed
    // once per block and shared by every channel.
    rampBuffer.setSize (4, juce::jmax (1, samplesPerBlock));
}

void PluginTemplateAudioProcessor::releaseResources() {}

void PluginTemplateAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    smoothedInput.setTargetValue (juce::Decibels::decibelsToGain (params.input->load()));
    smoothedOutput.setTargetValue (juce::Decibels::decibelsToGain (params.output->load()));
    smoothedMix.setTargetValue (params.mix->load() * 0.01f);
    smoothedBypass.setTargetValue (params.bypass->load() > 0.5f ? 1.0f : 0.0f);

    // Read once per block: fine for controls that shape a static curve
    // (drive amount, tone tilt) rather than a level that would otherwise
    // zipper audibly.
    const float drive01 = params.drive->load() * 0.01f;
    const float tone    = params.tone->load() * 0.01f;   // -1..1

    // Compute each ramp's per-sample trajectory once, so every channel reads
    // the identical sequence of values instead of a stereo pair's channels
    // drifting apart or the smoothing having to be advanced N times per
    // sample for N channels.
    if (rampBuffer.getNumSamples() < numSamples)
        rampBuffer.setSize (4, numSamples, false, false, true);

    auto* inputRow   = rampBuffer.getWritePointer (0);
    auto* outputRow  = rampBuffer.getWritePointer (1);
    auto* mixRow     = rampBuffer.getWritePointer (2);
    auto* bypassRow  = rampBuffer.getWritePointer (3);

    for (int i = 0; i < numSamples; ++i)
    {
        inputRow[i]  = smoothedInput.getNextValue();
        outputRow[i] = smoothedOutput.getNextValue();
        mixRow[i]    = smoothedMix.getNextValue();
        bypassRow[i] = smoothedBypass.getNextValue();
    }

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto& eff  = effect[(size_t) juce::jmin (ch, 1)];

        for (int i = 0; i < numSamples; ++i)
        {
            const float dry = data[i];
            const float in  = dry * inputRow[i];
            const float wet = eff.process (in, drive01, tone);
            const float mixed = juce::jmap (mixRow[i], dry, wet);
            const float out = mixed * outputRow[i];

            data[i] = juce::jmap (bypassRow[i], out, dry);
        }
    }
}

//==============================================================================
void PluginTemplateAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PluginTemplateAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessorEditor* PluginTemplateAudioProcessor::createEditor()
{
    return new PluginTemplateAudioProcessorEditor (*this);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginTemplateAudioProcessor();
}
