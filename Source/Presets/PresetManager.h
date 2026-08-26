#pragma once

#include "FactoryPresets.h"
#include "../Params/ParameterLayout.h"

namespace pt
{

//==============================================================================
/** Factory bank + user presets on disk + A/B compare. */
class PresetManager : public juce::ChangeBroadcaster
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& state);

    void rescanUserPresets();

    const juce::StringArray& getFactoryNames() const noexcept { return factoryNames; }
    const juce::StringArray& getUserNames()    const noexcept { return userNames; }

    /** Parallel to getUserNames(). The folder a saved preset was filed under,
        read back from the file itself so it survives a rescan. */
    const juce::StringArray& getUserCategories() const noexcept { return userCategories; }

    juce::String getCurrentName() const { return currentName; }
    bool isModified() const noexcept { return modified; }
    void markModified();

    void loadFactory (int index);
    void loadUser (int index);
    bool saveUser (const juce::String& name, const juce::String& category = "User");
    bool deleteUser (const juce::String& name);

    /** Steps through factory presets then user presets, wrapping. */
    void step (int delta);

    void resetToDefaults();

    juce::File getUserFolder() const;

    //== A/B ===================================================================
    int  getActiveSlot() const noexcept { return activeSlot; }
    void setActiveSlot (int slot);
    void copyActiveSlotToOther();

private:
    void applyValues (const std::vector<std::pair<juce::String, float>>& values);
    void applyStateTree (const juce::ValueTree& tree);

    juce::AudioProcessorValueTreeState& apvts;

    juce::StringArray factoryNames, userNames, userCategories;
    juce::Array<juce::File> userFiles;

    juce::String currentName { "Init" };
    bool modified = false;

    int activeSlot = 0;
    juce::ValueTree slots[2];
    juce::String    slotNames[2] { "Init", "Init" };
};

} // namespace pt
