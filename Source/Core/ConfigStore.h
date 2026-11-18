#pragma once

#include "Types.h"

namespace foundry
{

class ConfigStore
{
public:
    ConfigStore();

    bool load (const juce::File& file, juce::String& error);
    bool save (const juce::File& file, juce::String& error) const;

    /** Saves only per-session preferences (genre, bars, zoom, ...), merged
        into whatever is currently on disk. Use this for automatic saves. */
    bool savePreferences (const juce::File& file, juce::String& error) const;

    AppConfig&       get() noexcept       { return config; }
    const AppConfig& get() const noexcept { return config; }

    juce::File folder (FolderRole role) const;
    void setFolder (FolderRole role, const juce::File& dir);

    juce::File root() const;
    void setRoot (const juce::File& dir);

    juce::File databaseFile() const;

    /** Creates the recommended library tree under root and maps every role. */
    bool createRecommendedStructure (juce::String& error);

    juce::File generatedDrumDir (Genre g) const;
    juce::File generatedBassDir (bool spliced) const;

    /** Where rendered riffs go. Falls back to <root>/Riffs, or to the app data
        folder before a root is chosen, and is created on demand. */
    juce::File riffDir() const;

    /** Pattern library: <root>/Patterns, with Drums and Bass inside. */
    juce::File patternsRoot() const;
    juce::File patternDir (bool bass) const;

    /** Top-level folders of the batch generators (parents of the per-genre /
        per-type folders), whether mapped or defaulted. */
    juce::File generatedDrumsRoot() const;
    juce::File generatedBassRoot() const;

    double resolveBpm (Genre g, double hostBpm) const;
    double resolveRenderSampleRate (double hostSr) const;

    void saveCustomPattern (const juce::String& name, const DrumPattern& p);
    bool loadCustomPattern (const juce::String& name, DrumPattern& p) const;

private:
    AppConfig config;
    juce::File lastFile;
};

} // namespace foundry
