/*  End-to-end playback test: drives FoundryAudioProcessor the way a host does
    and checks that audio actually comes out of every path a user can trigger:

      - clicking a sample in the browser (preview), STOP and PLAY again
      - a MIDI note playing the riff (and its pitch not mattering)
      - the host transport with SYNC on / off, including starting mid-riff
      - a sample-rate change
      - undo / redo, the dice, render-to-WAV and reopening a rendered riff
      - session save / restore of the riff

    Build with -DFOUNDRY_BUILD_TESTS=ON, then run FoundryPlaybackTest. */

#include "../Source/PluginProcessor.h"
#include <iostream>

namespace
{
    int gFails = 0;

    void expect (bool cond, const juce::String& msg)
    {
        if (! cond)
        {
            std::cerr << "FAIL: " << msg << "\n";
            ++gFails;
        }
    }

    float magnitudeOf (const juce::AudioBuffer<float>& b)
    {
        float m = 0.0f;
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            m = juce::jmax (m, b.getMagnitude (ch, 0, b.getNumSamples()));
        return m;
    }

    bool isFinite (const juce::AudioBuffer<float>& b)
    {
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i)
                if (! std::isfinite (b.getSample (ch, i)))
                    return false;
        return true;
    }

    /** Renders `blocks` blocks and returns the loudest sample seen. */
    float renderPeak (FoundryAudioProcessor& proc, juce::AudioBuffer<float>& buffer,
                      juce::MidiBuffer& midi, int blocks)
    {
        float peak = 0.0f;
        for (int i = 0; i < blocks; ++i)
        {
            proc.processBlock (buffer, midi);
            expect (isFinite (buffer), "output is finite");
            peak = juce::jmax (peak, magnitudeOf (buffer));
            midi.clear();
        }
        return peak;
    }

    /** Minimal host transport. */
    struct TestPlayHead : juce::AudioPlayHead
    {
        bool playing = false;
        double ppq = 0.0;
        double bpm = 145.0;

        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setIsPlaying (playing);
            info.setBpm (bpm);
            info.setPpqPosition (ppq);
            return info;
        }
    };

    juce::File writeTone (const juce::File& file, double sr, double seconds, double freq = 220.0)
    {
        foundry::AudioIO io;
        const int n = juce::roundToInt (sr * seconds);
        juce::AudioBuffer<float> b (2, n);

        for (int i = 0; i < n; ++i)
        {
            const auto s = (float) (0.5 * std::sin (juce::MathConstants<double>::twoPi * freq * (double) i / sr));
            b.setSample (0, i, s);
            b.setSample (1, i, s);
        }

        juce::String err;
        expect (io.writeWav (file, b, sr, 24, err), "write test tone: " + err);
        return file;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                   .getChildFile ("FoundryPlaybackTest_" + juce::String (juce::Time::currentTimeMillis()));
    dir.createDirectory();

    const double sr = 48000.0;
    const int blockSize = 512;
    auto loopFile  = writeTone (dir.getChildFile ("loop.wav"), sr, 1.0);
    auto toneA     = writeTone (dir.getChildFile ("toneA.wav"), sr, 1.0, 110.0);
    auto toneB     = writeTone (dir.getChildFile ("toneB.wav"), sr, 1.0, 165.0);

    FoundryAudioProcessor proc;
    proc.prepareToPlay (sr, blockSize);

    // Point the library somewhere disposable so renders don't touch real folders.
    proc.getConfig().setRoot (dir);
    juce::String err;
    expect (proc.getConfig().createRecommendedStructure (err), "library structure: " + err);
    expect (proc.reopenDatabase (err), "open database: " + err);

    // A generator: MIDI in, no audio in, audio out.
    expect (proc.acceptsMidi(), "accepts MIDI so the piano roll can trigger it");
    expect (proc.getTotalNumInputChannels() == 0, "no audio input bus on a generator");
    expect (proc.getTotalNumOutputChannels() == 2, "stereo output");

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;

    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < blockSize; ++i)
            buffer.setSample (ch, i, 0.9f);
    expect (renderPeak (proc, buffer, midi, 1) == 0.0f,
            "idle generator outputs silence, not the host's buffer contents");

    // 1. Preview: clicking a browser tile plays the sample; STOP / PLAY work,
    //    even after the file is gone.
    expect (proc.previewFile (loopFile, true, err), "preview loop: " + err);
    expect (renderPeak (proc, buffer, midi, 8) > 0.05f, "audio after clicking a sample");

    proc.getPreview().stop();
    renderPeak (proc, buffer, midi, 4);
    expect (! proc.getPreview().isPlaying(), "stop halts the preview");

    loopFile.deleteFile();
    proc.getPreview().retrigger();
    expect (renderPeak (proc, buffer, midi, 8) > 0.05f, "preview PLAY still works after the file is removed");
    proc.getPreview().stop();
    renderPeak (proc, buffer, midi, 6);

    // 2. Build a riff by dropping samples into slots.
    expect (proc.assignSample (0, toneA, err), "drop sample into slot 1: " + err);
    expect (proc.assignSample (2, toneB, err), "drop sample into slot 3: " + err);
    expect (! proc.assignSample (1, dir.getChildFile ("nope.wav"), err), "dropping a missing file is refused");
    expect (proc.getSketch().slots[0].path == toneA.getFullPathName(), "slot 1 holds the sample");
    expect (proc.getSketch().slots[0].lengthSteps == 4, "a new slot lasts a quarter note by default");

    // 3. A MIDI note plays the riff; which note doesn't matter.
    proc.setFollowTransport (false);
    float peaks[2] = { 0.0f, 0.0f };
    const int notes[2] = { 36, 84 };

    for (int n = 0; n < 2; ++n)
    {
        midi.clear();
        midi.addEvent (juce::MidiMessage::noteOn (1, notes[n], 1.0f), 0);
        peaks[n] = renderPeak (proc, buffer, midi, 8);

        midi.clear();
        midi.addEvent (juce::MidiMessage::noteOff (1, notes[n]), 0);
        renderPeak (proc, buffer, midi, 4);
    }

    expect (peaks[0] > 0.05f, "MIDI note-on plays the riff");
    expect (std::abs (peaks[0] - peaks[1]) < 0.02f, "playback is the same whichever note is pressed");
    expect (renderPeak (proc, buffer, midi, 2) == 0.0f, "note-off stops the riff");

    // 4. Host transport with SYNC on.
    TestPlayHead playHead;
    proc.setPlayHead (&playHead);
    proc.setFollowTransport (true);

    playHead.playing = true;
    playHead.ppq = 0.0;
    expect (renderPeak (proc, buffer, midi, 4) > 0.05f, "host transport plays the riff");

    // Starting in the middle of slot 3 (beat 3) picks it up immediately.
    playHead.playing = false;
    renderPeak (proc, buffer, midi, 4);
    playHead.playing = true;
    playHead.ppq = 2.4;
    proc.processBlock (buffer, midi);
    expect (magnitudeOf (buffer) > 0.05f, "starting the host mid-slot plays that slot straight away");
    expect (proc.getRiff().getCurrentSlot() == 2, "the riff reports slot 3 as current");

    // Beat 2 is empty and slot 1 is only a quarter long: silence there.
    playHead.ppq = 1.1;
    renderPeak (proc, buffer, midi, 1);           // the jump fades out what was playing
    playHead.ppq = 1.3;
    expect (renderPeak (proc, buffer, midi, 1) == 0.0f, "an empty slot is a rest");

    playHead.playing = false;
    renderPeak (proc, buffer, midi, 2);
    expect (renderPeak (proc, buffer, midi, 2) == 0.0f, "host stop silences the riff");

    proc.setFollowTransport (false);
    playHead.playing = true;
    playHead.ppq = 0.0;
    expect (renderPeak (proc, buffer, midi, 8) == 0.0f, "SYNC off ignores the transport");
    playHead.playing = false;

    // 5. Tempo comes from the host.
    expect (std::abs (proc.getRiffBpm() - playHead.bpm) < 0.01, "riff tempo follows the host");

    proc.setPlayHead (nullptr);

    // 6. A sample-rate change rebuilds the riff at the new rate.
    proc.prepareToPlay (96000.0, blockSize);
    proc.getRiff().startInternal();
    expect (renderPeak (proc, buffer, midi, 8) > 0.05f, "riff plays after a sample-rate change");
    proc.getRiff().stopAll();
    proc.prepareToPlay (sr, blockSize);

    // 7. Undo / redo.
    {
        const auto before = proc.getSketch();
        auto next = before;
        next.slots[0].lengthSteps = 2;
        proc.setSketch (next, true);
        expect (proc.canUndo(), "an edit can be undone");
        expect (proc.undo() && proc.getSketch() == before, "undo restores the riff");
        expect (proc.redo() && proc.getSketch().slots[0].lengthSteps == 2, "redo re-applies the edit");
    }

    // 8. The dice fills unlocked slots and leaves locked ones alone.
    {
        auto next = proc.getSketch();
        next.slots[0].locked = true;
        proc.setSketch (next, true);

        std::vector<foundry::LibraryItem> pool;
        for (auto f : { toneB })
        {
            foundry::LibraryItem it;
            it.path = f.getFullPathName();
            pool.push_back (it);
        }

        juce::String msg;
        expect (proc.rollDice (pool, msg), "dice rolls: " + msg);
        const auto& s = proc.getSketch();
        expect (s.slots[0].path == toneA.getFullPathName(), "a locked slot keeps its sample");
        bool allFilled = true;
        for (int i = 1; i < foundry::kRiffSlots; ++i)
            allFilled = allFilled && s.slots[(size_t) i].path == toneB.getFullPathName();
        expect (allFilled, "every unlocked slot gets a sample from the pool");
        expect (proc.undo() && proc.getSketch().slots[1].isEmpty(), "a dice roll can be undone");
    }

    // 9. Render to WAV, reuse when unchanged, reopen for editing.
    {
        bool reused = false;
        auto file = proc.renderRiff (err, reused);
        expect (file.existsAsFile(), "render writes a WAV: " + err);
        expect (! reused, "first render is new");
        expect (file.getParentDirectory() == proc.getConfig().riffDir(), "renders land in My Riffs");
        expect (foundry::riffSidecarFor (file).existsAsFile(), "an editable riff file is saved next to it");
        expect (proc.getLibrary().findByPath (file.getFullPathName()).has_value(), "the render is indexed straight away");

        auto again = proc.renderRiff (err, reused);
        expect (reused && again == file, "rendering an unchanged riff reuses the file");

        foundry::AudioIO io;
        juce::AudioBuffer<float> rendered;
        double renderedSr = 0.0;
        expect (io.loadWav (file, rendered, renderedSr, err), "rendered WAV decodes");
        const double expectedLen = proc.getSketch().lengthBeats() * 60.0 / proc.getRiffBpm() * renderedSr;
        expect (std::abs (rendered.getNumSamples() - expectedLen) < 4.0, "rendered riff is exactly the riff's length");
        expect (rendered.getMagnitude (0, 0, rendered.getNumSamples()) > 0.05f, "rendered riff has audio");

        const auto saved = proc.getSketch();
        auto cleared = saved;
        for (auto& s : cleared.slots)
            s.path.clear();
        proc.setSketch (cleared, true);
        expect (proc.openRiff (file, err), "reopen a rendered riff: " + err);
        expect (proc.getSketch() == saved, "reopening brings back the exact riff");
        expect (FoundryAudioProcessor::isEditableRiff (file), "rendered riffs are editable");
    }

    // 10. File management: only Foundry's own files can be deleted or moved.
    {
        bool reused = false;
        auto next = proc.getSketch();
        next.slots[5].lengthSteps = 3;               // make it a new riff
        proc.setSketch (next, true);
        auto riffWav = proc.renderRiff (err, reused);
        expect (riffWav.existsAsFile(), "second render: " + err);

        expect (proc.isManagedFile (riffWav), "rendered riffs are Foundry's own files");
        expect (! proc.isManagedFile (toneA), "your own samples are not");

        // Move a render into one of your own folders, copy your own sample.
        auto keepers = dir.getChildFile ("Keepers");
        keepers.createDirectory();
        int copied = 0;
        const int n = proc.moveOrCopyFiles ({ riffWav.getFullPathName(), toneA.getFullPathName() }, keepers, copied, err);
        expect (n == 2 && copied == 1, "one file moved, one copied: " + err);
        const auto movedRiff = keepers.getChildFile (riffWav.getFileName());
        expect (movedRiff.existsAsFile() && ! riffWav.existsAsFile(), "a Foundry file is moved");
        expect (foundry::riffSidecarFor (movedRiff).existsAsFile(), "its editable riff travels with it");
        expect (toneA.existsAsFile() && keepers.getChildFile ("toneA.wav").existsAsFile(), "your own sample is copied, never moved");
        expect (proc.getLibrary().findByPath (movedRiff.getFullPathName()).has_value()
                    && ! proc.getLibrary().findByPath (riffWav.getFullPathName()).has_value(),
                "the index follows the move");

        // Batch delete skips anything that isn't Foundry's.
        auto doomed = proc.renderRiff (err, reused);      // same riff again -> reused? no: the old file moved
        expect (doomed.existsAsFile(), "render to delete");
        int skipped = 0;
        const int deleted = proc.deleteFiles ({ doomed.getFullPathName(), toneB.getFullPathName() }, skipped);
        expect (deleted == 1 && skipped == 1, "delete removes Foundry files and skips your own");
        expect (! doomed.existsAsFile() && ! foundry::riffSidecarFor (doomed).existsAsFile(), "deleted with its sidecar");
        expect (toneB.existsAsFile(), "your own sample survives a batch delete");
        expect (! proc.getLibrary().findByPath (doomed.getFullPathName()).has_value(), "deleted files leave the index");
    }

    // 11. Patterns: save to the library, list, load back.
    {
        foundry::PatternFile pf;
        pf.kind = foundry::PatternKind::Bass;
        pf.grid.setBars (1);
        pf.grid.clear();
        pf.grid.setStep (1, 6, true);
        const auto f = proc.savePattern (pf, "Wobble Groove", err);
        expect (f.existsAsFile(), "pattern saved: " + err);
        expect (f.getParentDirectory() == proc.getConfig().patternDir (true), "bass patterns go to Patterns/Bass");
        expect (proc.listPatterns (foundry::PatternKind::Bass).contains (f), "saved pattern is listed");
        foundry::PatternFile back;
        expect (proc.loadPattern (f, back, err) && back.grid.getStep (1, 6), "pattern loads back");
        expect (proc.isManagedFile (f), "patterns are Foundry files (deletable)");
        expect (FoundryAudioProcessor::hasPattern (f), "a pattern file counts as having a pattern");
    }

    // 12. The session carries the riff and the SYNC setting.
    proc.setFollowTransport (false);
    juce::MemoryBlock state;
    proc.getStateInformation (state);

    {
        FoundryAudioProcessor restored;
        restored.prepareToPlay (sr, blockSize);
        restored.setStateInformation (state.getData(), (int) state.getSize());

        expect (! restored.getFollowTransport(), "SYNC setting survives a session reload");
        expect (restored.getSketch() == proc.getSketch(), "the riff survives a session reload");
        expect (! restored.getRiff().isRunning(), "a restored session does not start playing on its own");

        juce::AudioBuffer<float> b2 (2, blockSize);
        juce::MidiBuffer m2;
        m2.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);
        expect (renderPeak (restored, b2, m2, 8) > 0.05f, "the restored riff plays");
        restored.releaseResources();
    }

    proc.releaseResources();
    dir.deleteRecursively();

    if (gFails > 0)
    {
        std::cerr << "PlaybackTest: " << gFails << " failure(s).\n";
        return 1;
    }

    std::cout << "PlaybackTest: all checks passed.\n";
    return 0;
}
