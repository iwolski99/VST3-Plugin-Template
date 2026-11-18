#pragma once

#include "TileBrowser.h"

namespace pt
{

/** The main page: eight slots on a beat grid, each playing only the start of
    its sample. Drop samples in from the browser (or from Explorer/Finder),
    slide where each one starts by dragging its waveform, set how long each
    one lasts, lock the keepers and roll the dice on the rest. RENDER saves
    the riff as a WAV in My Riffs; the DRAG handle drops it straight into the
    DAW's playlist. */
class RiffMakerPanel : public juce::Component,
                       private juce::ChangeListener,
                       private juce::Timer
{
public:
    std::function<std::vector<foundry::LibraryItem>()> getDicePool;
    std::function<void (const juce::File&)> onRendered;

    explicit RiffMakerPanel (FoundryAudioProcessor& p);
    ~RiffMakerPanel() override;

    void setTheme (const Theme& t);

    /** slot < 0 picks the first empty slot. */
    void loadIntoSlot (int slot, const juce::File& file);
    void loadFilesFrom (int firstSlot, const juce::StringArray& files);
    void showMessage (const juce::String& text, bool isError);

    void render();
    void rollDice();
    void syncFromProcessor();

    void paint (juce::Graphics&) override;
    void resized() override;

    class SlotView;
    class DragHandle;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void setGrid (int steps);

    FoundryAudioProcessor& processor;

    juce::TextButton playButton { "PLAY" }, stopButton { "STOP" }, syncButton { "SYNC" };
    juce::ComboBox gridBox;
    juce::TextEditor bpmEditor;
    juce::TextButton diceButton { "DICE" }, undoButton { "UNDO" }, redoButton { "REDO" }, clearButton { "CLEAR" };
    juce::TextButton renderButton { "RENDER" };
    std::unique_ptr<DragHandle> dragHandle;
    std::array<std::unique_ptr<SlotView>, foundry::kRiffSlots> slots;
    StatusBanner banner;

    int lastSlotShown = -2;
    bool lastHostTempo = false;
    double lastBpmShown = 0.0;

    Theme theme { getTheme() };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RiffMakerPanel)
};

} // namespace pt
