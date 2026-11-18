#pragma once

#include "Params/ParameterLayout.h"
#include "Core/ConfigStore.h"
#include "Core/JobWorker.h"
#include "Core/LibraryDatabase.h"
#include "Core/PreviewEngine.h"
#include "Core/BrowserModel.h"
#include "Core/RiffEngine.h"
#include "Core/ThumbnailCache.h"

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
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }

    /** Foundry is a generator: it has no input and its output outlives any one
        block (a previewed loop, the end of a slice), so report a tail. Hosts
        use it to decide how long to keep a silent instrument running. */
    double getTailLengthSeconds() const override;

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

    foundry::DrumPattern&       getBassPattern() noexcept { return bassPattern; }
    foundry::SampleLock&        getBassLock (int row) noexcept { return bassLocks[(size_t) juce::jlimit (0, 3, row)]; }
    bool bassUsePattern = false;

    //== patterns ============================================================
    /** Reads a .foundrypattern file, or the pattern saved next to a loop. */
    bool loadPattern (const juce::File& file, foundry::PatternFile& out, juce::String& error) const;
    static bool hasPattern (const juce::File& file);

    /** Saves into the pattern library (Patterns/Drums or Patterns/Bass). */
    juce::File savePattern (const foundry::PatternFile& pattern, const juce::String& name, juce::String& error);
    juce::Array<juce::File> listPatterns (foundry::PatternKind kind) const;

    //== file management =====================================================
    /** True for files Foundry made: batch output, rendered riffs, patterns.
        Only these can be deleted from the browser - never your own samples. */
    bool isManagedFile (const juce::File& f) const;

    /** Deletes managed files (with their sidecars) and unindexes them.
        Returns how many were deleted; unmanaged paths are skipped. */
    int deleteFiles (const juce::StringArray& paths, int& skipped);

    /** Moves Foundry-made files into `dir`, copies everything else (your own
        samples are never moved), and indexes the results. */
    int moveOrCopyFiles (const juce::StringArray& paths, const juce::File& dir, int& copied, juce::String& error);

    bool addUserFolder (const juce::File& dir, juce::String& error);
    void removeUserFolder (const juce::File& dir);

    double getHostBpm() const noexcept { return hostBpm.load(); }
    double getSampleRate() const noexcept { return currentSr.load(); }

    /** `preferencesOnly` merges just this instance's last-used values into the
        config on disk (automatic saves); a full save also writes library
        folders and is only for the Settings page. */
    bool persistConfig (juce::String& error, bool preferencesOnly = false);
    bool reopenDatabase (juce::String& error);

    bool previewItem (const foundry::LibraryItem& item, juce::String& error);
    bool previewFile (const juce::File& file, bool loop, juce::String& error);

    /** Arms the most recently generated loop so the transport always has
        something to play once a batch finishes. */
    bool loadNewestGenerated (juce::String& error);

    /** SYNC: the riff plays along with the host transport, locked to its bar. */
    bool getFollowTransport() const noexcept { return riff.getFollowHost(); }
    void setFollowTransport (bool shouldFollow);

    //== Riff Maker ===========================================================
    const foundry::RiffSketch& getSketch() const noexcept { return sketch; }

    /** Replaces the riff. `recordUndo` adds an undo step; continuous gestures
        (dragging a slice's start) pass false while dragging and call
        recordUndoFrom() with the state they started from when they end. */
    void setSketch (const foundry::RiffSketch& next, bool recordUndo);
    void recordUndoFrom (const foundry::RiffSketch& before);
    bool undo();
    bool redo();
    bool canUndo() const noexcept { return ! undoStack.empty(); }
    bool canRedo() const noexcept { return ! redoStack.empty(); }

    /** Turns a slot into silence (the Delete key). If an earlier sample was
        still ringing into it, that sample is shortened so the slot really is
        silent. Its lock stays, so a locked silence survives the dice. */
    void silenceSlot (int slot);

    /** Drops a sample into a slot (start offset resets; length, gain and lock stay). */
    bool assignSample (int slot, const juce::File& file, juce::String& error);

    /** Fills every unlocked slot with a random sample from `pool`. */
    bool rollDice (const std::vector<foundry::LibraryItem>& pool, juce::String& message);

    /** Renders the riff as one seamless loop into the Riffs folder (with an
        editable .foundryriff next to it) and indexes it. If nothing changed
        since the last render the existing file is returned and `reused` set. */
    juce::File renderRiff (juce::String& error, bool& reused);

    /** Opens a rendered riff (its WAV or its .foundryriff) back in the editor. */
    bool openRiff (const juce::File& file, juce::String& error);
    static bool isEditableRiff (const juce::File& file);

    /** Tempo the riff plays at: the host's when it reports one, else riffBpm. */
    double getRiffBpm() const noexcept;
    void   setStandaloneBpm (double bpm);
    double getStandaloneBpm() const noexcept { return standaloneBpm.load(); }
    bool   hostProvidesTempo() const noexcept { return hostBpm.load() > 20.0; }

    juce::StringArray getSketchProblems() const;

    foundry::RiffEngine&     getRiff()       noexcept { return riff; }
    foundry::SampleCache&    getSamples()    noexcept { return samples; }
    foundry::ThumbnailCache& getThumbnails() noexcept { return thumbnails; }
    foundry::BrowserModel&   getBrowser()    noexcept { return browser; }

    /** Fires (on the message thread) whenever the riff changes for any reason. */
    juce::ChangeBroadcaster sketchChanged;

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

    void rebuildRiff();
    void relinkMissingSamples (foundry::RiffSketch& s) const;

    foundry::SampleCache samples;
    foundry::RiffEngine riff;
    foundry::ThumbnailCache thumbnails { library };
    foundry::BrowserModel browser { config, library };

    foundry::RiffSketch sketch;
    std::vector<foundry::RiffSketch> undoStack, redoStack;
    juce::StringArray sketchProblems;
    mutable juce::CriticalSection sketchLock;   // prepareToPlay may run off the message thread
    std::atomic<double> standaloneBpm { 140.0 };
    juce::String lastRenderKey;
    juce::File lastRenderFile;

    void migrateLegacyPatterns();

    foundry::DrumPattern drumPattern;
    foundry::DrumPattern bassPattern;
    std::array<foundry::SampleLock, 4> bassLocks {};
    foundry::SampleLock locks[4];
    foundry::Genre uiGenre = foundry::Genre::Riddim;
    int uiBars = 2;
    int uiQuantity = 10;

    std::atomic<double> hostBpm { 0.0 };
    std::atomic<double> currentSr { 48000.0 };

    int  heldNotes = 0;
    bool riffPrepared = false;

    juce::SmoothedValue<float> smoothedPreviewGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FoundryAudioProcessor)
};
