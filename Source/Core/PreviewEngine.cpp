#include "PreviewEngine.h"

namespace foundry
{

void PreviewEngine::prepare (double sampleRate, int)
{
    hostSr = sampleRate > 1000.0 ? sampleRate : 44100.0;
}

void PreviewEngine::reset()
{
    stop();
}

void PreviewEngine::stop()
{
    playing.store (false);
    position.store (0);
}

void PreviewEngine::play()
{
    const juce::SpinLock::ScopedLockType sl (swapLock);
    if (clip != nullptr && clip->audio.getNumSamples() > 0)
        playing.store (true);
}

void PreviewEngine::retrigger()
{
    position.store (0);
    play();
}

juce::String PreviewEngine::currentName() const
{
    return name;
}

bool PreviewEngine::loadAndPlay (const juce::File& f, bool loop, juce::String& error)
{
    juce::AudioBuffer<float> raw;
    double sr = 0.0;

    if (! io.loadWav (f, raw, sr, error))
        return false;

    auto stereo = AudioIO::toStereo (AudioIO::resample (raw, sr, hostSr));
    auto next = std::make_unique<Clip>();
    next->audio = std::move (stereo);
    next->loop = loop;

    {
        const juce::SpinLock::ScopedLockType sl (swapLock);
        clip = std::move (next);
        file = f;
        name = f.getFileName();
        looping.store (loop);
        position.store (0);
        playing.store (true);
    }

    return true;
}

void PreviewEngine::mixInto (juce::AudioBuffer<float>& buffer) noexcept
{
    if (! playing.load())
        return;

    const juce::SpinLock::ScopedTryLockType sl (swapLock);
    if (! sl.isLocked() || clip == nullptr)
        return;

    auto& audio = clip->audio;
    const int srcN = audio.getNumSamples();
    const int srcCh = audio.getNumChannels();
    if (srcN <= 0 || srcCh <= 0)
        return;

    int pos = position.load();
    const bool loop = clip->loop;
    const float g = volume.load();
    const int outN = buffer.getNumSamples();
    const int outCh = buffer.getNumChannels();

    for (int i = 0; i < outN; ++i)
    {
        if (pos >= srcN)
        {
            if (loop)
            {
                pos = 0;
            }
            else
            {
                playing.store (false);
                position.store (0);
                return;
            }
        }

        for (int ch = 0; ch < outCh; ++ch)
        {
            const int sch = juce::jmin (ch, srcCh - 1);
            buffer.addSample (ch, i, audio.getSample (sch, pos) * g);
        }

        ++pos;
    }

    position.store (pos);
}

void PreviewEngine::copyPeaks (std::vector<float>& out, int numBins) const
{
    out.assign ((size_t) juce::jmax (0, numBins), 0.0f);

    const juce::SpinLock::ScopedTryLockType sl (swapLock);
    if (! sl.isLocked() || clip == nullptr || numBins <= 0)
        return;

    const auto& audio = clip->audio;
    const int n = audio.getNumSamples();
    if (n <= 0)
        return;

    const int bin = juce::jmax (1, n / numBins);

    for (int i = 0; i < numBins; ++i)
    {
        const int start = i * bin;
        const int len = juce::jmin (bin, n - start);
        if (len <= 0)
            break;

        float mag = 0.0f;
        for (int ch = 0; ch < audio.getNumChannels(); ++ch)
            mag = juce::jmax (mag, audio.getMagnitude (ch, start, len));

        out[(size_t) i] = mag;
    }
}

} // namespace foundry
