#pragma once

#include "../Common.h"
#include <cstdint>

namespace foundry
{

inline constexpr const char* kAppName        = "Foundry";
inline constexpr const char* kAppTagline     = "YOUR SOUNDS IN. IDEAS OUT.";
inline constexpr double      kRiddimBpm      = 145.0;
inline constexpr double      kDubstepBpm     = 140.0;
inline constexpr int         kStepsPerBar    = 16;   // 16th-note grid
inline constexpr int         kFadeMs         = 2;
inline constexpr int         kMaxQuantity    = 50;
inline constexpr int         kMinQuantity    = 1;

enum class Genre { Riddim = 0, Dubstep = 1 };
enum class TempoMode { Host = 0, GenreDefault = 1, Custom = 2 };
enum class Variation { Conservative = 0, Normal = 1, Wild = 2 };
enum class SliceRes
{
    Half = 0,
    Quarter,
    Eighth,
    Sixteenth,
    EighthTriplet,
    SixteenthTriplet,
    Mixed
};
enum class SpliceMode
{
    Alternating = 0,
    BlockSwap,
    CallAndResponse,
    Syncopated,
    Burst,
    Weighted
};

enum class Category
{
    DrumLoopsRiddim = 0,
    DrumLoopsDubstep,
    BassLoopsBase,
    BassLoopsSpliced,
    OneShotKick,
    OneShotSnare,
    OneShotClap,
    OneShotHat,
    OneShotRide,
    OneShotBass,
    Unknown
};

enum class FolderRole
{
    Kick = 0,
    Snare,
    Clap,
    Hat,
    Ride,
    Bass,
    GeneratedDrums,
    GeneratedBass
};

enum class SortMode { Name = 0, Newest, Oldest, Bpm, Genre, Favorites };

struct LibraryItem
{
    juce::int64   id = 0;
    juce::String  filename;
    juce::String  path;
    juce::String  fileType;          // oneshot | loop
    Category      category = Category::Unknown;
    double        bpm = 0.0;
    int           bars = 0;
    juce::String  genre;
    juce::int64   dateCreated = 0;
    juce::int64   dateIndexed = 0;
    bool          favorite = false;
    juce::String  generationType;    // drum | bass | splice | source
    juce::int64   generationSeed = 0;
    juce::String  parentSourceIds;   // comma-separated ids
    juce::String  patternId;
    juce::String  engine;
    int           generationDepth = 0;
    bool          existsOnDisk = true;
};

struct SampleLock
{
    bool         locked = false;
    juce::String path;
    juce::String name;
};

struct DrumPattern
{
    int bars = 1;
    std::array<std::vector<uint8_t>, 4> rows; // kick, clap, hat, ride — 16 * bars

    void setBars (int newBars);
    void clear();
    void setStep (int row, int step, bool on);
    bool getStep (int row, int step) const noexcept;
    int  numSteps() const noexcept { return bars * kStepsPerBar; }
    juce::String toJson() const;
    static DrumPattern fromJson (const juce::String& json, int fallbackBars);
};

struct AppConfig
{
    juce::String rootPath;
    std::array<juce::String, 8> folderPaths {}; // indexed by FolderRole
    Genre     defaultGenre     = Genre::Riddim;
    TempoMode tempoMode        = TempoMode::GenreDefault;
    double    customBpm        = kRiddimBpm;
    int       defaultBars      = 2;
    int       defaultQuantity  = 10;
    float     previewVolume    = 0.80f;
    juce::String sampleRateMode = "host"; // host | 44100 | 48000
    int       bitDepth         = 24;
    bool      renderProtection = true;
    int       uiZoom           = 100;
    int       lastTab          = 0;
    juce::String savedDrumPatternJson;
    juce::StringArray customPatternNames;
    juce::StringArray customPatternJson;
};

juce::String genreName (Genre g);
Genre        genreFromName (const juce::String&);
double       genreDefaultBpm (Genre g) noexcept;

juce::String categoryName (Category c);
juce::String categoryKey  (Category c);
Category     categoryFromKey (const juce::String&);
bool         isGeneratedCategory (Category c) noexcept;
bool         isBassLoopCategory (Category c) noexcept;

juce::String folderRoleName (FolderRole r);
juce::String folderRoleKey  (FolderRole r);

Category categoryForRole (FolderRole r) noexcept;
Category drumLoopCategory (Genre g) noexcept;

juce::String sliceResName (SliceRes r);
juce::String spliceModeName (SpliceMode m);
double       sliceBeats (SliceRes r, juce::Random* mixedRng = nullptr);

DrumPattern  defaultDrumPattern (Genre g, int bars);

juce::File appDataDir();
juce::File defaultConfigFile();
juce::File defaultDatabaseFile(); // used until a root is chosen

juce::String legalStem (const juce::String& s);

} // namespace foundry
