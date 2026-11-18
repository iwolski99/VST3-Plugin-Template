#pragma once

#include "Types.h"

namespace foundry
{

enum class PatternKind { Drums = 0, Bass = 1 };

/** A reusable step pattern - Foundry's equivalent of a MIDI clip.

    Drum patterns have four rows (kick, clap, hat, ride); bass patterns have
    four source rows (A-D), at most one sounding per step. A pattern can also
    remember which sample each row played: every loop the generator renders
    gets one of these saved next to it, so a loop with great sounds but the
    wrong groove can be reopened in the sequencer, fixed and regenerated. */
struct PatternFile
{
    PatternKind kind = PatternKind::Drums;
    DrumPattern grid;                       // 4 rows x (16 * bars) steps
    std::array<juce::String, 4> samples {}; // optional: the sample on each row
    double bpm = 0.0;
    Genre genre = Genre::Riddim;

    bool hasSamples() const noexcept;

    juce::String toJson() const;
    static bool fromJson (const juce::String& json, PatternFile& out);

    bool save (const juce::File& file) const;
    static bool load (const juce::File& file, PatternFile& out);
};

inline constexpr const char* kPatternExtension = ".foundrypattern";

/** The pattern saved next to a rendered loop. */
juce::File patternSidecarFor (const juce::File& audio);

juce::String patternKindName (PatternKind k);
juce::StringArray patternRowNames (PatternKind k);

} // namespace foundry
