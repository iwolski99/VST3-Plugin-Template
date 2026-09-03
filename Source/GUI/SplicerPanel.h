#pragma once

#include "BrowserList.h"
#include "Widgets.h"
#include "../PluginProcessor.h"

namespace pt
{

class SplicerPanel : public juce::Component
{
public:
    std::function<void()> onOpenSettings;

    explicit SplicerPanel (FoundryAudioProcessor& p);

    void setTheme (const Theme& t);
    void refresh();
    void addSource (const foundry::LibraryItem& item);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void rebuildPool();
    void generate();

    FoundryAudioProcessor& processor;
    BrowserList pool;
    juce::ComboBox sliceBox, patternBox, variationBox, barsBox;
    Stepper quantity { "QUANTITY", 1, 50, 20 };
    juce::TextButton generateButton { "GENERATE RIFF VARIATIONS" };
    juce::TextButton clearButton { "CLEAR SELECTION" };
    juce::StringArray selectedPaths;
    StatusBanner banner;
    Theme theme { getTheme() };
};

} // namespace pt
