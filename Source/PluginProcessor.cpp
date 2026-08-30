#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
BinauralBeatsAudioProcessor::BinauralBeatsAudioProcessor()
    : AudioProcessor (BusesProperties()
                           .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", pt::createParameterLayout()),
      presets (apvts)
{
    params.bind (apvts);
}

BinauralBeatsAudioProcessor::~BinauralBeatsAudioProcessor() = default;

//==============================================================================
bool BinauralBeatsAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mono = juce::AudioChannelSet::mono();
    const auto stereo = juce::AudioChannelSet::stereo();
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (in != out)
        return false;

    return in == mono || in == stereo;
}

pt::RenderSpec BinauralBeatsAudioProcessor::currentSpec() const
{
    pt::RenderSpec spec;
    spec.sampleRate  = currentSampleRate;
    spec.leftHz      = params.leftFreq->load();
    spec.rightHz     = params.rightFreq->load();
    spec.durationSec = params.duration->load();
    spec.fadeInSec   = params.fadeIn->load();
    spec.fadeOutSec  = params.fadeOut->load();
    spec.mode        = (pt::OutputMode) juce::roundToInt (params.mode->load());
    spec.gain        = 0.35f;
    return spec;
}

void BinauralBeatsAudioProcessor::startPlayback()
{
    retrigger.store (true, std::memory_order_release);
    playing.store (true, std::memory_order_release);
}

void BinauralBeatsAudioProcessor::stopPlayback()
{
    playing.store (false, std::memory_order_release);
}

void BinauralBeatsAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    renderer.prepare (sampleRate);

    constexpr double smoothingSeconds = 0.02;
    smoothedOutput.reset (sampleRate, smoothingSeconds);
    smoothedMix.reset (sampleRate, smoothingSeconds);
    smoothedBypass.reset (sampleRate, smoothingSeconds);

    smoothedOutput.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.output->load()));
    smoothedMix.setCurrentAndTargetValue (params.mix->load() * 0.01f);
    smoothedBypass.setCurrentAndTargetValue (params.bypass->load() > 0.5f ? 1.0f : 0.0f);

    const int n = juce::jmax (1, samplesPerBlock);
    rampBuffer.setSize (3, n);   // output, mix, bypass
    genBuffer.setSize (2, n);
}

void BinauralBeatsAudioProcessor::releaseResources() {}

void BinauralBeatsAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    for (int ch = numChannels; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    smoothedOutput.setTargetValue (juce::Decibels::decibelsToGain (params.output->load()));
    smoothedMix.setTargetValue (params.mix->load() * 0.01f);
    smoothedBypass.setTargetValue (params.bypass->load() > 0.5f ? 1.0f : 0.0f);

    if (rampBuffer.getNumSamples() < numSamples)
        rampBuffer.setSize (3, numSamples, false, false, true);

    if (genBuffer.getNumSamples() < numSamples || genBuffer.getNumChannels() < 2)
        genBuffer.setSize (2, numSamples, false, false, true);

    auto* outputRow = rampBuffer.getWritePointer (0);
    auto* mixRow    = rampBuffer.getWritePointer (1);
    auto* bypassRow = rampBuffer.getWritePointer (2);

    for (int i = 0; i < numSamples; ++i)
    {
        outputRow[i] = smoothedOutput.getNextValue();
        mixRow[i]    = smoothedMix.getNextValue();
        bypassRow[i] = smoothedBypass.getNextValue();
    }

    if (retrigger.exchange (false, std::memory_order_acq_rel))
    {
        renderer.prepare (currentSampleRate);
        renderer.reset();
        playing.store (true, std::memory_order_relaxed);
    }

    auto spec = currentSpec();
    spec.sampleRate = currentSampleRate;
    renderer.setSpec (spec);

    auto* genL = genBuffer.getWritePointer (0);
    auto* genR = genBuffer.getWritePointer (1);
    const bool active = playing.load (std::memory_order_relaxed);

    if (active)
    {
        bool stillGoing = true;

        for (int i = 0; i < numSamples; ++i)
        {
            stillGoing = renderer.processSample (genL[i], genR[i]);
            if (! stillGoing)
            {
                for (int j = i + 1; j < numSamples; ++j)
                    genL[j] = genR[j] = 0.0f;
                break;
            }
        }

        playbackSeconds.store (renderer.getTimeSeconds(), std::memory_order_relaxed);

        if (! stillGoing)
            playing.store (false, std::memory_order_relaxed);
    }
    else
    {
        genBuffer.clear();
    }

    float* outL = buffer.getWritePointer (0);
    float* outR = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        const float dryL = outL[i];
        const float dryR = outR != nullptr ? outR[i] : dryL;
        float wetL = genL[i];
        float wetR = genR[i];

        if (outR == nullptr)
            wetL = wetR = 0.5f * (wetL + wetR);

        const float mixedL = juce::jmap (mixRow[i], dryL, wetL) * outputRow[i];
        const float mixedR = juce::jmap (mixRow[i], dryR, wetR) * outputRow[i];

        outL[i] = juce::jmap (bypassRow[i], mixedL, dryL);
        if (outR != nullptr)
            outR[i] = juce::jmap (bypassRow[i], mixedR, dryR);
    }
}

//==============================================================================
juce::File BinauralBeatsAudioProcessor::renderTempWav (pt::ExportKind kind, juce::String& error)
{
    auto spec = currentSpec();
    spec.sampleRate = 44100.0;

    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                   .getChildFile ("BinauralBeats");
    dir.createDirectory();

    auto file = dir.getChildFile (pt::makeExportFileName (spec, kind));

    if (! renderWavToFile (file, kind, error))
        return {};

    return file;
}

bool BinauralBeatsAudioProcessor::renderWavToFile (const juce::File& dest,
                                                   pt::ExportKind kind,
                                                   juce::String& error)
{
    auto spec = currentSpec();
    spec.sampleRate = 44100.0;
    return pt::renderToWav (dest, spec, kind, error);
}

//==============================================================================
void BinauralBeatsAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void BinauralBeatsAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessorEditor* BinauralBeatsAudioProcessor::createEditor()
{
    return new BinauralBeatsAudioProcessorEditor (*this);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BinauralBeatsAudioProcessor();
}
