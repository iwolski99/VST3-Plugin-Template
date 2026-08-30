#pragma once

#include "ToneRenderer.h"

namespace pt
{

//==============================================================================
/** Stream a RenderSpec to a 16-bit PCM WAV. Pure-tone exports are mono;
    binaural (and "both" preview-style) exports are stereo.

    Written in blocks so a 60-minute file never has to live in RAM as a
    single AudioBuffer. */
inline bool renderToWav (const juce::File& file,
                         RenderSpec spec,
                         ExportKind kind,
                         juce::String& error)
{
    const double sr = spec.sampleRate > 0.0 ? spec.sampleRate : 44100.0;
    spec.sampleRate = sr;

    const bool mono = (kind == ExportKind::pureTone);
    if (mono)
        spec.mode = OutputMode::pureTone;
    else if (spec.mode == OutputMode::pureTone)
        spec.mode = OutputMode::binaural;

    const int total = juce::jmax (1, (int) std::llround ((double) spec.durationSec * sr));
    const int ch    = mono ? 1 : 2;

    if (file.existsAsFile() && ! file.deleteFile())
    {
        error = "Could not overwrite " + file.getFullPathName();
        return false;
    }

    file.getParentDirectory().createDirectory();

    auto stream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream());
    if (stream == nullptr)
    {
        error = "Could not create " + file.getFullPathName();
        return false;
    }

    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer (format.createWriterFor (
        stream.get(), sr, (unsigned) ch, 16, {}, 0));

    if (writer == nullptr)
    {
        error = "Could not open a WAV writer";
        return false;
    }

    stream.release();

    ToneRenderer renderer;
    renderer.prepare (sr);
    renderer.setSpec (spec);

    constexpr int kBlock = 4096;
    juce::AudioBuffer<float> block (ch, kBlock);
    int remaining = total;

    while (remaining > 0)
    {
        const int n = juce::jmin (kBlock, remaining);
        block.setSize (ch, n, false, false, true);

        auto* l = block.getWritePointer (0);
        float* r = ch > 1 ? block.getWritePointer (1) : nullptr;

        for (int i = 0; i < n; ++i)
        {
            float left = 0.0f, right = 0.0f;
            renderer.processSample (left, right);
            l[i] = left;
            if (r != nullptr)
                r[i] = right;
        }

        if (! writer->writeFromAudioSampleBuffer (block, 0, n))
        {
            error = "Write failed part-way through " + file.getFileName();
            return false;
        }

        remaining -= n;
    }

    return true;
}

inline juce::String makeExportFileName (const RenderSpec& spec, ExportKind kind)
{
    if (kind == ExportKind::pureTone)
        return "Tone_" + juce::String (spec.leftHz, 1) + "Hz_"
             + juce::String (spec.durationSec, 1) + "s.wav";

    const float beat = std::abs (spec.rightHz - spec.leftHz);
    return "Binaural_L" + juce::String (spec.leftHz, 1)
         + "_R" + juce::String (spec.rightHz, 1)
         + "_Beat" + juce::String (beat, 2)
         + "_" + juce::String (spec.durationSec, 1) + "s.wav";
}

} // namespace pt
