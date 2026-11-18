#pragma once

#include "RiffTypes.h"
#include "SampleCache.h"

namespace foundry
{

/** A riff compiled for playback at one sample rate. Immutable once published. */
struct RiffSnapshot
{
    struct Slot
    {
        SampleBufferPtr audio;      // stereo at `sampleRate`; null = rest
        int   offsetSamples = 0;
        int   lengthSteps   = 4;
        float gain          = 1.0f;
    };

    std::array<Slot, kRiffSlots> slots;
    int gridSteps = 4;
    double sampleRate = 48000.0;

    double lengthBeats() const noexcept { return gridSteps * kRiffSlots / 4.0; }
};

/** Builds a snapshot from a sketch. Missing or unreadable files become rests;
    their names are added to `problems`. Message thread / non-audio threads. */
std::unique_ptr<RiffSnapshot> buildRiffSnapshot (const RiffSketch& sketch, SampleCache& cache,
                                                 double sampleRate, juce::StringArray* problems = nullptr);

//==============================================================================
/** Plays a riff, either locked to the host timeline or on its own internal
    clock (the PLAY button and MIDI notes), and auditions single slots.

    Realtime contract: process() never locks, allocates or frees. Snapshots
    are swapped through an atomic pointer guarded by two hazard pointers, so
    the message thread only frees a snapshot once the audio thread has
    provably stopped reading it - including the samples of voices that are
    still fading out from before an edit. */
class RiffEngine
{
public:
    struct HostInfo
    {
        bool   playing   = false;
        bool   ppqValid  = false;
        double ppq       = 0.0;
        double bpm       = 140.0;    // host tempo, or the plugin's own when there is no host
    };

    RiffEngine();
    ~RiffEngine();

    //== any non-audio thread ===================================================
    void publish (std::unique_ptr<RiffSnapshot> snapshot);

    //== controls (any thread) ===============================================
    void setFollowHost (bool shouldFollow) noexcept { followHost.store (shouldFollow); }
    bool getFollowHost() const noexcept             { return followHost.load(); }

    void startInternal() noexcept   { internalStartRequest.store (true); }
    void stopInternal() noexcept    { internalStopRequest.store (true); }
    void auditionSlot (int slot) noexcept { auditionRequest.store (slot); }
    void stopAll() noexcept         { stopAllRequest.store (true); }

    void setVolume (float linear) noexcept { volume.store (juce::jlimit (0.0f, 2.0f, linear)); }

    bool isRunning() const noexcept      { return running.load(); }
    bool isInternalRunning() const noexcept { return internalRunning.load(); }
    int  getCurrentSlot() const noexcept { return currentSlot.load(); }

    //== audio thread =========================================================
    void prepare (double sampleRate) noexcept;
    /** Adds the riff into `buffer` (does not clear it). */
    void process (juce::AudioBuffer<float>& buffer, const HostInfo& host) noexcept;

    //== offline =============================================================
    /** Renders `loops` passes of the riff from its start on the internal
        clock - the same code path as live playback, so a render sounds the way
        the riff plays in the plugin. */
    static juce::AudioBuffer<float> renderOffline (const RiffSketch& sketch, SampleCache& cache,
                                                   double bpm, double sampleRate, int loops,
                                                   juce::StringArray* problems = nullptr);

    /** Renders one seamless loop: two passes, keeping the second, so a slice
        ringing past the end of the riff (into a rest at its start) is already
        folded into the beginning exactly as it is when the riff loops live. */
    static juce::AudioBuffer<float> renderLoop (const RiffSketch& sketch, SampleCache& cache,
                                                double bpm, double sampleRate,
                                                juce::StringArray* problems = nullptr);

private:
    struct Voice
    {
        const juce::AudioBuffer<float>* audio = nullptr;
        const RiffSnapshot* owner = nullptr;
        int   slot = -1;
        int   readPos = 0;
        int   remaining = 0;     // samples left before the slice ends
        float gain = 1.0f;
        float env = 0.0f;
        float attackStep = 1.0f;
        float releaseStep = 0.0f;
        bool  active = false;
    };

    enum class Mode { Stopped, Host, Internal };

    const RiffSnapshot* acquireSnapshot() noexcept;
    void switchSnapshotIfNeeded() noexcept;
    void startSlot (int slot, double elapsedSteps, double bpm, bool chokeOthers = true) noexcept;
    void chase (double posInRiffSteps, double bpm) noexcept;
    void releaseAll (float seconds) noexcept;
    void renderVoices (juce::AudioBuffer<float>& out, int start, int num, float volStart, float volEnd) noexcept;
    void collectGarbage();

    static constexpr int kMaxVoices = 8;

    // snapshot exchange
    std::atomic<RiffSnapshot*> latest { nullptr };
    std::atomic<RiffSnapshot*> hazard[2] { { nullptr }, { nullptr } };
    juce::CriticalSection publishLock;
    std::vector<std::unique_ptr<RiffSnapshot>> owned;

    // controls
    std::atomic<bool>  followHost { true };
    std::atomic<bool>  internalStartRequest { false }, internalStopRequest { false }, stopAllRequest { false };
    std::atomic<int>   auditionRequest { -1 };
    std::atomic<float> volume { 0.8f };

    // reported to the UI
    std::atomic<bool> running { false }, internalRunning { false };
    std::atomic<int>  currentSlot { -1 };

    // audio-thread state
    const RiffSnapshot* current = nullptr;
    std::array<Voice, kMaxVoices> voices;
    double sampleRate = 48000.0;
    Mode   mode = Mode::Stopped;
    bool   internalOn = false;
    double internalSteps = 0.0;       // internal clock, in 16th notes
    double expectedSteps = -1.0;      // where the next block should start if time is continuous
    float  lastVolume = 0.8f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RiffEngine)
};

} // namespace foundry
