#pragma once

#include "SequencerGrid.h"
#include "Widgets.h"
#include "../PluginProcessor.h"

namespace pt
{

class DrumPanel : public juce::Component
{
public:
    std::function<void()> onOpenSettings;

    explicit DrumPanel (FoundryAudioProcessor& p);

    void setTheme (const Theme& t);
    void syncFromProcessor();
    void applyLock (const foundry::LibraryItem& item);
    void applyPatternFile (const foundry::PatternFile& pf, const juce::String& name);
    void refreshPatternList();

    std::function<void()> onPatternsChanged;

    void paint (juce::Graphics&) override;
    void resized() override;

    SequencerGrid grid;

private:
    void generate();
    void refreshLockLabels();

    FoundryAudioProcessor& processor;
    juce::ComboBox styleBox;
    juce::ComboBox tempoBox;
    juce::TextEditor bpmBox;
    juce::ComboBox barsBox;
    Stepper quantity { "QUANTITY", 1, 50, 10 };
    juce::TextButton fullToggle { "FULL LOOP" };
    juce::TextButton stemsToggle { "STEMS" };
    juce::TextButton generateButton { "GENERATE DRUM LOOPS" };
    juce::TextButton resetButton { "RESET" };
    juce::TextButton saveDefaultButton { "SAVE DEFAULT" };
    juce::TextButton savePatternButton { "SAVE PATTERN" };
    juce::ComboBox loadPatternBox;
    juce::Array<juce::File> patternFiles;
    juce::TextButton lockButtons[4];
    juce::Label lockLabels[4];
    StatusBanner banner;
    Theme theme { getTheme() };
};

} // namespace pt
