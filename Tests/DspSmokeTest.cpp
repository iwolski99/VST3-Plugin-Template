/*  Offline DSP smoke test: runs the generator through every output mode at
    several sample rates and block sizes, checks fade-in starts near silence,
    checks binaural left/right actually differ, checks a timed playback goes
    quiet after duration, and round-trips a short WAV export.

    Build with -DTEMPLATE_BUILD_TESTS=ON, then run the TemplateDspTest binary.
    Exit code 0 means clean; non-zero means a failure was printed. */

#include "../Source/PluginProcessor.h"
#include "../Source/DSP/ToneRenderer.h"
#include "../Source/DSP/WavExporter.h"
#include <iostream>

namespace
{
    int fail (const juce::String& msg)
    {
        std::cerr << "FAIL: " << msg << "\n";
        return 1;
    }

    bool isFinite (float x) noexcept
    {
        return std::isfinite (x);
    }

    int checkBufferFinite (const juce::AudioBuffer<float>& buffer, const char* tag)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* d = buffer.getReadPointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                if (! isFinite (d[i]) || std::abs (d[i]) > 4.0f)
                    return fail (juce::String ("non-finite or wild sample (") + tag
                                 + ") ch " + juce::String (ch) + " = " + juce::String (d[i]));
        }
        return 0;
    }

    float peak (const juce::AudioBuffer<float>& buffer, int start, int count)
    {
        float m = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* d = buffer.getReadPointer (ch);
            for (int i = 0; i < count; ++i)
                m = juce::jmax (m, std::abs (d[start + i]));
        }
        return m;
    }

    float channelDiff (const juce::AudioBuffer<float>& buffer)
    {
        if (buffer.getNumChannels() < 2)
            return 0.0f;

        float acc = 0.0f;
        auto* l = buffer.getReadPointer (0);
        auto* r = buffer.getReadPointer (1);
        const int n = buffer.getNumSamples();
        for (int i = 0; i < n; ++i)
            acc += std::abs (l[i] - r[i]);
        return acc / (float) juce::jmax (1, n);
    }

    int runProcessorPass (double sampleRate, int blockSize, int mode, bool startPlay)
    {
        BinauralBeatsAudioProcessor proc;
        proc.prepareToPlay (sampleRate, blockSize);

        auto& apvts = proc.getAPVTS();
        apvts.getParameter (pt::pid::mode)->setValueNotifyingHost (
            apvts.getParameter (pt::pid::mode)->convertTo0to1 ((float) mode));
        apvts.getParameter (pt::pid::duration)->setValueNotifyingHost (
            apvts.getParameter (pt::pid::duration)->convertTo0to1 (0.5f));
        apvts.getParameter (pt::pid::fadeIn)->setValueNotifyingHost (
            apvts.getParameter (pt::pid::fadeIn)->convertTo0to1 (0.05f));
        apvts.getParameter (pt::pid::fadeOut)->setValueNotifyingHost (
            apvts.getParameter (pt::pid::fadeOut)->convertTo0to1 (0.05f));
        apvts.getParameter (pt::pid::mix)->setValueNotifyingHost (1.0f);

        if (startPlay)
            proc.startPlayback();

        juce::AudioBuffer<float> buffer (2, blockSize);
        juce::MidiBuffer midi;
        int failures = 0;

        for (int b = 0; b < 12; ++b)
        {
            buffer.clear();
            proc.processBlock (buffer, midi);
            failures += checkBufferFinite (buffer, "processBlock");
            if (failures)
                return failures;
        }

        proc.releaseResources();
        return failures;
    }

    int testEnvelopeAndStereo()
    {
        pt::RenderSpec spec;
        spec.sampleRate  = 48000.0;
        spec.leftHz      = 200.0f;
        spec.rightHz     = 210.0f;
        spec.durationSec = 0.4f;
        spec.fadeInSec   = 0.1f;
        spec.fadeOutSec  = 0.1f;
        spec.mode        = pt::OutputMode::binaural;
        spec.gain        = 0.35f;

        const int n = (int) std::llround (spec.durationSec * spec.sampleRate);
        juce::AudioBuffer<float> buffer (2, n);
        pt::ToneRenderer r;
        r.prepare (spec.sampleRate);
        r.setSpec (spec);
        r.render (buffer, 2);

        if (auto f = checkBufferFinite (buffer, "binaural-render"))
            return f;

        const int fadeSamples = (int) (0.1f * spec.sampleRate);
        if (peak (buffer, 0, 8) > 0.02f)
            return fail ("fade-in should start near silence");

        if (peak (buffer, fadeSamples, 64) < 0.05f)
            return fail ("steady-state binaural should be audible");

        if (channelDiff (buffer) < 0.01f)
            return fail ("binaural left and right should differ");

        if (peak (buffer, n - 4, 4) > 0.05f)
            return fail ("fade-out should end near silence");

        // Pure tone: channels must match.
        spec.mode = pt::OutputMode::pureTone;
        r.prepare (spec.sampleRate);
        r.setSpec (spec);
        r.reset();
        r.render (buffer, 2);

        if (channelDiff (buffer) > 1.0e-6f)
            return fail ("pure tone left and right should match");

        return 0;
    }

    int testPlaybackStops()
    {
        BinauralBeatsAudioProcessor proc;
        proc.prepareToPlay (48000.0, 512);

        auto& apvts = proc.getAPVTS();
        apvts.getParameter (pt::pid::duration)->setValueNotifyingHost (
            apvts.getParameter (pt::pid::duration)->convertTo0to1 (0.5f));
        apvts.getParameter (pt::pid::mix)->setValueNotifyingHost (1.0f);
        apvts.getParameter (pt::pid::fadeIn)->setValueNotifyingHost (0.0f);
        apvts.getParameter (pt::pid::fadeOut)->setValueNotifyingHost (0.0f);

        proc.startPlayback();

        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        float maxDuring = 0.0f, maxAfter = 0.0f;

        const int blocksForOneSec = (int) std::ceil (48000.0 / 512.0);
        for (int b = 0; b < blocksForOneSec; ++b)
        {
            buffer.clear();
            proc.processBlock (buffer, midi);
            const float p = peak (buffer, 0, buffer.getNumSamples());
            if (b < 20)
                maxDuring = juce::jmax (maxDuring, p);
            if (b > blocksForOneSec / 2)
                maxAfter = juce::jmax (maxAfter, p);
        }

        if (maxDuring < 0.02f)
            return fail ("playback produced near-silence while it should have been playing");
        if (maxAfter > 1.0e-4f)
            return fail ("generator kept producing after duration elapsed");
        if (proc.isPlaying())
            return fail ("processor still reports playing after duration");

        return 0;
    }

    int testWavExport()
    {
        pt::RenderSpec spec;
        spec.sampleRate  = 44100.0;
        spec.leftHz      = 200.0f;
        spec.rightHz     = 210.0f;
        spec.durationSec = 0.25f;
        spec.fadeInSec   = 0.02f;
        spec.fadeOutSec  = 0.02f;
        spec.mode        = pt::OutputMode::binaural;

        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getChildFile ("BinauralBeatsTest");
        dir.createDirectory();
        const auto stereo = dir.getChildFile ("binaural.wav");
        const auto mono   = dir.getChildFile ("tone.wav");

        juce::String error;
        if (! pt::renderToWav (stereo, spec, pt::ExportKind::binaural, error))
            return fail ("binaural wav: " + error);
        if (! pt::renderToWav (mono, spec, pt::ExportKind::pureTone, error))
            return fail ("tone wav: " + error);

        juce::AudioFormatManager formats;
        formats.registerBasicFormats();

        auto checkFile = [&] (const juce::File& f, int expectedCh) -> int
        {
            std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (f));
            if (reader == nullptr)
                return fail ("could not read " + f.getFileName());
            if ((int) reader->numChannels != expectedCh)
                return fail (f.getFileName() + " channel count");
            const int expected = (int) std::llround (spec.durationSec * spec.sampleRate);
            if (std::abs ((int) reader->lengthInSamples - expected) > 2)
                return fail (f.getFileName() + " length " + juce::String ((int) reader->lengthInSamples)
                             + " expected " + juce::String (expected));
            return 0;
        };

        if (auto f = checkFile (stereo, 2)) return f;
        if (auto f = checkFile (mono, 1)) return f;

        stereo.deleteFile();
        mono.deleteFile();
        return 0;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    int failures = 0;
    const double rates[] { 44100.0, 48000.0, 96000.0 };
    const int blocks[] { 32, 512, 4096 };

    for (double sr : rates)
        for (int bs : blocks)
            for (int mode : { 0, 1, 2 })
                for (bool play : { false, true })
                    failures += runProcessorPass (sr, bs, mode, play);

    failures += testEnvelopeAndStereo();
    failures += testPlaybackStops();
    failures += testWavExport();

    if (failures == 0)
        std::cout << "DspSmokeTest: all passes clean.\n";

    return failures == 0 ? 0 : 1;
}
