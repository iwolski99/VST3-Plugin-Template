#include "Types.h"
#include <algorithm>

namespace foundry
{

void DrumPattern::setBars (int newBars)
{
    newBars = juce::jlimit (1, 8, newBars);
    const int n = newBars * kStepsPerBar;

    for (auto& row : rows)
    {
        row.resize ((size_t) n, 0);
        if ((int) row.size() > n)
            row.resize ((size_t) n);
    }

    bars = newBars;
}

void DrumPattern::clear()
{
    for (auto& row : rows)
        std::fill (row.begin(), row.end(), 0);
}

void DrumPattern::setStep (int row, int step, bool on)
{
    if (row < 0 || row > 3)
        return;

    auto& r = rows[(size_t) row];
    if (step < 0 || step >= (int) r.size())
        return;

    r[(size_t) step] = on ? 1 : 0;
}

bool DrumPattern::getStep (int row, int step) const noexcept
{
    if (row < 0 || row > 3)
        return false;

    const auto& r = rows[(size_t) row];
    if (step < 0 || step >= (int) r.size())
        return false;

    return r[(size_t) step] != 0;
}

juce::String DrumPattern::toJson() const
{
    auto obj = juce::DynamicObject::Ptr (new juce::DynamicObject());
    obj->setProperty ("bars", bars);

    auto encode = [] (const std::vector<uint8_t>& row) -> juce::var
    {
        juce::Array<juce::var> a;
        a.ensureStorageAllocated ((int) row.size());
        for (auto v : row)
            a.add ((int) v);
        return a;
    };

    obj->setProperty ("kick", encode (rows[0]));
    obj->setProperty ("clap", encode (rows[1]));
    obj->setProperty ("hat",  encode (rows[2]));
    obj->setProperty ("ride", encode (rows[3]));
    return juce::JSON::toString (juce::var (obj.get()), false);
}

DrumPattern DrumPattern::fromJson (const juce::String& json, int fallbackBars)
{
    DrumPattern p = defaultDrumPattern (Genre::Riddim, fallbackBars);

    auto parsed = juce::JSON::parse (json);
    auto* obj = parsed.getDynamicObject();
    if (obj == nullptr)
        return p;

    const int bars = juce::jlimit (1, 8, (int) obj->getProperty ("bars"));
    p.setBars (bars);

    auto decode = [&] (const juce::Identifier& key, int row)
    {
        const auto& v = obj->getProperty (key);
        if (auto* arr = v.getArray())
        {
            const int n = juce::jmin (p.numSteps(), arr->size());
            for (int i = 0; i < n; ++i)
                p.rows[(size_t) row][(size_t) i] = (uint8_t) ((int) arr->getUnchecked (i) != 0);
        }
    };

    decode ("kick", 0);
    decode ("clap", 1);
    decode ("hat",  2);
    decode ("ride", 3);
    return p;
}

//==============================================================================
juce::String genreName (Genre g)
{
    return g == Genre::Dubstep ? "Dubstep" : "Riddim";
}

Genre genreFromName (const juce::String& s)
{
    return s.equalsIgnoreCase ("Dubstep") ? Genre::Dubstep : Genre::Riddim;
}

double genreDefaultBpm (Genre g) noexcept
{
    return g == Genre::Dubstep ? kDubstepBpm : kRiddimBpm;
}

juce::String categoryName (Category c)
{
    switch (c)
    {
        case Category::DrumLoopsRiddim:  return "Riddim Drum Loops";
        case Category::DrumLoopsDubstep: return "Dubstep Drum Loops";
        case Category::BassLoopsBase:    return "Base Riffs";
        case Category::BassLoopsSpliced: return "Spliced Riffs";
        case Category::OneShotKick:      return "Kicks";
        case Category::OneShotSnare:     return "Snares";
        case Category::OneShotClap:      return "Claps";
        case Category::OneShotHat:       return "Closed Hats";
        case Category::OneShotRide:      return "Rides";
        case Category::OneShotBass:      return "Bass";
        case Category::Riff:             return "Riffs";
        case Category::Unknown:
        default:                         return "Unknown";
    }
}

juce::String categoryKey (Category c)
{
    switch (c)
    {
        case Category::DrumLoopsRiddim:  return "drum_riddim";
        case Category::DrumLoopsDubstep: return "drum_dubstep";
        case Category::BassLoopsBase:    return "bass_base";
        case Category::BassLoopsSpliced: return "bass_spliced";
        case Category::OneShotKick:      return "kick";
        case Category::OneShotSnare:     return "snare";
        case Category::OneShotClap:      return "clap";
        case Category::OneShotHat:       return "hat";
        case Category::OneShotRide:      return "ride";
        case Category::OneShotBass:      return "bass";
        case Category::Riff:             return "riff";
        case Category::Unknown:
        default:                         return "unknown";
    }
}

Category categoryFromKey (const juce::String& k)
{
    if (k == "drum_riddim")  return Category::DrumLoopsRiddim;
    if (k == "drum_dubstep") return Category::DrumLoopsDubstep;
    if (k == "bass_base")    return Category::BassLoopsBase;
    if (k == "bass_spliced") return Category::BassLoopsSpliced;
    if (k == "kick")         return Category::OneShotKick;
    if (k == "snare")        return Category::OneShotSnare;
    if (k == "clap")         return Category::OneShotClap;
    if (k == "hat")          return Category::OneShotHat;
    if (k == "ride")         return Category::OneShotRide;
    if (k == "bass")         return Category::OneShotBass;
    if (k == "riff")         return Category::Riff;
    return Category::Unknown;
}

bool isGeneratedCategory (Category c) noexcept
{
    return c == Category::DrumLoopsRiddim || c == Category::DrumLoopsDubstep
        || c == Category::BassLoopsBase   || c == Category::BassLoopsSpliced
        || c == Category::Riff;
}

bool isBassLoopCategory (Category c) noexcept
{
    return c == Category::BassLoopsBase || c == Category::BassLoopsSpliced;
}

juce::String folderRoleName (FolderRole r)
{
    switch (r)
    {
        case FolderRole::Kick:           return "Kicks";
        case FolderRole::Snare:          return "Snares";
        case FolderRole::Clap:           return "Claps";
        case FolderRole::Hat:            return "Closed Hats";
        case FolderRole::Ride:           return "Rides";
        case FolderRole::Bass:           return "Bass One-Shots";
        case FolderRole::GeneratedDrums: return "Generated Drum Loops";
        case FolderRole::GeneratedBass:  return "Generated Bass Loops";
        case FolderRole::Riffs:          return "My Riffs";
        default:                         return {};
    }
}

juce::String folderRoleKey (FolderRole r)
{
    switch (r)
    {
        case FolderRole::Kick:           return "kick";
        case FolderRole::Snare:          return "snare";
        case FolderRole::Clap:           return "clap";
        case FolderRole::Hat:            return "hat";
        case FolderRole::Ride:           return "ride";
        case FolderRole::Bass:           return "bass";
        case FolderRole::GeneratedDrums: return "generatedDrums";
        case FolderRole::GeneratedBass:  return "generatedBass";
        case FolderRole::Riffs:          return "riffs";
        default:                         return {};
    }
}

Category categoryForRole (FolderRole r) noexcept
{
    switch (r)
    {
        case FolderRole::Kick:  return Category::OneShotKick;
        case FolderRole::Snare: return Category::OneShotSnare;
        case FolderRole::Clap:  return Category::OneShotClap;
        case FolderRole::Hat:   return Category::OneShotHat;
        case FolderRole::Ride:  return Category::OneShotRide;
        case FolderRole::Bass:  return Category::OneShotBass;
        case FolderRole::Riffs: return Category::Riff;
        case FolderRole::GeneratedDrums:
        case FolderRole::GeneratedBass:
        default:                return Category::Unknown;
    }
}

Category drumLoopCategory (Genre g) noexcept
{
    return g == Genre::Dubstep ? Category::DrumLoopsDubstep : Category::DrumLoopsRiddim;
}

juce::String sliceResName (SliceRes r)
{
    switch (r)
    {
        case SliceRes::Half:             return "1/2";
        case SliceRes::Quarter:          return "1/4";
        case SliceRes::Eighth:           return "1/8";
        case SliceRes::Sixteenth:        return "1/16";
        case SliceRes::EighthTriplet:    return "1/8T";
        case SliceRes::SixteenthTriplet: return "1/16T";
        case SliceRes::Mixed:            return "Mixed";
        default:                         return "1/4";
    }
}

juce::String spliceModeName (SpliceMode m)
{
    switch (m)
    {
        case SpliceMode::Alternating:     return "Alternating";
        case SpliceMode::BlockSwap:       return "Block Swap";
        case SpliceMode::CallAndResponse: return "Call & Response";
        case SpliceMode::Syncopated:      return "Syncopated";
        case SpliceMode::Burst:           return "Burst";
        case SpliceMode::Weighted:        return "Weighted";
        default:                          return "Alternating";
    }
}

double sliceBeats (SliceRes r, juce::Random* mixedRng)
{
    switch (r)
    {
        case SliceRes::Half:             return 2.0;
        case SliceRes::Quarter:          return 1.0;
        case SliceRes::Eighth:           return 0.5;
        case SliceRes::Sixteenth:        return 0.25;
        case SliceRes::EighthTriplet:    return 2.0 / 3.0;
        case SliceRes::SixteenthTriplet: return 1.0 / 3.0;
        case SliceRes::Mixed:
            if (mixedRng != nullptr)
                return mixedRng->nextBool() ? 0.5 : 0.25;
            return 0.5;
        default:                         return 1.0;
    }
}

DrumPattern defaultDrumPattern (Genre g, int bars)
{
    DrumPattern p;
    p.setBars (juce::jlimit (1, 8, bars));

    const int n = p.numSteps();

    for (int s = 0; s < n; ++s)
    {
        const int inBar = s % kStepsPerBar;

        // Hats: eighth notes. Ride: quarter notes. Shared by both genres.
        p.rows[2][(size_t) s] = (inBar % 2 == 0) ? 1 : 0;
        p.rows[3][(size_t) s] = (inBar % 4 == 0) ? 1 : 0;

        if (g == Genre::Riddim)
        {
            // Foundational riddim: kick on 1, the '+' of 2, and the '+' of 4.
            // Clap on 3 — the half-time backbeat.
            p.rows[0][(size_t) s] = (inBar == 0 || inBar == 6 || inBar == 14) ? 1 : 0;
            p.rows[1][(size_t) s] = (inBar == 8) ? 1 : 0;
        }
        else
        {
            // Dubstep half-time: kick on 1, clap on 3.
            p.rows[0][(size_t) s] = (inBar == 0) ? 1 : 0;
            p.rows[1][(size_t) s] = (inBar == 8) ? 1 : 0;
        }
    }

    return p;
}

juce::File appDataDir()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile (kAppName);

    if (! dir.isDirectory())
        dir.createDirectory();

    return dir;
}

juce::File defaultConfigFile()
{
    return appDataDir().getChildFile ("config.json");
}

juce::File defaultDatabaseFile()
{
    return appDataDir().getChildFile ("library.db");
}

juce::String legalStem (const juce::String& s)
{
    return juce::File::createLegalFileName (s);
}

const juce::String& audioFileWildcard()
{
    static const juce::String w ("*.wav;*.aif;*.aiff;*.flac;*.ogg");
    return w;
}

bool isSupportedAudioFile (const juce::File& f)
{
    return f.hasFileExtension ("wav;aif;aiff;flac;ogg");
}

} // namespace foundry
