#include "AudioIO.h"
#include <limits>

namespace foundry
{

AudioIO::AudioIO()
{
    // WAV is the default; AIFF, FLAC and Ogg cover the rest of a typical
    // sample library. Output is always written as WAV.
    formats.registerBasicFormats();
}

int AudioIO::beatsToSamples (double beats, double bpm, double sampleRate) noexcept
{
    if (bpm <= 0.0 || sampleRate <= 0.0)
        return 0;

    return juce::roundToInt (beats * (60.0 / bpm) * sampleRate);
}

int AudioIO::loopLengthSamples (int bars, double bpm, double sampleRate) noexcept
{
    return juce::jmax (1, beatsToSamples (bars * 4.0, bpm, sampleRate));
}

int AudioIO::fadeSamplesFor (double sampleRate) noexcept
{
    return juce::jmax (8, juce::roundToInt (sampleRate * (0.001 * (double) kFadeMs)));
}

juce::AudioBuffer<float> AudioIO::toStereo (const juce::AudioBuffer<float>& src)
{
    juce::AudioBuffer<float> out (2, juce::jmax (1, src.getNumSamples()));
    out.clear();

    if (src.getNumChannels() <= 0 || src.getNumSamples() <= 0)
        return out;

    if (src.getNumChannels() == 1)
    {
        out.copyFrom (0, 0, src, 0, 0, src.getNumSamples());
        out.copyFrom (1, 0, src, 0, 0, src.getNumSamples());
    }
    else
    {
        out.copyFrom (0, 0, src, 0, 0, src.getNumSamples());
        out.copyFrom (1, 0, src, juce::jmin (1, src.getNumChannels() - 1), 0, src.getNumSamples());
    }

    return out;
}

juce::AudioBuffer<float> AudioIO::resample (const juce::AudioBuffer<float>& src,
                                            double srcRate, double dstRate)
{
    if (src.getNumSamples() <= 0 || src.getNumChannels() <= 0)
        return src;

    if (srcRate <= 0.0 || dstRate <= 0.0 || std::abs (srcRate - dstRate) < 0.5)
        return src;

    const double speed = srcRate / dstRate;
    const int srcN = src.getNumSamples();

    // process() consumes ceil(outN * speed) input samples. Round the output
    // length *down* against that so the interpolator never reads past the end
    // of the source buffer.
    const int outN = juce::jmax (1, (int) std::floor ((double) srcN / speed));
    juce::AudioBuffer<float> out (src.getNumChannels(), outN);
    out.clear();

    juce::LagrangeInterpolator interpolator;

    for (int ch = 0; ch < src.getNumChannels(); ++ch)
    {
        interpolator.reset();
        interpolator.process (speed,
                              src.getReadPointer (ch),
                              out.getWritePointer (ch),
                              outN);
    }

    return out;
}

bool AudioIO::loadWav (const juce::File& file, juce::AudioBuffer<float>& buffer,
                       double& sampleRate, juce::String& error, double maxSeconds)
{
    if (! file.existsAsFile())
    {
        error = "File not found: " + file.getFileName();
        return false;
    }

    if (! isSupportedAudioFile (file))
    {
        error = "Unsupported audio file (WAV, AIFF, FLAC or Ogg): " + file.getFileName();
        return false;
    }

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr)
    {
        error = "Could not read audio file (corrupt or unsupported): " + file.getFileName();
        return false;
    }

    auto available = reader->lengthInSamples;
    if (maxSeconds > 0.0 && reader->sampleRate > 0.0)
        available = juce::jmin (available, (juce::int64) (maxSeconds * reader->sampleRate));

    const int n = (int) juce::jmin ((juce::int64) std::numeric_limits<int>::max() - 8, available);
    if (n <= 0)
    {
        error = "Audio file is empty: " + file.getFileName();
        return false;
    }

    buffer.setSize ((int) juce::jmax ((juce::uint32) 1, reader->numChannels), n, false, true, true);
    if (! reader->read (&buffer, 0, n, 0, true, true))
    {
        error = "Failed while decoding: " + file.getFileName();
        return false;
    }

    sampleRate = reader->sampleRate;
    if (sampleRate < 1000.0)
    {
        error = "Invalid sample rate in " + file.getFileName();
        return false;
    }

    return true;
}

bool AudioIO::writeWav (const juce::File& file, const juce::AudioBuffer<float>& buffer,
                        double sampleRate, int bitDepth, juce::String& error)
{
    if (buffer.getNumSamples() <= 0 || buffer.getNumChannels() <= 0)
    {
        error = "Nothing to write.";
        return false;
    }

    if (! file.getParentDirectory().isDirectory())
        if (! file.getParentDirectory().createDirectory())
        {
            error = "Cannot create folder: " + file.getParentDirectory().getFullPathName();
            return false;
        }

    if (file.existsAsFile())
        file.deleteFile();

    auto os = std::unique_ptr<juce::OutputStream> (new juce::FileOutputStream (file));
    auto* fos = dynamic_cast<juce::FileOutputStream*> (os.get());
    if (fos == nullptr || fos->failedToOpen())
    {
        error = "Cannot write file (permission or disk error): " + file.getFullPathName();
        return false;
    }

    juce::WavAudioFormat wav;
    const int bits = bitDepth == 16 ? 16 : 24;
    const int chans = juce::jlimit (1, 2, buffer.getNumChannels());
    juce::AudioChannelSet layout = chans == 1 ? juce::AudioChannelSet::mono()
                                              : juce::AudioChannelSet::stereo();

    const auto options = juce::AudioFormatWriterOptions{}
                             .withSampleRate (sampleRate)
                             .withChannelLayout (layout)
                             .withBitsPerSample (bits)
                             .withNumChannels (chans);

    auto writer = wav.createWriterFor (os, options);
    if (writer == nullptr)
    {
        error = "Could not create WAV writer for " + file.getFileName();
        return false;
    }

    if (! writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples()))
    {
        writer.reset();
        file.deleteFile();
        error = "WAV write failed (disk full?): " + file.getFileName();
        return false;
    }

    return true;
}

void AudioIO::mixAt (juce::AudioBuffer<float>& dest,
                     const juce::AudioBuffer<float>& src,
                     int destSample,
                     float gain,
                     int fadeInSamples,
                     int extraFadeOutSamples)
{
    if (destSample >= dest.getNumSamples() || src.getNumSamples() <= 0)
        return;

    destSample = juce::jmax (0, destSample);
    const int destCh = dest.getNumChannels();
    const int srcCh = src.getNumChannels();
    if (destCh <= 0 || srcCh <= 0)
        return;

    const int available = dest.getNumSamples() - destSample;
    int n = juce::jmin (src.getNumSamples(), available);
    if (n <= 0)
        return;

    const int fadeIn  = juce::jlimit (0, n, fadeInSamples);
    const int fadeOut = juce::jlimit (0, n, extraFadeOutSamples);
    const bool truncated = src.getNumSamples() > available;

    for (int dch = 0; dch < destCh; ++dch)
    {
        const int sch = juce::jmin (dch, srcCh - 1);
        auto* d = dest.getWritePointer (dch, destSample);
        const auto* s = src.getReadPointer (sch);

        for (int i = 0; i < n; ++i)
        {
            float env = 1.0f;

            if (fadeIn > 0 && i < fadeIn)
            {
                const float t = (float) i / (float) fadeIn;
                env *= 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * t);
            }

            const int fromEnd = n - 1 - i;
            int outFade = fadeOut;
            if (truncated)
            {
                // The source was cut off by the end of the destination, so it
                // needs its own release regardless of how short the head fade
                // was - a truncated hit otherwise ends on a step.
                outFade = juce::jmax (outFade, juce::jmin (n / 4, 64));
            }

            if (outFade > 0 && fromEnd < outFade)
            {
                const float t = (float) fromEnd / (float) outFade;
                env *= 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * t);
            }

            d[i] += s[i] * gain * env;
        }
    }
}

void AudioIO::fadeLoopEnds (juce::AudioBuffer<float>& buffer, int fadeSamples)
{
    const int n = buffer.getNumSamples();
    const int f = juce::jlimit (0, n / 2, fadeSamples);
    if (f <= 0)
        return;

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* d = buffer.getWritePointer (ch);

        for (int i = 0; i < f; ++i)
        {
            const float tIn  = (float) i / (float) f;
            const float tOut = (float) i / (float) f;
            const float inEnv  = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * tIn);
            const float outEnv = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * tOut);
            d[i] *= inEnv;
            d[n - 1 - i] *= outEnv;
        }
    }
}

float AudioIO::peakAbs (const juce::AudioBuffer<float>& buffer) noexcept
{
    float peak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        peak = juce::jmax (peak, buffer.getMagnitude (ch, 0, buffer.getNumSamples()));
    return peak;
}

void AudioIO::protectPeaks (juce::AudioBuffer<float>& buffer, float ceiling)
{
    const float peak = peakAbs (buffer);
    if (peak > ceiling && peak > 1.0e-8f)
        buffer.applyGain (ceiling / peak);
}

juce::File AudioIO::uniqueWavPath (const juce::File& dir, const juce::String& stem)
{
    auto f = dir.getChildFile (stem + ".wav");
    int n = 2;
    while (f.existsAsFile() && n < 10000)
    {
        f = dir.getChildFile (stem + "_" + juce::String (n) + ".wav");
        ++n;
    }
    return f;
}

} // namespace foundry
