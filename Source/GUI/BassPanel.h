#pragma once

#include "SequencerGrid.h"
#include "Widgets.h"
#include "../PluginProcessor.h"

namespace pt
{

/** Bass riff generator. Either rolls Foundry's rhythm templates (the
    original behaviour) or plays your own pattern from the grid, where rows
    A-D are the bass sources. Lock a source to keep a sound you like across
    the whole batch; open any generated bass loop to get its exact pattern
    and sounds back. */
class BassPanel : public juce::Component
{
public:
    std::function<void()> onOpenSettings;
    std::function<void()> onPatternsChanged;

    explicit BassPanel (FoundryAudioProcessor& p);

    void setTheme (const Theme& t);
    void syncFromProcessor();
    void applyPatternFile (const foundry::PatternFile& pf, const juce::String& name);
    void applyLock (int row, const foundry::LibraryItem& item);
    void refreshPatternList();

    void paint (juce::Graphics&) override;
    void resized() override;

    SequencerGrid grid;

private:
    void setUsePattern (bool use);
    void refreshLockLabels();

    FoundryAudioProcessor& processor;
    juce::ComboBox styleBox, tempoBox, barsBox, variationBox;
    juce::TextEditor bpmBox;
    Stepper quantity { "QUANTITY", 1, 50, 10 };
    Stepper sources { "SOURCES", 2, 4, 3 };
    juce::TextButton templatesButton { "TEMPLATES" }, patternButton { "MY PATTERN" };
    juce::TextButton lockButtons[4];
    juce::Label lockLabels[4];
    juce::TextButton generateButton { "GENERATE BASS LOOPS" };
    juce::TextButton clearButton { "CLEAR GRID" };
    juce::TextButton savePatternButton { "SAVE PATTERN" };
    juce::ComboBox loadPatternBox;
    juce::Array<juce::File> patternFiles;
    StatusBanner banner;
    Theme theme { getTheme() };
};

} // namespace pt
