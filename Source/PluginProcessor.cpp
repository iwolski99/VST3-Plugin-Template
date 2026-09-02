#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Core/LibraryScan.h"

//==============================================================================
FoundryAudioProcessor::FoundryAudioProcessor()
    : AudioProcessor (BusesProperties()
                           .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", pt::createParameterLayout())
{
    params.bind (apvts);

    juce::String err;
    config.load (foundry::defaultConfigFile(), err);

    uiGenre = config.get().defaultGenre;
    uiBars = config.get().defaultBars;
    uiQuantity = config.get().defaultQuantity;
    renderFull = true;
    renderStems = true;

    if (config.get().savedDrumPatternJson.isNotEmpty())
        drumPattern = foundry::DrumPattern::fromJson (config.get().savedDrumPatternJson, uiBars);
    else
        drumPattern = foundry::defaultDrumPattern (uiGenre, uiBars);

    if (auto* p = apvts.getParameter (pt::pid::previewVolume))
        p->setValueNotifyingHost (p->convertTo0to1 (config.get().previewVolume * 100.0f));

    reopenDatabase (err);
}

FoundryAudioProcessor::~FoundryAudioProcessor()
{
    worker.requestCancel();
    worker.stopThread (12000);

    if (params.previewVolume != nullptr)
        config.get().previewVolume = params.previewVolume->load() * 0.01f;

    juce::String err;
    persistConfig (err);
    library.close();
}

bool FoundryAudioProcessor::persistConfig (juce::String& error)
{
    config.get().defaultGenre = uiGenre;
    config.get().defaultBars = uiBars;
    config.get().defaultQuantity = uiQuantity;
    config.get().savedDrumPatternJson = drumPattern.toJson();

    if (params.previewVolume != nullptr)
        config.get().previewVolume = juce::jlimit (0.0f, 1.0f, params.previewVolume->load() * 0.01f);

    return config.save (foundry::defaultConfigFile(), error);
}

bool FoundryAudioProcessor::reopenDatabase (juce::String& error)
{
    library.close();
    return library.open (config.databaseFile(), error);
}

void FoundryAudioProcessor::setUiGenre (foundry::Genre g)
{
    uiGenre = g;
    if (config.get().tempoMode == foundry::TempoMode::GenreDefault)
        config.get().customBpm = foundry::genreDefaultBpm (g);
}

void FoundryAudioProcessor::setUiBars (int bars)
{
    uiBars = juce::jlimit (1, 4, bars);
    drumPattern.setBars (uiBars);
}

void FoundryAudioProcessor::setUiQuantity (int q)
{
    uiQuantity = juce::jlimit (foundry::kMinQuantity, foundry::kMaxQuantity, q);
}

bool FoundryAudioProcessor::previewItem (const foundry::LibraryItem& item, juce::String& error)
{
    const bool loop = foundry::isGeneratedCategory (item.category) || item.fileType == "loop";
    return previewFile (juce::File (item.path), loop, error);
}

bool FoundryAudioProcessor::previewFile (const juce::File& file, bool loop, juce::String& error)
{
    return preview.loadAndPlay (file, loop, error);
}

juce::String FoundryAudioProcessor::startLibraryScan()
{
    if (worker.isBusy())
        return "A job is already running.";

    if (! worker.startScan (config, library))
        return "Could not start library scan.";

    sendChangeMessage();
    return {};
}

juce::String FoundryAudioProcessor::startDrumGenerate()
{
    if (worker.isBusy())
        return "A job is already running.";

    foundry::DrumRequest req;
    req.genre = uiGenre;
    req.bpm = config.resolveBpm (uiGenre, hostBpm.load());
    req.bars = uiBars;
    req.quantity = uiQuantity;
    req.renderFull = renderFull;
    req.renderStems = renderStems;
    req.protectPeaks = config.get().renderProtection;
    req.sampleRate = config.resolveRenderSampleRate (currentSr.load());
    req.bitDepth = 24;
    req.pattern = drumPattern;
    req.pattern.setBars (uiBars);
    req.kick = locks[0];
    req.clap = locks[1];
    req.hat  = locks[2];
    req.ride = locks[3];
    req.outputDir = config.generatedDrumDir (uiGenre);

    if (! worker.startDrums (config, library, std::move (req)))
        return "Could not start drum generation.";

    sendChangeMessage();
    return {};
}

juce::String FoundryAudioProcessor::startBassGenerate (int sourceCount, foundry::Variation variation)
{
    if (worker.isBusy())
        return "A job is already running.";

    foundry::BassRequest req;
    req.genre = uiGenre;
    req.bpm = config.resolveBpm (uiGenre, hostBpm.load());
    req.bars = uiBars;
    req.quantity = uiQuantity;
    req.sourceCount = juce::jlimit (2, 4, sourceCount);
    req.variation = variation;
    req.protectPeaks = config.get().renderProtection;
    req.sampleRate = config.resolveRenderSampleRate (currentSr.load());
    req.bitDepth = 24;
    req.outputDir = config.generatedBassDir (false);

    auto shots = library.queryByRole (foundry::FolderRole::Bass, {});
    if (! worker.startBass (config, library, std::move (req), shots))
        return "Could not start bass generation.";

    sendChangeMessage();
    return {};
}

juce::String FoundryAudioProcessor::startSpliceGenerate (const std::vector<juce::File>& sources,
                                                         const juce::String& parentIds,
                                                         int parentDepth,
                                                         foundry::SliceRes slice,
                                                         foundry::SpliceMode mode,
                                                         foundry::Variation variation,
                                                         int quantity, int bars)
{
    if (worker.isBusy())
        return "A job is already running.";

    foundry::SpliceRequest req;
    req.genre = uiGenre;
    req.bpm = config.resolveBpm (uiGenre, hostBpm.load());
    req.bars = juce::jlimit (1, 4, bars);
    req.quantity = juce::jlimit (foundry::kMinQuantity, foundry::kMaxQuantity, quantity);
    req.slice = slice;
    req.mode = mode;
    req.variation = variation;
    req.protectPeaks = config.get().renderProtection;
    req.sampleRate = config.resolveRenderSampleRate (currentSr.load());
    req.bitDepth = 24;
    req.outputDir = config.generatedBassDir (true);
    req.sources = sources;
    req.parentIds = parentIds;
    req.parentDepth = parentDepth;

    if (! worker.startSplice (config, library, std::move (req)))
        return "Could not start riff splicer.";

    sendChangeMessage();
    return {};
}

//==============================================================================
bool FoundryAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mono = juce::AudioChannelSet::mono();
    const auto stereo = juce::AudioChannelSet::stereo();
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (out != mono && out != stereo)
        return false;

    if (in != juce::AudioChannelSet::disabled() && in != out)
        return false;

    return true;
}

void FoundryAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSr.store (sampleRate);
    preview.prepare (sampleRate, samplesPerBlock);
    smoothedPreviewGain.reset (sampleRate, 0.02);
    const float v = params.previewVolume != nullptr ? params.previewVolume->load() * 0.01f : 0.8f;
    smoothedPreviewGain.setCurrentAndTargetValue (v);
    preview.setVolume (v);
}

void FoundryAudioProcessor::releaseResources()
{
    preview.reset();
}

void FoundryAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    for (int i = getTotalNumInputChannels(); i < numChannels; ++i)
        buffer.clear (i, 0, numSamples);

    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm())
                hostBpm.store (*bpm);
        }
    }

    if (params.previewVolume != nullptr)
    {
        const float v = params.previewVolume->load() * 0.01f;
        smoothedPreviewGain.setTargetValue (v);
        preview.setVolume (smoothedPreviewGain.getNextValue());
        // Advance the rest of the block so the smoother stays in time even
        // though preview applies a per-block gain (volume isn't sample-accurate).
        for (int i = 1; i < numSamples; ++i)
            smoothedPreviewGain.getNextValue();
    }

    preview.mixInto (buffer);
}

//==============================================================================
void FoundryAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("uiGenre", (int) uiGenre, nullptr);
    state.setProperty ("uiBars", uiBars, nullptr);
    state.setProperty ("uiQuantity", uiQuantity, nullptr);
    state.setProperty ("renderFull", renderFull, nullptr);
    state.setProperty ("renderStems", renderStems, nullptr);
    state.setProperty ("drumPattern", drumPattern.toJson(), nullptr);
    state.setProperty ("lastTab", config.get().lastTab, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void FoundryAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));

        auto state = apvts.state;
        uiGenre = (foundry::Genre) (int) state.getProperty ("uiGenre", (int) uiGenre);
        uiBars = juce::jlimit (1, 4, (int) state.getProperty ("uiBars", uiBars));
        uiQuantity = juce::jlimit (1, 50, (int) state.getProperty ("uiQuantity", uiQuantity));
        renderFull = (bool) state.getProperty ("renderFull", true);
        renderStems = (bool) state.getProperty ("renderStems", true);
        config.get().lastTab = (int) state.getProperty ("lastTab", 0);

        const auto json = state.getProperty ("drumPattern").toString();
        if (json.isNotEmpty())
            drumPattern = foundry::DrumPattern::fromJson (json, uiBars);
    }
}

juce::AudioProcessorEditor* FoundryAudioProcessor::createEditor()
{
    return new FoundryAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FoundryAudioProcessor();
}
