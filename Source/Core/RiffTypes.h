#pragma once

#include "Types.h"

namespace foundry
{

inline constexpr int kRiffSlots    = 8;
inline constexpr int kMaxSlotSteps = 16;   // a slice can last up to one bar of 16ths

/** One cell of the Riff Maker. A slot plays only the start of its sample -
    `lengthSteps` 16th notes of it, beginning `offsetSeconds` into the file. */
struct RiffSlot
{
    juce::String path;              // empty = a rest
    double offsetSeconds = 0.0;     // slip-edit: where in the sample playback begins
    int    lengthSteps   = 4;       // in 16th notes; 4 = a quarter note
    float  gain          = 1.0f;    // linear, 0..2
    bool   locked        = false;   // the dice leaves locked slots alone

    bool isEmpty() const noexcept { return path.isEmpty(); }
    juce::String displayName() const { return juce::File (path).getFileNameWithoutExtension(); }

    bool operator== (const RiffSlot& o) const noexcept
    {
        return path == o.path && juce::approximatelyEqual (offsetSeconds, o.offsetSeconds)
            && lengthSteps == o.lengthSteps && juce::approximatelyEqual (gain, o.gain)
            && locked == o.locked;
    }
    bool operator!= (const RiffSlot& o) const noexcept { return ! (*this == o); }
};

/** A whole riff: eight slots placed one after another on a fixed grid.
    Slot i starts at i * gridSteps. A slice longer than the grid spacing is cut
    off when the next slot starts (the riff is monophonic, like a bass line);
    if the next slot is a rest it rings on into it. */
struct RiffSketch
{
    std::array<RiffSlot, kRiffSlots> slots;
    int gridSteps = 4;                  // 1/16 = 1, 1/8 = 2, 1/4 = 4, 1/2 = 8
    juce::int64 lastSeed = 0;           // seed of the most recent dice roll

    int    totalSteps()  const noexcept { return gridSteps * kRiffSlots; }
    double lengthBeats() const noexcept { return totalSteps() / 4.0; }
    bool   isEmpty() const noexcept;

    /** An empty slot is silence - unless the last sample before it was made
        longer than the grid and is still ringing into it. Returns that
        slot's index, or -1 when `slot` is silent. */
    int heldBy (int slot) const noexcept;

    juce::var toVar() const;
    static RiffSketch fromVar (const juce::var&);

    juce::String toJson() const;
    static bool fromJson (const juce::String& json, RiffSketch& out);

    bool operator== (const RiffSketch& o) const noexcept
    {
        return slots == o.slots && gridSteps == o.gridSteps;
    }
    bool operator!= (const RiffSketch& o) const noexcept { return ! (*this == o); }
};

/** "1/16", "1/8", "3/16", "1/4", ... "1 bar" for a count of 16th notes. */
juce::String stepsName (int steps);

/** Grid spacings the Riff Maker offers, in 16ths. */
const std::array<int, 4>& gridChoices() noexcept;

/** Seconds a run of 16th notes lasts at a tempo. */
inline double stepsToSeconds (double steps, double bpm) noexcept
{
    return bpm > 0.0 ? steps * 0.25 * 60.0 / bpm : 0.0;
}

/** The editable description saved next to every rendered riff, so the WAV in
    "My Riffs" can be reopened in the Riff Maker later. */
juce::File riffSidecarFor (const juce::File& wav);
inline constexpr const char* kRiffSidecarExtension = ".foundryriff";

} // namespace foundry
