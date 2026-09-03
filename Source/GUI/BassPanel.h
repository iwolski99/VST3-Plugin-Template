#pragma once

#include "Widgets.h"
#include "../PluginProcessor.h"

namespace pt
{

class BassPanel : public juce::Component
{
public:
    std::function<void()> onOpenSettings;

    explicit BassPanel (FoundryAudioProcessor& p);

    void setTheme (const Theme& t);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    FoundryAudioProcessor& processor;
    juce::ComboBox styleBox, tempoBox, barsBox, variationBox;
    juce::TextEditor bpmBox;
    Stepper quantity { "QUANTITY", 1, 50, 10 };
    Stepper sources { "BASS SOURCES", 2, 4, 3 };
    juce::TextButton generateButton { "GENERATE BASS LOOPS" };
    StatusBanner banner;
    Theme theme { getTheme() };
};

} // namespace pt
