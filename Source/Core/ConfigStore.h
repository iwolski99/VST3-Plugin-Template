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

    double resolveBpm (Genre g, double hostBpm) const;
    double resolveRenderSampleRate (double hostSr) const;

    void saveCustomPattern (const juce::String& name, const DrumPattern& p);
    bool loadCustomPattern (const juce::String& name, DrumPattern& p) const;

private:
    AppConfig config;
    juce::File lastFile;
};

} // namespace foundry
