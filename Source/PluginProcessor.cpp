#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Core/LibraryScan.h"

//==============================================================================
FoundryAudioProcessor::FoundryAudioProcessor()
    : AudioProcessor (BusesProperties()
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

    // Seed the player from the stored preference. Calling setValueNotifyingHost
    // from a constructor pushes a parameter change at a host that has not
    // finished instantiating us yet, which some hosts do not survive.
    preview.setVolume (juce::jlimit (0.0f, 1.0f, config.get().previewVolume));
    followTransport.store (config.get().followTransport);

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

bool FoundryAudioProcessor::loadNewestGenerated (juce::String& error)
{
    const foundry::Category cats[] = { foundry::drumLoopCategory (uiGenre),
                                       foundry::Category::BassLoopsSpliced,
                                       foundry::Category::BassLoopsBase };

    foundry::LibraryItem newest;
    bool found = false;

    for (auto cat : cats)
    {
        for (const auto& it : library.query (cat, {}, foundry::SortMode::Newest, false))
        {
            if (! juce::File (it.path).existsAsFile())
                continue;

            if (! found || it.dateCreated > newest.dateCreated)
            {
                newest = it;
                found = true;
            }
            break; // the query is already newest-first
        }
    }

    if (! found)
    {
        error = "No generated loops in the library yet.";
        return false;
    }

    // Arm it without starting: the user decides when to hit PLAY.
    return preview.load (juce::File (newest.path), true, error);
}

void FoundryAudioProcessor::setFollowTransport (bool shouldFollow)
{
    followTransport.store (shouldFollow);
    config.get().followTransport = shouldFollow;
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
    const auto out = layouts.getMainOutputChannelSet();

    if (out != mono && out != stereo)
        return false;

    // A generator takes no audio in; refuse any layout that hands us one.
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::disabled();
}

double FoundryAudioProcessor::getTailLengthSeconds() const
{
    return preview.clipLengthSeconds();
}

void FoundryAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSr.store (sampleRate);

    // Note and transport tracking do not survive a stop/start, otherwise a
    // note-on the host never matched with a note-off leaves the voice stuck.
    heldNotes = 0;
    transportWasPlaying = false;
    startedByTransport = false;

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

void FoundryAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();

    // Nothing feeds us, so start from silence every block rather than passing
    // through whatever the host left in the buffer.
    buffer.clear();

    bool transportPlaying = false;
    double ppq = 0.0;
    bool havePpq = false;

    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm())
                hostBpm.store (*bpm);

            transportPlaying = pos->getIsPlaying();

            if (auto p = pos->getPpqPosition())
            {
                ppq = *p;
                havePpq = true;
            }
        }
    }

    // Any note plays the loaded loop at its recorded pitch - Foundry plays
    // finished loops, so the note chosen in the piano roll only says when to
    // start, never how to transpose.
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();

        if (m.isNoteOn())
        {
            ++heldNotes;
            startedByTransport = false;
            preview.retrigger();
        }
        else if (m.isNoteOff())
        {
            heldNotes = juce::jmax (0, heldNotes - 1);
            if (heldNotes == 0 && ! startedByTransport)
                preview.stop();
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            heldNotes = 0;
            if (! startedByTransport)
                preview.stop();
        }
    }

    if (followTransport.load())
    {
        if (transportPlaying && ! transportWasPlaying)
        {
            if (havePpq)
                preview.alignToHost (ppq, hostBpm.load());

            if (heldNotes == 0 && preview.hasClip())
            {
                startedByTransport = true;
                preview.play();
            }
        }
        else if (! transportPlaying && transportWasPlaying && startedByTransport)
        {
            startedByTransport = false;
            preview.stop();
        }
    }

    transportWasPlaying = transportPlaying;

    if (params.previewVolume != nullptr)
    {
        const float v = params.previewVolume->load() * 0.01f;
        smoothedPreviewGain.setTargetValue (v);
        preview.setVolume (smoothedPreviewGain.getNextValue());
        // Advance the rest of the block so the smoother stays in time even
        // though the player applies a per-block gain (volume isn't sample-accurate).
        for (int i = 1; i < numSamples; ++i)
            smoothedPreviewGain.getNextValue();
    }

    preview.render (buffer);
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
    state.setProperty ("followTransport", followTransport.load(), nullptr);
    state.setProperty ("loadedLoop", preview.currentFile().getFullPathName(), nullptr);
    state.setProperty ("loadedLoopLoops", preview.currentIsLoop(), nullptr);

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

        setFollowTransport ((bool) state.getProperty ("followTransport", followTransport.load()));

        // Re-arm the loop the session was saved with, stopped: reopening a
        // project should leave PLAY ready rather than empty.
        const auto loopPath = state.getProperty ("loadedLoop").toString();
        if (loopPath.isNotEmpty())
        {
            const juce::File f (loopPath);
            if (f.existsAsFile())
            {
                juce::String err;
                preview.load (f, (bool) state.getProperty ("loadedLoopLoops", true), err);
            }
        }

        // An editor may already be open on this instance.
        sendChangeMessage();
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
