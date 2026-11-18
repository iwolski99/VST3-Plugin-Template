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

    // A starter bass groove for the pattern grid: A on the one, answered by B.
    bassPattern.setBars (uiBars);
    bassPattern.clear();
    for (int bar = 0; bar < uiBars; ++bar)
        for (auto [row, step] : { std::pair<int, int> { 0, 0 }, { 0, 3 }, { 1, 6 }, { 1, 8 }, { 0, 12 } })
            bassPattern.setStep (row, bar * foundry::kStepsPerBar + step, true);
    // Seed the player from the stored preference. Calling setValueNotifyingHost
    // from a constructor pushes a parameter change at a host that has not
    // finished instantiating us yet, which some hosts do not survive.
    preview.setVolume (juce::jlimit (0.0f, 1.0f, config.get().previewVolume));
    riff.setVolume (juce::jlimit (0.0f, 1.0f, config.get().previewVolume));
    riff.setFollowHost (config.get().followTransport);
    standaloneBpm.store (juce::jlimit (40.0, 300.0, config.get().customBpm));

    reopenDatabase (err);
    migrateLegacyPatterns();
}

FoundryAudioProcessor::~FoundryAudioProcessor()
{
    worker.requestCancel();
    worker.stopThread (12000);

    if (params.previewVolume != nullptr)
        config.get().previewVolume = params.previewVolume->load() * 0.01f;

    juce::String err;
    persistConfig (err, true);
    library.close();
}

bool FoundryAudioProcessor::persistConfig (juce::String& error, bool preferencesOnly)
{
    config.get().defaultGenre = uiGenre;
    config.get().defaultBars = uiBars;
    config.get().defaultQuantity = uiQuantity;
    config.get().savedDrumPatternJson = drumPattern.toJson();

    if (params.previewVolume != nullptr)
        config.get().previewVolume = juce::jlimit (0.0f, 1.0f, params.previewVolume->load() * 0.01f);

    config.get().followTransport = riff.getFollowHost();

    return preferencesOnly ? config.savePreferences (foundry::defaultConfigFile(), error)
                           : config.save (foundry::defaultConfigFile(), error);
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
    bassPattern.setBars (uiBars);
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
    riff.setFollowHost (shouldFollow);
    config.get().followTransport = shouldFollow;
}

//==============================================================================
// Patterns

void FoundryAudioProcessor::migrateLegacyPatterns()
{
    // Older versions kept saved drum patterns inside config.json. They now
    // live as files in the pattern library, where the browser can show them.
    const auto& names = config.get().customPatternNames;
    const auto& jsons = config.get().customPatternJson;
    for (int i = 0; i < juce::jmin (names.size(), jsons.size()); ++i)
    {
        auto f = config.patternDir (false).getChildFile (foundry::legalStem (names[i]) + foundry::kPatternExtension);
        if (f.existsAsFile())
            continue;
        foundry::PatternFile pf;
        pf.kind = foundry::PatternKind::Drums;
        pf.grid = foundry::DrumPattern::fromJson (jsons[i], uiBars);
        pf.save (f);
    }
}

bool FoundryAudioProcessor::hasPattern (const juce::File& file)
{
    return file.hasFileExtension (foundry::kPatternExtension)
        || foundry::patternSidecarFor (file).existsAsFile();
}

bool FoundryAudioProcessor::loadPattern (const juce::File& file, foundry::PatternFile& out, juce::String& error) const
{
    const auto f = file.hasFileExtension (foundry::kPatternExtension) ? file : foundry::patternSidecarFor (file);
    if (! f.existsAsFile())
    {
        error = file.getFileName() + " has no pattern saved with it (loops generated by this version of Foundry do).";
        return false;
    }
    if (! foundry::PatternFile::load (f, out))
    {
        error = "Could not read " + f.getFileName();
        return false;
    }
    return true;
}

juce::File FoundryAudioProcessor::savePattern (const foundry::PatternFile& pattern, const juce::String& name,
                                               juce::String& error)
{
    const auto dir = config.patternDir (pattern.kind == foundry::PatternKind::Bass);
    const auto stem = foundry::legalStem (name.trim().isNotEmpty() ? name.trim() : juce::String ("Pattern"));
    auto file = dir.getChildFile (stem + foundry::kPatternExtension);
    if (file.existsAsFile())
        file = dir.getNonexistentChildFile (stem, foundry::kPatternExtension, false);

    if (! pattern.save (file))
    {
        error = "Could not write " + file.getFullPathName();
        return {};
    }
    return file;
}

juce::Array<juce::File> FoundryAudioProcessor::listPatterns (foundry::PatternKind kind) const
{
    auto files = config.patternDir (kind == foundry::PatternKind::Bass)
                     .findChildFiles (juce::File::findFiles, true, juce::String ("*") + foundry::kPatternExtension);
    files.sort();
    return files;
}

//==============================================================================
// File management

bool FoundryAudioProcessor::isManagedFile (const juce::File& f) const
{
    for (const auto& dir : { config.generatedDrumsRoot(), config.generatedBassRoot(),
                             config.riffDir(), config.patternsRoot() })
        if (dir.getFullPathName().isNotEmpty() && f.isAChildOf (dir))
            return true;
    return false;
}

int FoundryAudioProcessor::deleteFiles (const juce::StringArray& paths, int& skipped)
{
    int deleted = 0;
    skipped = 0;
    juce::String err;

    for (const auto& p : paths)
    {
        const juce::File f (p);
        if (! isManagedFile (f))
        {
            ++skipped;
            continue;
        }

        if (f.existsAsFile() && ! f.deleteFile())
            continue;

        if (! f.hasFileExtension (foundry::kPatternExtension))
        {
            foundry::patternSidecarFor (f).deleteFile();
            foundry::riffSidecarFor (f).deleteFile();
        }

        library.removeByPath (p, err);
        thumbnails.forget (p);
        ++deleted;
    }

    return deleted;
}

int FoundryAudioProcessor::moveOrCopyFiles (const juce::StringArray& paths, const juce::File& dir,
                                            int& copied, juce::String& error)
{
    copied = 0;
    if (! dir.isDirectory())
    {
        error = "That folder doesn't exist on disk.";
        return 0;
    }

    int done = 0;
    for (const auto& p : paths)
    {
        const juce::File src (p);
        if (! src.existsAsFile() || src.getParentDirectory() == dir)
            continue;

        const auto target = dir.getNonexistentChildFile (src.getFileNameWithoutExtension(), src.getFileExtension(), false);

        // Foundry's own output moves (so culling a batch keeps the keepers);
        // your own samples are only ever copied.
        const bool move = isManagedFile (src);
        if (! (move ? src.moveFileTo (target) : src.copyFileTo (target)))
        {
            error = "Could not " + juce::String (move ? "move " : "copy ") + src.getFileName();
            continue;
        }

        for (auto sidecar : { foundry::patternSidecarFor (src), foundry::riffSidecarFor (src) })
        {
            if (sidecar == src || ! sidecar.existsAsFile())
                continue;
            const auto dest = target.withFileExtension (sidecar.getFileExtension());
            move ? sidecar.moveFileTo (dest) : sidecar.copyFileTo (dest);
        }

        if (foundry::isSupportedAudioFile (src))
        {
            foundry::LibraryItem it;
            if (auto existing = library.findByPath (p))
                it = *existing;
            else
            {
                it.fileType = "oneshot";
                it.generationType = "source";
                it.dateCreated = juce::Time::currentTimeMillis();
            }
            it.path = target.getFullPathName();
            it.filename = target.getFileName();
            it.dateIndexed = juce::Time::currentTimeMillis();
            it.existsOnDisk = true;

            juce::String err;
            if (move)
            {
                library.removeByPath (p, err);
                thumbnails.forget (p);
            }
            library.upsert (it, err);
        }

        ++done;
        if (! move)
            ++copied;
    }

    return done;
}

bool FoundryAudioProcessor::addUserFolder (const juce::File& dir, juce::String& error)
{
    if (! dir.isDirectory())
    {
        error = "Choose a folder that exists.";
        return false;
    }

    config.get().userFolders.addIfNotAlreadyThere (dir.getFullPathName());
    persistConfig (error);
    error.clear();

    const auto scanErr = startLibraryScan();
    if (scanErr.isNotEmpty())
        error = "Folder added. " + scanErr + " Press RESCAN when it's done to index it.";
    return true;
}

void FoundryAudioProcessor::removeUserFolder (const juce::File& dir)
{
    config.get().userFolders.removeString (dir.getFullPathName());
    juce::String err;
    persistConfig (err);

    // Drop its files from the index unless another library folder covers them.
    for (int i = 0; i < foundry::kNumFolderRoles; ++i)
    {
        const auto roleDir = config.folder ((foundry::FolderRole) i);
        if (roleDir.isDirectory() && (dir == roleDir || dir.isAChildOf (roleDir)))
            return;
    }
    for (const auto& it : library.queryUnderFolder (dir.getFullPathName(), {}, 0))
        library.removeByPath (it.path, err);
}

//==============================================================================
// Riff Maker
void FoundryAudioProcessor::rebuildRiff()
{
    foundry::RiffSketch copy;
    {
        const juce::ScopedLock sl (sketchLock);
        copy = sketch;
    }

    juce::StringArray problems;
    riff.publish (foundry::buildRiffSnapshot (copy, samples, currentSr.load(), &problems));

    const juce::ScopedLock sl (sketchLock);
    sketchProblems = problems;
}

juce::StringArray FoundryAudioProcessor::getSketchProblems() const
{
    const juce::ScopedLock sl (sketchLock);
    return sketchProblems;
}

void FoundryAudioProcessor::setSketch (const foundry::RiffSketch& next, bool recordUndo)
{
    {
        const juce::ScopedLock sl (sketchLock);
        if (recordUndo && next != sketch)
        {
            undoStack.push_back (sketch);
            if (undoStack.size() > 200)
                undoStack.erase (undoStack.begin());
            redoStack.clear();
        }
        sketch = next;
    }

    rebuildRiff();
    sketchChanged.sendChangeMessage();
}

void FoundryAudioProcessor::recordUndoFrom (const foundry::RiffSketch& before)
{
    const juce::ScopedLock sl (sketchLock);
    if (before == sketch)
        return;
    undoStack.push_back (before);
    if (undoStack.size() > 200)
        undoStack.erase (undoStack.begin());
    redoStack.clear();
}

bool FoundryAudioProcessor::undo()
{
    foundry::RiffSketch target;
    {
        const juce::ScopedLock sl (sketchLock);
        if (undoStack.empty())
            return false;
        redoStack.push_back (sketch);
        target = undoStack.back();
        undoStack.pop_back();
    }
    setSketch (target, false);
    return true;
}

bool FoundryAudioProcessor::redo()
{
    foundry::RiffSketch target;
    {
        const juce::ScopedLock sl (sketchLock);
        if (redoStack.empty())
            return false;
        undoStack.push_back (sketch);
        target = redoStack.back();
        redoStack.pop_back();
    }
    setSketch (target, false);
    return true;
}

bool FoundryAudioProcessor::assignSample (int slot, const juce::File& file, juce::String& error)
{
    if (slot < 0 || slot >= foundry::kRiffSlots)
        return false;

    if (! foundry::isSupportedAudioFile (file))
    {
        error = "Foundry can load WAV, AIFF, FLAC or Ogg files: " + file.getFileName();
        return false;
    }

    // Decode now so a broken file is reported on drop rather than playing silence.
    if (samples.get (file, error) == nullptr)
        return false;

    auto next = getSketch();
    auto& s = next.slots[(size_t) slot];
    s.path = file.getFullPathName();
    s.offsetSeconds = 0.0;
    setSketch (next, true);
    return true;
}

bool FoundryAudioProcessor::rollDice (const std::vector<foundry::LibraryItem>& pool, juce::String& message)
{
    std::vector<juce::String> paths;
    for (const auto& it : pool)
        if (it.existsOnDisk && foundry::isSupportedAudioFile (juce::File (it.path)))
            paths.push_back (it.path);

    if (paths.empty())
    {
        message = "Nothing to roll from here - open a folder with samples in the browser first.";
        return false;
    }

    auto next = getSketch();
    int unlocked = 0;
    for (const auto& s : next.slots)
        if (! s.locked)
            ++unlocked;

    if (unlocked == 0)
    {
        message = "Every slot is locked - unlock a slot to let the dice change it.";
        return false;
    }

    static std::atomic<juce::int64> salt { 0 };
    const auto seed = juce::Time::currentTimeMillis() ^ (++salt * (juce::int64) 0x9E3779B97F4A7C15LL);
    juce::Random rng (seed);

    for (auto& s : next.slots)
    {
        if (s.locked)
            continue;

        // A few tries in case the index points at files that have since gone.
        for (int attempt = 0; attempt < 8; ++attempt)
        {
            const auto& candidate = paths[(size_t) rng.nextInt ((int) paths.size())];
            if (juce::File (candidate).existsAsFile())
            {
                s.path = candidate;
                s.offsetSeconds = 0.0;
                break;
            }
        }
    }

    next.lastSeed = seed;
    setSketch (next, true);
    message = "Rolled " + juce::String (unlocked) + (unlocked == 1 ? " slot" : " slots")
            + " from " + juce::String ((int) paths.size()) + " samples.";
    return true;
}

double FoundryAudioProcessor::getRiffBpm() const noexcept
{
    const auto host = hostBpm.load();
    return host > 20.0 && host < 1000.0 ? host : standaloneBpm.load();
}

void FoundryAudioProcessor::setStandaloneBpm (double bpm)
{
    standaloneBpm.store (juce::jlimit (40.0, 300.0, bpm));
    config.get().customBpm = standaloneBpm.load();
}

bool FoundryAudioProcessor::isEditableRiff (const juce::File& file)
{
    return file.hasFileExtension (foundry::kRiffSidecarExtension)
        || foundry::riffSidecarFor (file).existsAsFile();
}

void FoundryAudioProcessor::relinkMissingSamples (foundry::RiffSketch& s) const
{
    // Moving a sample folder shouldn't break every riff that used it: look
    // the file up by name in the library index.
    for (auto& slot : s.slots)
    {
        if (slot.isEmpty() || juce::File (slot.path).existsAsFile())
            continue;
        if (auto found = library.findByFilename (juce::File (slot.path).getFileName()))
            if (juce::File (found->path).existsAsFile())
                slot.path = found->path;
    }
}

bool FoundryAudioProcessor::openRiff (const juce::File& file, juce::String& error)
{
    const auto sidecar = file.hasFileExtension (foundry::kRiffSidecarExtension)
                             ? file : foundry::riffSidecarFor (file);
    if (! sidecar.existsAsFile())
    {
        error = file.getFileName() + " has no saved riff to open (only riffs rendered by Foundry do).";
        return false;
    }

    foundry::RiffSketch loaded;
    if (! foundry::RiffSketch::fromJson (sidecar.loadFileAsString(), loaded))
    {
        error = "Could not read " + sidecar.getFileName();
        return false;
    }

    relinkMissingSamples (loaded);
    setSketch (loaded, true);
    return true;
}

juce::File FoundryAudioProcessor::renderRiff (juce::String& error, bool& reused)
{
    reused = false;
    const auto current = getSketch();

    if (current.isEmpty())
    {
        error = "The riff is empty - drop samples into the slots first.";
        return {};
    }

    const double bpm = getRiffBpm();
    const double sr = config.resolveRenderSampleRate (currentSr.load());
    const auto key = current.toJson() + "|" + juce::String (bpm, 3) + "|" + juce::String (sr, 0);

    if (key == lastRenderKey && lastRenderFile.existsAsFile())
    {
        reused = true;
        return lastRenderFile;
    }

    juce::StringArray problems;
    auto audio = foundry::RiffEngine::renderLoop (current, samples, bpm, sr, &problems);
    if (config.get().renderProtection)
        foundry::AudioIO::protectPeaks (audio);

    const auto dir = config.riffDir();
    const int seq = library.isOpen() ? library.nextSequence ("Riff_") : 1;
    const auto stem = "Riff_" + juce::String (seq).paddedLeft ('0', 4)
                    + "_" + juce::String (juce::roundToInt (bpm)) + "BPM";
    auto file = foundry::AudioIO::uniqueWavPath (dir, stem);

    foundry::AudioIO io;
    if (! io.writeWav (file, audio, sr, 24, error))
        return {};

    foundry::riffSidecarFor (file).replaceWithText (current.toJson());

    foundry::LibraryItem it;
    it.filename = file.getFileName();
    it.path = file.getFullPathName();
    it.fileType = "loop";
    it.category = foundry::Category::Riff;
    it.bpm = bpm;
    it.bars = juce::jmax (1, juce::roundToInt (std::ceil (current.lengthBeats() / 4.0)));
    it.dateCreated = juce::Time::currentTimeMillis();
    it.dateIndexed = it.dateCreated;
    it.generationType = "riff";
    it.generationSeed = current.lastSeed;
    it.patternId = "grid " + foundry::stepsName (current.gridSteps);
    it.engine = "riffmaker";
    juce::String dbErr;
    library.upsert (it, dbErr);

    if (! problems.isEmpty())
        error = "Rendered, but these samples were missing and played as rests: " + problems.joinIntoString (", ");

    lastRenderKey = key;
    lastRenderFile = file;
    return file;
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
    req.usePattern = bassUsePattern;
    req.pattern = bassPattern;
    req.pattern.setBars (uiBars);
    req.locks = bassLocks;
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
    return juce::jmax (0.05, preview.clipLengthSeconds());
}

void FoundryAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const bool rateChanged = std::abs (currentSr.load() - sampleRate) > 0.5;
    currentSr.store (sampleRate);

    // Note tracking does not survive a stop/start, otherwise a note-on the
    // host never matched with a note-off leaves the riff running.
    heldNotes = 0;

    preview.prepare (sampleRate, samplesPerBlock);
    riff.prepare (sampleRate);

    // Slot samples are resampled to the playback rate; a new rate needs a new
    // snapshot or the riff would play back transposed.
    if (rateChanged || ! riffPrepared)
        rebuildRiff();
    riffPrepared = true;

    smoothedPreviewGain.reset (sampleRate, 0.02);
    const float v = params.previewVolume != nullptr ? params.previewVolume->load() * 0.01f : 0.8f;
    smoothedPreviewGain.setCurrentAndTargetValue (v);
    preview.setVolume (v);
    riff.setVolume (v);
}

void FoundryAudioProcessor::releaseResources()
{
    preview.reset();
    riff.stopAll();
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

    // A MIDI note plays the riff from the top for as long as it is held - the
    // note only says when, never how to transpose. Handy for auditioning a
    // riff against a beat straight from the piano roll.
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            ++heldNotes;
            riff.startInternal();
        }
        else if (m.isNoteOff())
        {
            if (heldNotes > 0 && --heldNotes == 0)
                riff.stopInternal();
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            if (heldNotes > 0)
                riff.stopInternal();
            heldNotes = 0;
        }
    }

    if (params.previewVolume != nullptr)
    {
        const float v = params.previewVolume->load() * 0.01f;
        smoothedPreviewGain.setTargetValue (v);
        const float g = smoothedPreviewGain.getNextValue();
        preview.setVolume (g);
        riff.setVolume (g);
        // Advance the rest of the block so the smoother stays in time even
        // though the players take one gain per block.
        for (int i = 1; i < numSamples; ++i)
            smoothedPreviewGain.getNextValue();
    }

    foundry::RiffEngine::HostInfo host;
    host.playing  = transportPlaying;
    host.ppqValid = havePpq;
    host.ppq      = ppq;
    host.bpm      = getRiffBpm();
    riff.process (buffer, host);

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
    state.setProperty ("bassPattern", bassPattern.toJson(), nullptr);
    state.setProperty ("bassUsePattern", bassUsePattern, nullptr);
    {
        juce::StringArray drumLockPaths, bassLockPaths;
        for (auto& l : locks)     drumLockPaths.add (l.locked ? l.path : juce::String());
        for (auto& l : bassLocks) bassLockPaths.add (l.locked ? l.path : juce::String());
        state.setProperty ("drumLocks", drumLockPaths.joinIntoString ("\n"), nullptr);
        state.setProperty ("bassLocks", bassLockPaths.joinIntoString ("\n"), nullptr);
    }
    state.setProperty ("lastTab", config.get().lastTab, nullptr);
    state.setProperty ("followTransport", riff.getFollowHost(), nullptr);
    state.setProperty ("riffSketch", getSketch().toJson(), nullptr);
    state.setProperty ("standaloneBpm", standaloneBpm.load(), nullptr);

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
        config.get().lastTab = (int) state.getProperty ("lastTab", 0) > 0 ? 1 : 0;

        if (auto bj = state.getProperty ("bassPattern").toString(); bj.isNotEmpty())
            bassPattern = foundry::DrumPattern::fromJson (bj, uiBars);
        bassUsePattern = (bool) state.getProperty ("bassUsePattern", false);

        auto restoreLocks = [] (const juce::String& joined, auto& target)
        {
            const auto paths = juce::StringArray::fromLines (joined);
            for (size_t i = 0; i < target.size(); ++i)
            {
                const auto path = (int) i < paths.size() ? paths[(int) i] : juce::String();
                target[i].locked = path.isNotEmpty();
                target[i].path = path;
                target[i].name = juce::File (path).getFileName();
            }
        };
        restoreLocks (state.getProperty ("bassLocks").toString(), bassLocks);
        {
            std::array<foundry::SampleLock, 4> drums {};
            restoreLocks (state.getProperty ("drumLocks").toString(), drums);
            for (size_t i = 0; i < 4; ++i)
                locks[i] = drums[i];
        }

        const auto json = state.getProperty ("drumPattern").toString();
        if (json.isNotEmpty())
            drumPattern = foundry::DrumPattern::fromJson (json, uiBars);

        setFollowTransport ((bool) state.getProperty ("followTransport", riff.getFollowHost()));
        standaloneBpm.store (juce::jlimit (40.0, 300.0,
                                           (double) state.getProperty ("standaloneBpm", standaloneBpm.load())));

        // The riff is part of the session: reopening the project brings back
        // exactly what was in the slots (relinking samples that moved).
        foundry::RiffSketch restored;
        if (foundry::RiffSketch::fromJson (state.getProperty ("riffSketch").toString(), restored))
        {
            relinkMissingSamples (restored);
            {
                const juce::ScopedLock sl (sketchLock);
                undoStack.clear();
                redoStack.clear();
            }
            setSketch (restored, false);
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
