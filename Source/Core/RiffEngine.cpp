#include "RiffEngine.h"
#include <algorithm>

namespace foundry
{

namespace
{
    constexpr double kChokeSeconds   = 0.003;   // previous slice fades when the next one starts
    constexpr double kSliceEndSeconds = 0.003;  // every slice fades out over its last few ms
    constexpr double kJumpSeconds    = 0.004;   // fade used when the timeline jumps or stops
    constexpr int    kTimingSlack    = 64;      // samples of host timing jitter tolerated as "continuous"

    double positiveMod (double x, double m) noexcept
    {
        if (m <= 0.0)
            return 0.0;
        auto r = std::fmod (x, m);
        return r < 0.0 ? r + m : r;
    }
}

std::unique_ptr<RiffSnapshot> buildRiffSnapshot (const RiffSketch& sketch, SampleCache& cache,
                                                 double sampleRate, juce::StringArray* problems)
{
    auto snap = std::make_unique<RiffSnapshot>();
    snap->gridSteps = sketch.gridSteps;
    snap->sampleRate = sampleRate;

    for (int i = 0; i < kRiffSlots; ++i)
    {
        const auto& src = sketch.slots[(size_t) i];
        auto& dst = snap->slots[(size_t) i];

        dst.lengthSteps = juce::jlimit (1, kMaxSlotSteps, src.lengthSteps);
        dst.gain = juce::jlimit (0.0f, 2.0f, src.gain);

        if (src.isEmpty())
            continue;

        juce::String err;
        dst.audio = cache.getAtRate (juce::File (src.path), sampleRate, err);

        if (dst.audio == nullptr || dst.audio->getNumSamples() <= 0)
        {
            dst.audio.reset();
            if (problems != nullptr)
                problems->addIfNotAlreadyThere (juce::File (src.path).getFileName());
            continue;
        }

        dst.offsetSamples = juce::jlimit (0, dst.audio->getNumSamples() - 1,
                                          juce::roundToInt (src.offsetSeconds * sampleRate));
    }

    return snap;
}

//==============================================================================
RiffEngine::RiffEngine()
{
    publish (std::make_unique<RiffSnapshot>());
}

RiffEngine::~RiffEngine() = default;

void RiffEngine::publish (std::unique_ptr<RiffSnapshot> snapshot)
{
    if (snapshot == nullptr)
        return;

    const juce::ScopedLock sl (publishLock);
    auto* raw = snapshot.get();
    owned.push_back (std::move (snapshot));
    latest.store (raw);
    collectGarbage();
}

void RiffEngine::collectGarbage()
{
    // Called under publishLock. A snapshot may go once it is neither the
    // latest nor protected by either hazard pointer: the audio thread
    // re-checks `latest` after publishing its hazard, so it can never start
    // using a snapshot this loop has already decided to free.
    auto* keepLatest = latest.load();
    auto* keepA = hazard[0].load();
    auto* keepB = hazard[1].load();

    owned.erase (std::remove_if (owned.begin(), owned.end(),
                                 [&] (const std::unique_ptr<RiffSnapshot>& p)
                                 {
                                     auto* r = p.get();
                                     return r != keepLatest && r != keepA && r != keepB;
                                 }),
                 owned.end());
}

const RiffSnapshot* RiffEngine::acquireSnapshot() noexcept
{
    RiffSnapshot* s = nullptr;
    do
    {
        s = latest.load();
        hazard[0].store (s);
    }
    while (s != latest.load());
    return s;
}

void RiffEngine::switchSnapshotIfNeeded() noexcept
{
    if (latest.load() == current)
        return;

    // Only the outgoing snapshot can be kept alive for fading voices; anything
    // older than that loses its protection now, so silence it first.
    for (auto& v : voices)
        if (v.active && v.owner != current)
            v.active = false;

    const auto* old = current;
    hazard[1].store (const_cast<RiffSnapshot*> (old));
    current = acquireSnapshot();

    for (auto& v : voices)
    {
        if (! v.active)
            continue;

        bool stillThere = false;
        for (const auto& s : current->slots)
            if (s.audio.get() == v.audio)
                stillThere = true;

        if (stillThere)
            v.owner = current;                 // same sample: keep ringing, no click
        else
            v.releaseStep = juce::jmax (v.releaseStep,
                                        v.env / (float) juce::jmax (1.0, kChokeSeconds * sampleRate));
    }
}

void RiffEngine::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate > 1000.0 ? newSampleRate : 48000.0;
    for (auto& v : voices)
        v.active = false;
    mode = Mode::Stopped;
    expectedSteps = -1.0;
    running.store (false);
    currentSlot.store (-1);
}

void RiffEngine::releaseAll (float seconds) noexcept
{
    const float samples = (float) juce::jmax (1.0, (double) seconds * sampleRate);
    for (auto& v : voices)
        if (v.active)
            v.releaseStep = juce::jmax (v.releaseStep, v.env / samples);
}

void RiffEngine::startSlot (int slot, double elapsedSteps, double bpm, bool chokeOthers) noexcept
{
    if (current == nullptr || slot < 0 || slot >= kRiffSlots)
        return;

    const auto& s = current->slots[(size_t) slot];
    if (s.audio == nullptr)
        return;                                 // a rest never cuts the previous slice

    const double samplesPerStep = 60.0 / juce::jmax (1.0, bpm) / 4.0 * sampleRate;
    const int sliceLen = juce::jmax (1, juce::roundToInt (s.lengthSteps * samplesPerStep));
    const int elapsed = juce::jmax (0, juce::roundToInt (elapsedSteps * samplesPerStep));
    if (elapsed >= sliceLen)
        return;

    const int readPos = s.offsetSamples + elapsed;
    if (readPos >= s.audio->getNumSamples())
        return;

    if (chokeOthers)
        releaseAll ((float) kChokeSeconds);

    Voice* target = nullptr;
    for (auto& v : voices)
        if (! v.active) { target = &v; break; }

    if (target == nullptr)
    {
        target = &voices[0];
        for (auto& v : voices)
            if (v.env < target->env)
                target = &v;
    }

    // A slice that starts at the very top of the sample keeps its transient
    // (an 8-sample guard only); one cut out of the middle needs a real fade.
    const bool cleanStart = s.offsetSamples == 0 && elapsed == 0;
    const int attack = cleanStart ? AudioIO::kOneShotFadeInSamples
                                  : juce::jmax (8, juce::roundToInt (0.001 * sampleRate));

    target->audio       = s.audio.get();
    target->owner       = current;
    target->slot        = slot;
    target->readPos     = readPos;
    target->remaining   = sliceLen - elapsed;
    target->gain        = s.gain;
    target->env         = 0.0f;
    target->attackStep  = 1.0f / (float) attack;
    target->releaseStep = 0.0f;
    target->active      = true;
}

void RiffEngine::chase (double posInRiff, double bpm) noexcept
{
    // Playback began (or jumped) mid-riff: pick up the slice that should be
    // sounding at this point instead of waiting for the next slot boundary.
    if (current == nullptr)
        return;

    const int grid = juce::jmax (1, current->gridSteps);
    const int k = juce::jlimit (0, kRiffSlots - 1, (int) (posInRiff / grid));

    for (int j = k; j >= 0; --j)
    {
        const auto& s = current->slots[(size_t) j];
        const double elapsed = posInRiff - (double) (j * grid);

        if (s.audio == nullptr)
            continue;                           // look back through rests

        // At (or within a hair of) a slot boundary the event loop starts it.
        if (elapsed > 1.0e-6 && elapsed < (double) s.lengthSteps)
            startSlot (j, elapsed, bpm, false);
        return;
    }
}

void RiffEngine::renderVoices (juce::AudioBuffer<float>& out, int start, int num,
                               float volStart, float volEnd) noexcept
{
    if (num <= 0)
        return;

    const int outCh = out.getNumChannels();
    if (outCh <= 0)
        return;

    float* o0 = out.getWritePointer (0);
    float* o1 = outCh > 1 ? out.getWritePointer (1) : nullptr;
    const int fadeOut = juce::jmax (8, juce::roundToInt (kSliceEndSeconds * sampleRate));
    const float volStep = (volEnd - volStart) / (float) num;

    for (auto& v : voices)
    {
        if (! v.active || v.audio == nullptr)
            continue;

        const auto& a = *v.audio;
        const int len = a.getNumSamples();
        const float* l = a.getReadPointer (0);
        const float* r = a.getReadPointer (juce::jmin (1, a.getNumChannels() - 1));

        for (int i = 0; i < num; ++i)
        {
            if (v.readPos >= len || v.remaining <= 0)
            {
                v.active = false;
                break;
            }

            if (v.releaseStep <= 0.0f && v.remaining <= fadeOut)
                v.releaseStep = juce::jmax (1.0e-6f, v.env / (float) juce::jmax (1, v.remaining));

            if (v.releaseStep > 0.0f)
            {
                v.env -= v.releaseStep;
                if (v.env <= 0.0f)
                {
                    v.active = false;
                    break;
                }
            }
            else if (v.env < 1.0f)
            {
                v.env = juce::jmin (1.0f, v.env + v.attackStep);
            }

            const float g = v.gain * v.env * (volStart + volStep * (float) i);
            o0[start + i] += l[v.readPos] * g;
            if (o1 != nullptr)
                o1[start + i] += r[v.readPos] * g;

            ++v.readPos;
            --v.remaining;
        }
    }
}

void RiffEngine::process (juce::AudioBuffer<float>& buffer, const HostInfo& host) noexcept
{
    const int n = buffer.getNumSamples();
    if (n <= 0)
        return;

    switchSnapshotIfNeeded();

    if (stopAllRequest.exchange (false))
    {
        internalOn = false;
        releaseAll ((float) kJumpSeconds);
    }
    if (internalStopRequest.exchange (false))
        internalOn = false;

    bool restartInternal = false;
    if (internalStartRequest.exchange (false))
    {
        internalOn = true;
        internalSteps = 0.0;
        restartInternal = true;
    }

    const double bpm = juce::jlimit (20.0, 999.0, host.bpm > 0.0 ? host.bpm : 140.0);
    const double stepsPerSample = bpm / 60.0 * 4.0 / sampleRate;
    const double blockSteps = stepsPerSample * n;

    Mode newMode = Mode::Stopped;
    double pos = 0.0;

    if (followHost.load() && host.playing && host.ppqValid)
    {
        newMode = Mode::Host;
        pos = host.ppq * 4.0;
        internalOn = false;     // the host has taken over
    }
    else if (internalOn)
    {
        newMode = Mode::Internal;
        pos = internalSteps;
    }

    if (newMode != mode || restartInternal)
    {
        if (mode != Mode::Stopped)
            releaseAll ((float) kJumpSeconds);
        mode = newMode;
        expectedSteps = -1.0;
    }

    internalRunning.store (internalOn);

    const int audition = auditionRequest.exchange (-1);
    if (audition >= 0)
        startSlot (audition, 0.0, bpm);

    const float vol = volume.load();
    const bool haveRiff = current != nullptr && std::abs (current->sampleRate - sampleRate) < 1.0;

    if (mode == Mode::Stopped || ! haveRiff)
    {
        renderVoices (buffer, 0, n, lastVolume, vol);
        lastVolume = vol;
        running.store (false);
        currentSlot.store (-1);
        if (! haveRiff)
            expectedSteps = -1.0;
    }
    else
    {
        const int grid = juce::jmax (1, current->gridSteps);
        const double riffLen = (double) (grid * kRiffSlots);
        const double slack = stepsPerSample * kTimingSlack;

        double windowStart = pos;
        if (expectedSteps >= 0.0 && std::abs (pos - expectedSteps) <= slack)
        {
            windowStart = expectedSteps;        // continuous: no gaps, no double triggers
        }
        else
        {
            if (expectedSteps >= 0.0)
                releaseAll ((float) kJumpSeconds);   // the host jumped (seek or loop)
            chase (positiveMod (pos, riffLen), bpm);
        }

        const double windowEnd = pos + blockSteps;

        struct Event { int offset; int slot; };
        std::array<Event, 64> events {};
        int numEvents = 0;

        for (double base = std::floor (windowStart / riffLen) * riffLen; base < windowEnd; base += riffLen)
        {
            for (int k = 0; k < kRiffSlots && numEvents < (int) events.size(); ++k)
            {
                const double t = base + (double) (k * grid);
                if (t >= windowStart && t < windowEnd)
                    events[(size_t) numEvents++] = { juce::jlimit (0, n - 1, (int) ((t - pos) / stepsPerSample)), k };
            }
        }

        std::sort (events.begin(), events.begin() + numEvents,
                   [] (const Event& a, const Event& b) { return a.offset < b.offset; });

        const float volStep = (vol - lastVolume) / (float) n;
        auto volAt = [&] (int i) { return lastVolume + volStep * (float) i; };

        int cursor = 0;
        for (int e = 0; e < numEvents; ++e)
        {
            const int at = events[(size_t) e].offset;
            renderVoices (buffer, cursor, at - cursor, volAt (cursor), volAt (at));
            cursor = at;
            startSlot (events[(size_t) e].slot, 0.0, bpm);
        }
        renderVoices (buffer, cursor, n - cursor, volAt (cursor), vol);
        lastVolume = vol;

        expectedSteps = windowEnd;

        if (mode == Mode::Internal)
        {
            internalSteps = windowEnd;
            if (internalSteps > riffLen * 4096.0)  // keep the double precise over long sessions
            {
                internalSteps = positiveMod (internalSteps, riffLen);
                expectedSteps = internalSteps;
            }
        }

        running.store (true);
        currentSlot.store (juce::jlimit (0, kRiffSlots - 1, (int) (positiveMod (windowEnd, riffLen) / grid)));
    }

    // Let go of the outgoing snapshot once nothing still reads from it.
    if (auto* old = hazard[1].load())
    {
        bool inUse = false;
        for (const auto& v : voices)
            if (v.active && v.owner == old)
                inUse = true;
        if (! inUse)
            hazard[1].store (nullptr);
    }
}

//==============================================================================
juce::AudioBuffer<float> RiffEngine::renderOffline (const RiffSketch& sketch, SampleCache& cache,
                                                    double bpm, double sr, int loops,
                                                    juce::StringArray* problems)
{
    RiffEngine engine;
    engine.publish (buildRiffSnapshot (sketch, cache, sr, problems));
    engine.prepare (sr);
    engine.setFollowHost (false);
    engine.setVolume (1.0f);
    engine.lastVolume = 1.0f;
    engine.startInternal();

    const int loopLen = juce::jmax (1, juce::roundToInt (sketch.lengthBeats() * 60.0 / bpm * sr));
    const int total = loopLen * juce::jmax (1, loops);

    juce::AudioBuffer<float> out (2, total);
    out.clear();

    HostInfo host;
    host.bpm = bpm;

    constexpr int block = 512;
    for (int pos = 0; pos < total; pos += block)
    {
        const int len = juce::jmin (block, total - pos);
        juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), 2, pos, len);
        engine.process (view, host);
    }

    return out;
}

juce::AudioBuffer<float> RiffEngine::renderLoop (const RiffSketch& sketch, SampleCache& cache,
                                                 double bpm, double sr, juce::StringArray* problems)
{
    auto twice = renderOffline (sketch, cache, bpm, sr, 2, problems);
    const int half = twice.getNumSamples() / 2;

    juce::AudioBuffer<float> loop (2, juce::jmax (1, half));
    for (int ch = 0; ch < 2; ++ch)
        loop.copyFrom (ch, 0, twice, ch, half, half);
    return loop;
}

} // namespace foundry
