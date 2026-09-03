/*  End-to-end playback test: drives FoundryAudioProcessor the way a host does
    and checks that audio actually comes out.

    This is the test that would have caught "the pattern shows up but pressing
    play makes no sound": every path a user can trigger playback from - the
    PLAY button, a MIDI note, the host transport - is rendered here and the
    output buffer is checked for signal.

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

    /** Minimal host transport so the plugin's follow-transport path can run. */
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

    juce::File writeLoop (const juce::File& dir, double sr, double seconds)
    {
        foundry::AudioIO io;
        const int n = juce::roundToInt (sr * seconds);
        juce::AudioBuffer<float> b (2, n);

        for (int i = 0; i < n; ++i)
        {
            const auto s = (float) (0.5 * std::sin (juce::MathConstants<double>::twoPi
                                                    * 220.0 * (double) i / sr));
            b.setSample (0, i, s);
            b.setSample (1, i, s);
        }

        auto f = dir.getChildFile ("loop.wav");
        juce::String err;
        expect (io.writeWav (f, b, sr, 24, err), "write test loop: " + err);
        return f;
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
    auto loopFile = writeLoop (dir, sr, 1.0);
    auto alignFile = writeLoop (dir.getChildFile ("align"), sr, 1.0);

    FoundryAudioProcessor proc;
    proc.prepareToPlay (sr, blockSize);

    // A generator: MIDI in, no audio in, audio out.
    expect (proc.acceptsMidi(), "accepts MIDI so the piano roll can trigger it");
    expect (proc.getTotalNumInputChannels() == 0, "no audio input bus on a generator");
    expect (proc.getTotalNumOutputChannels() == 2, "stereo output");

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    buffer.clear();

    // Silence before anything is loaded, and no stale host buffer passed through.
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < blockSize; ++i)
            buffer.setSample (ch, i, 0.9f);
    expect (renderPeak (proc, buffer, midi, 1) == 0.0f,
            "idle generator outputs silence, not the host's buffer contents");

    // 1. Loading a clip starts it - this is what clicking a browser row does.
    juce::String err;
    expect (proc.previewFile (loopFile, true, err), "load loop: " + err);
    expect (proc.getPreview().hasClip(), "clip is loaded");
    expect (renderPeak (proc, buffer, midi, 8) > 0.05f, "audio after loading a loop");

    // 2. STOP then PLAY - the reported bug. PLAY is gated on a decoded clip,
    //    so it works whether or not the source file is still on disk.
    proc.getPreview().stop();
    renderPeak (proc, buffer, midi, 4);            // let the release ramp finish
    expect (! proc.getPreview().isPlaying(), "stop halts playback");

    proc.getPreview().retrigger();
    expect (renderPeak (proc, buffer, midi, 8) > 0.05f, "PLAY restarts a loaded loop");

    proc.getPreview().stop();
    renderPeak (proc, buffer, midi, 4);
    loopFile.deleteFile();
    proc.getPreview().retrigger();
    expect (renderPeak (proc, buffer, midi, 8) > 0.05f,
            "PLAY still works when the source file has been removed");

    // 3. A MIDI note plays the loaded loop; the note chosen does not transpose it.
    proc.getPreview().stop();
    renderPeak (proc, buffer, midi, 4);

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

    expect (peaks[0] > 0.05f, "MIDI note-on plays the loop");
    expect (peaks[1] > 0.05f, "a note two octaves up plays the same loop");
    expect (std::abs (peaks[0] - peaks[1]) < 0.02f,
            "playback level is the same whichever note is pressed");
    expect (! proc.getPreview().isPlaying(), "note-off stops the loop");

    // 4. The host transport starts and stops playback when SYNC is on.
    TestPlayHead playHead;
    proc.setPlayHead (&playHead);
    proc.setFollowTransport (true);

    playHead.playing = true;
    playHead.ppq = 0.0;
    expect (renderPeak (proc, buffer, midi, 8) > 0.05f, "host transport starts the loop");

    playHead.playing = false;
    renderPeak (proc, buffer, midi, 8);
    expect (! proc.getPreview().isPlaying(), "host transport stop halts the loop");

    // Starting mid-timeline folds into the loop rather than restarting it.
    // Arm a clip first, so a freshly loaded loop's pending restart cannot
    // rewind the alignment to zero.
    expect (proc.getPreview().load (alignFile, true, err), "arm loop for alignment: " + err);
    expect (! proc.getPreview().isPlaying(), "load() arms without starting");

    playHead.playing = true;
    playHead.ppq = 12.0;                            // three bars in at 4/4
    proc.processBlock (buffer, midi);

    {
        const auto len = proc.getPreview().clipLengthSamples();
        const auto elapsed = (juce::int64) (playHead.ppq * (60.0 / playHead.bpm) * sr);
        const auto expected = (int) (elapsed % (juce::int64) len);

        // One block has been rendered since the alignment.
        const auto actual = proc.getPreview().positionSamples() - blockSize;
        expect (std::abs (actual - expected) < 64,
                "transport start mid-song folds into the loop (position " + juce::String (actual)
                    + ", expected about " + juce::String (expected) + ")");
    }

    expect (renderPeak (proc, buffer, midi, 4) > 0.05f, "transport start mid-song plays");

    playHead.playing = false;
    renderPeak (proc, buffer, midi, 8);

    // With SYNC off the transport is ignored.
    proc.setFollowTransport (false);
    playHead.playing = true;
    playHead.ppq = 0.0;
    expect (renderPeak (proc, buffer, midi, 8) == 0.0f, "SYNC off ignores the transport");

    proc.setPlayHead (nullptr);

    // 5. A sample-rate change re-decodes the clip, so the loop keeps its
    //    original speed and pitch instead of playing back transposed.
    auto rateFile = writeLoop (dir.getChildFile ("rate"), sr, 1.0);
    expect (proc.previewFile (rateFile, true, err), "load loop for rate change: " + err);

    const int lengthAt48k = proc.getPreview().clipLengthSamples();
    expect (lengthAt48k > 0, "clip length known at 48 kHz");

    proc.prepareToPlay (96000.0, blockSize);
    const int lengthAt96k = proc.getPreview().clipLengthSamples();
    expect (std::abs (lengthAt96k - lengthAt48k * 2) < 64,
            "clip is re-decoded at the new rate (got " + juce::String (lengthAt96k)
                + ", expected about " + juce::String (lengthAt48k * 2) + ")");

    proc.prepareToPlay (sr, blockSize);
    expect (std::abs (proc.getPreview().clipLengthSamples() - lengthAt48k) < 64,
            "clip returns to its original length when the rate goes back");

    proc.getPreview().retrigger();
    expect (renderPeak (proc, buffer, midi, 8) > 0.05f, "still plays after rate changes");

    // 6. Session state carries the armed loop and the SYNC setting.
    proc.setFollowTransport (false);
    juce::MemoryBlock saved;
    proc.getStateInformation (saved);

    {
        FoundryAudioProcessor restored;
        restored.prepareToPlay (sr, blockSize);
        restored.setStateInformation (saved.getData(), (int) saved.getSize());

        expect (! restored.getFollowTransport(), "SYNC setting survives a session reload");
        expect (restored.getPreview().hasClip(), "the armed loop survives a session reload");
        expect (! restored.getPreview().isPlaying(), "a restored session does not start playing on its own");

        juce::AudioBuffer<float> b2 (2, blockSize);
        juce::MidiBuffer m2;
        b2.clear();
        restored.getPreview().retrigger();
        expect (renderPeak (restored, b2, m2, 8) > 0.05f, "PLAY works right after a session reload");
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
