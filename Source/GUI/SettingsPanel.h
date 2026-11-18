#pragma once

#include "Widgets.h"
#include "../PluginProcessor.h"

namespace pt
{

class SettingsPanel : public juce::Component
{
public:
    std::function<void()> onClose;
    std::function<void()> onLibraryChanged;
    std::function<void (const juce::String&)> onThemeChanged;

    explicit SettingsPanel (FoundryAudioProcessor& p);

    void setTheme (const Theme& t);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void pickFolder (foundry::FolderRole role);
    void pickRoot();
    void refreshLabels();
    void persist();

    FoundryAudioProcessor& processor;
    juce::TextButton closeButton { "CLOSE" };
    juce::TextButton rootButton { "CHOOSE ROOT" };
    juce::TextButton createButton { "CREATE RECOMMENDED STRUCTURE" };
    juce::TextButton refreshButton { "REFRESH LIBRARY" };
    juce::TextButton rebuildButton { "REBUILD DATABASE" };
    juce::TextButton openDbButton { "OPEN DATABASE FOLDER" };
    juce::TextButton folderButtons[foundry::kNumFolderRoles];
    juce::Label folderLabels[foundry::kNumFolderRoles];
    juce::Label rootLabel;
    juce::ComboBox rateBox, genreBox, protectBox, themeBox;
    juce::TextEditor bpmBox;
    StatusBanner banner;
    std::unique_ptr<juce::FileChooser> chooser;
    Theme theme { getTheme() };
};

} // namespace pt
