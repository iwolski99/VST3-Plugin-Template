#include "PreviewEngine.h"

namespace foundry
{

namespace
{
    // Short raised-cosine ramp applied on start/stop so triggering a loop from
    // the transport, a MIDI note or the PLAY button never clicks.
    constexpr double kDeclickSeconds = 0.004;
}

void PreviewEngine::prepare (double sampleRate, int)
{
    const double next = sampleRate > 1000.0 ? sampleRate : 44100.0;
    const bool rateChanged = std::abs (next - hostSr) > 0.5;

    hostSr = next;
    envStep = (float) (1.0 / juce::jmax (1.0, hostSr * kDeclickSeconds));

    // Clips are resampled to the host rate on load, so a rate change would
    // otherwise leave the loaded loop playing at the wrong speed and pitch.
    if (rateChanged && clipLoaded.load())
    {
        const auto f = currentFile();
        const bool wasLooping = looping.load();
        const bool wasPlaying = playing.load();

        if (f.existsAsFile())
        {
            juce::String error;
            if (loadInternal (f, wasLooping, wasPlaying, error))
                return;
        }

        // The source is gone, so the stale clip cannot be rebuilt: drop it
        // rather than play it back at the wrong rate.
        {
            const juce::SpinLock::ScopedLockType sl (swapLock);
            clip.reset();
        }

        clipLoaded.store (false);
        clipSamples.store (0);
        playing.store (false);
        position.store (0);
        envelope = 0.0f;
    }
}

void PreviewEngine::reset()
{
    playing.store (false);
    position.store (0);
    envelope = 0.0f;
}

void PreviewEngine::stop()
{
    playing.store (false);
}

void PreviewEngine::play()
{
    if (clipLoaded.load())
        playing.store (true);
}

void PreviewEngine::retrigger()
{
    if (! clipLoaded.load())
        return;

    restart.store (true);
    playing.store (true);
}

juce::String PreviewEngine::currentName() const
{
    const juce::ScopedLock sl (nameLock);
    return name;
}

juce::File PreviewEngine::currentFile() const
{
    const juce::ScopedLock sl (nameLock);
    return file;
}

double PreviewEngine::clipLengthSeconds() const noexcept
{
    return hostSr > 0.0 ? (double) clipSamples.load() / hostSr : 0.0;
}

void PreviewEngine::alignToHost (double ppqPosition, double bpm) noexcept
{
    const int len = clipSamples.load();
    if (len <= 0 || bpm <= 0.0 || hostSr <= 0.0 || ! std::isfinite (ppqPosition))
        return;

    // Where the host is, in samples since the start of the timeline, folded
    // into the loop. Generated loops are a whole number of bars, so this keeps
    // them locked to the grid however far into the song playback starts.
    const double seconds = ppqPosition * (60.0 / bpm);
    if (seconds < 0.0)
        return;

    const auto samples = (juce::int64) (seconds * hostSr);
    position.store ((int) (samples % (juce::int64) len));

    // A freshly loaded clip has a restart pending, which render() would
    // otherwise consume and rewind to zero, discarding this alignment.
    restart.store (false);
}

bool PreviewEngine::loadAndPlay (const juce::File& f, bool loop, juce::String& error)
{
    return loadInternal (f, loop, true, error);
}

bool PreviewEngine::load (const juce::File& f, bool loop, juce::String& error)
{
    return loadInternal (f, loop, false, error);
}

std::vector<float> PreviewEngine::computePeaks (const juce::AudioBuffer<float>& audio, int numBins)
{
    std::vector<float> out ((size_t) juce::jmax (0, numBins), 0.0f);

    const int n = audio.getNumSamples();
    if (n <= 0 || numBins <= 0)
        return out;

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

    return out;
}

bool PreviewEngine::loadInternal (const juce::File& f, bool loop, bool startNow, juce::String& error)
{
    juce::AudioBuffer<float> raw;
    double sr = 0.0;

    if (! io.loadWav (f, raw, sr, error))
        return false;

    auto next = std::make_unique<Clip>();
    next->audio = AudioIO::toStereo (AudioIO::resample (raw, sr, hostSr));
    next->loop = loop;
    next->peaks = computePeaks (next->audio, kPeakBins);

    const int n = next->audio.getNumSamples();
    if (n <= 0)
    {
        error = "Decoded audio was empty: " + f.getFileName();
        return false;
    }

    {
        const juce::ScopedLock nl (nameLock);
        file = f;
        name = f.getFileName();
    }

    {
        const juce::SpinLock::ScopedLockType sl (swapLock);
        clip = std::move (next);
    }

    clipSamples.store (n);
    looping.store (loop);
    clipLoaded.store (true);
    position.store (0);
    restart.store (true);
    playing.store (startNow);

    return true;
}

void PreviewEngine::render (juce::AudioBuffer<float>& buffer) noexcept
{
    const bool wantAudio = playing.load();

    // Keep rendering while the release ramp finishes, otherwise stopping clicks.
    if (! wantAudio && envelope <= 0.0f)
        return;

    const juce::SpinLock::ScopedTryLockType sl (swapLock);
    if (! sl.isLocked() || clip == nullptr)
        return;

    auto& audio = clip->audio;
    const int srcN = audio.getNumSamples();
    const int srcCh = audio.getNumChannels();
    if (srcN <= 0 || srcCh <= 0)
        return;

    if (restart.exchange (false))
    {
        position.store (0);
        envelope = 0.0f;
    }

    int pos = position.load();
    if (pos < 0 || pos >= srcN)
        pos = 0;

    const bool loop = clip->loop;
    const float g = volume.load();
    const int outN = buffer.getNumSamples();
    const int outCh = buffer.getNumChannels();
    const float target = wantAudio ? 1.0f : 0.0f;

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
                envelope = 0.0f;
                return;
            }
        }

        envelope = envelope < target ? juce::jmin (target, envelope + envStep)
                                     : juce::jmax (target, envelope - envStep);

        const float gain = g * envelope;

        for (int ch = 0; ch < outCh; ++ch)
        {
            const int sch = juce::jmin (ch, srcCh - 1);
            buffer.addSample (ch, i, audio.getSample (sch, pos) * gain);
        }

        ++pos;

        if (! wantAudio && envelope <= 0.0f)
        {
            // Fully released: hold the position so PLAY resumes where we left off.
            position.store (pos);
            return;
        }
    }

    position.store (pos);
}

void PreviewEngine::copyPeaks (std::vector<float>& out, int numBins) const
{
    out.assign ((size_t) juce::jmax (0, numBins), 0.0f);

    if (numBins <= 0)
        return;

    const juce::SpinLock::ScopedTryLockType sl (swapLock);
    if (! sl.isLocked() || clip == nullptr)
        return;

    const auto& peaks = clip->peaks;
    if (peaks.empty())
        return;

    for (int i = 0; i < numBins; ++i)
    {
        const auto src = (size_t) ((juce::int64) i * (juce::int64) peaks.size() / (juce::int64) numBins);
        out[(size_t) i] = peaks[juce::jmin (src, peaks.size() - 1)];
    }
}

} // namespace foundry
