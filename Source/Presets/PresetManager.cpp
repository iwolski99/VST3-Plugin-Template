#include "PresetManager.h"

namespace pt
{

// Both of these embed the plugin's name - update them together with the rest
// of the rename (see scripts/rename_plugin.py).
static constexpr const char* kPresetExtension = ".pluginpreset";
static constexpr const char* kPresetFolderName = "PluginTemplate";

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& state)
    : apvts (state)
{
    for (const auto& p : getFactoryPresets())
        factoryNames.add (p.name);

    rescanUserPresets();
}

juce::File PresetManager::getUserFolder() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile (kPresetFolderName)
                   .getChildFile ("Presets");

    if (! dir.isDirectory())
        dir.createDirectory();

    return dir;
}

void PresetManager::rescanUserPresets()
{
    userNames.clear();
    userFiles.clear();
    userCategories.clear();

    for (const auto& f : getUserFolder().findChildFiles (juce::File::findFiles, false,
                                                         juce::String ("*") + kPresetExtension))
    {
        userFiles.add (f);
        userNames.add (f.getFileNameWithoutExtension());

        // Stored on the preset itself rather than in a side-car index, so
        // copying a preset file between machines carries its folder with it.
        juce::String cat ("User");

        if (auto xml = juce::XmlDocument::parse (f))
            cat = xml->getStringAttribute ("presetCategory", "User");

        userCategories.add (cat);
    }

    // findChildFiles does not promise an order; keep the browser alphabetical.
    juce::Array<int> order;
    for (int i = 0; i < userNames.size(); ++i)
        order.add (i);

    std::sort (order.begin(), order.end(), [this] (int a, int b)
    {
        return userNames[a].compareIgnoreCase (userNames[b]) < 0;
    });

    juce::StringArray sortedNames, sortedCategories;
    juce::Array<juce::File> sortedFiles;
    for (int i : order)
    {
        sortedCategories.add (userCategories[i]);
        sortedNames.add (userNames[i]);
        sortedFiles.add (userFiles[i]);
    }

    userNames = sortedNames;
    userFiles = sortedFiles;
    userCategories = sortedCategories;

    sendChangeMessage();
}

//==============================================================================
void PresetManager::markModified()
{
    if (! modified)
    {
        modified = true;
        sendChangeMessage();
    }
}

void PresetManager::resetToDefaults()
{
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    currentName = "Init";
    modified = false;
    sendChangeMessage();
}

void PresetManager::applyValues (const std::vector<std::pair<juce::String, float>>& values)
{
    resetToDefaults();

    for (const auto& kv : values)
        if (auto* p = apvts.getParameter (kv.first))
            p->setValueNotifyingHost (p->convertTo0to1 (kv.second));
}

void PresetManager::loadFactory (int index)
{
    const auto& all = getFactoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) all.size()))
        return;

    const auto& preset = all[(size_t) index];
    applyValues (preset.values);

    currentName = preset.name;
    modified = false;
    sendChangeMessage();
}

void PresetManager::applyStateTree (const juce::ValueTree& tree)
{
    if (! tree.isValid())
        return;

    // Push through the parameters rather than replacing the tree wholesale,
    // so the host sees automation-visible changes and the editor follows
    // along.
    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        const auto child = tree.getChild (i);
        if (! child.hasType ("PARAM"))
            continue;

        const auto id = child.getProperty ("id").toString();
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) child.getProperty ("value")));
    }
}

void PresetManager::loadUser (int index)
{
    if (! juce::isPositiveAndBelow (index, userFiles.size()))
        return;

    if (auto xml = juce::XmlDocument::parse (userFiles[index]))
    {
        resetToDefaults();
        applyStateTree (juce::ValueTree::fromXml (*xml));
        currentName = userNames[index];
        modified = false;
        sendChangeMessage();
    }
}

bool PresetManager::saveUser (const juce::String& name, const juce::String& category)
{
    const auto clean = juce::File::createLegalFileName (name.trim());
    if (clean.isEmpty())
        return false;

    auto file = getUserFolder().getChildFile (clean + kPresetExtension);
    auto state = apvts.copyState();

    if (auto xml = state.createXml())
    {
        xml->setAttribute ("presetCategory", category.isEmpty() ? juce::String ("User") : category);

        if (! xml->writeTo (file))
            return false;

        rescanUserPresets();
        currentName = clean;
        modified = false;
        sendChangeMessage();
        return true;
    }

    return false;
}

bool PresetManager::deleteUser (const juce::String& name)
{
    auto file = getUserFolder().getChildFile (juce::File::createLegalFileName (name) + kPresetExtension);

    if (file.existsAsFile() && file.deleteFile())
    {
        rescanUserPresets();
        return true;
    }

    return false;
}

void PresetManager::step (int delta)
{
    const int total = factoryNames.size() + userNames.size();
    if (total == 0)
        return;

    int current = factoryNames.indexOf (currentName);
    if (current < 0)
    {
        const int u = userNames.indexOf (currentName);
        current = (u >= 0 ? factoryNames.size() + u : -1);
    }

    int next = (current < 0 ? (delta >= 0 ? 0 : total - 1) : current + delta);
    next = ((next % total) + total) % total;

    if (next < factoryNames.size())
        loadFactory (next);
    else
        loadUser (next - factoryNames.size());
}

//==============================================================================
void PresetManager::setActiveSlot (int slot)
{
    slot = juce::jlimit (0, 1, slot);
    if (slot == activeSlot)
        return;

    slots[activeSlot] = apvts.copyState();
    slotNames[activeSlot] = currentName;

    if (slots[slot].isValid())
    {
        applyStateTree (slots[slot]);
        currentName = slotNames[slot];
    }
    else
    {
        // First visit to the other slot mirrors the current sound.
        slots[slot] = slots[activeSlot].createCopy();
        slotNames[slot] = currentName;
    }

    activeSlot = slot;
    sendChangeMessage();
}

void PresetManager::copyActiveSlotToOther()
{
    const int other = 1 - activeSlot;
    slots[other] = apvts.copyState();
    slotNames[other] = currentName;
    sendChangeMessage();
}

} // namespace pt
