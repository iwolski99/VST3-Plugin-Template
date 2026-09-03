#pragma once

#include "AudioIO.h"
#include "ConfigStore.h"
#include "LibraryDatabase.h"

namespace foundry
{

struct GenerateOutcome
{
    bool ok = false;
    juce::String error;
    int filesWritten = 0;
    juce::StringArray paths;
};

struct DrumRequest
{
    Genre genre = Genre::Riddim;
    double bpm = kRiddimBpm;
    int bars = 2;
    int quantity = 10;
    bool renderFull = true;
    bool renderStems = true;
    bool protectPeaks = true;
    double sampleRate = 48000.0;
    int bitDepth = 24;
    DrumPattern pattern;
    SampleLock kick, clap, hat, ride;
    juce::File outputDir;
    juce::int64 baseSeed = 0;
};

struct BassRequest
{
    Genre genre = Genre::Riddim;
    double bpm = kRiddimBpm;
    int bars = 2;
    int quantity = 10;
    int sourceCount = 3;          // 2–4
    Variation variation = Variation::Normal;
    bool protectPeaks = true;
    double sampleRate = 48000.0;
    int bitDepth = 24;
    juce::File outputDir;
    juce::int64 baseSeed = 0;
};

struct SpliceRequest
{
    Genre genre = Genre::Riddim;
    double bpm = kRiddimBpm;
    int bars = 2;
    int quantity = 10;
    SliceRes slice = SliceRes::Quarter;
    SpliceMode mode = SpliceMode::CallAndResponse;
    Variation variation = Variation::Normal;
    bool protectPeaks = true;
    double sampleRate = 48000.0;
    int bitDepth = 24;
    juce::File outputDir;
    std::vector<juce::File> sources;
    juce::String parentIds;
    int parentDepth = 0;
    juce::int64 baseSeed = 0;
};

using CancelFn = std::function<bool()>;
using ProgressFn = std::function<void (int done, int total, juce::String status)>;

GenerateOutcome generateDrumBatch (const DrumRequest&,
                                   LibraryDatabase&,
                                   const std::vector<LibraryItem>& kicks,
                                   const std::vector<LibraryItem>& claps,
                                   const std::vector<LibraryItem>& hats,
                                   const std::vector<LibraryItem>& rides,
                                   CancelFn, ProgressFn);

GenerateOutcome generateBassBatch (const BassRequest&,
                                   LibraryDatabase&,
                                   const std::vector<LibraryItem>& bassShots,
                                   CancelFn, ProgressFn);

GenerateOutcome generateSpliceBatch (const SpliceRequest&,
                                     LibraryDatabase&,
                                     CancelFn, ProgressFn);

juce::StringArray bassPatternIds (Genre g, Variation v);

} // namespace foundry
