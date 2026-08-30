#pragma once

#include "../Params/ParamIDs.h"

namespace pt
{

/** The browser's fixed sidebar. Pre-made so a user never has to build the
    obvious folders by hand; "All" is synthesised by the browser and is not
    stored on any preset. */
inline juce::StringArray presetCategories()
{
    return { "Delta", "Theta", "Alpha", "Beta", "Gamma", "Tones", "Special", "User" };
}

struct FactoryPreset
{
    juce::String name;
    juce::String category { "Special" };
    /** Full parameter IDs mapped to real-world values. Anything not listed
        falls back to that parameter's default. */
    std::vector<std::pair<juce::String, float>> values;
};

const std::vector<FactoryPreset>& getFactoryPresets();

} // namespace pt
