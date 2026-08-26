#pragma once

#include "KnobComponent.h"
#include "../Presets/PresetManager.h"

namespace pt
{

//==============================================================================
class PagerArrow : public juce::Button
{
public:
    explicit PagerArrow (bool pointsRight);

    void setColours (juce::Colour accent, juce::Colour dim);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    bool right;
    juce::Colour accentCol, dimCol;
};

//==============================================================================
/** The always-visible docked strip: preset browser and A/B compare on the
    left, the global Mix / Output knobs in the middle, and a UI zoom selector
    on the right. */
class FooterBar : public juce::Component,
                  private juce::ChangeListener
{
public:
    FooterBar (juce::AudioProcessorValueTreeState& state, PresetManager& presetManager);
    ~FooterBar() override;

    std::function<void (int)>  onZoomChanged;   // percent
    std::function<void()>      onRequestSave;   // the editor owns the naming panel
    std::function<void()>      onBrowsePresets; // the editor owns the browser overlay

    /** Public so the browser can push the footer's label back in step after
        loading something. */
    void refreshPresetList();

    void setTheme (const Theme& t);
    void setZoomPercent (int percent);

    static const juce::Array<int>& zoomOptions();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void updateSlotButtons();

    juce::AudioProcessorValueTreeState& apvts;
    PresetManager& presets;

    PagerArrow prevPreset { false }, nextPreset { true };
    juce::TextButton presetButton;
    juce::TextButton saveButton { "SAVE" };
    juce::TextButton slotA { "A" }, slotB { "B" };

    Knob mixKnob, outputKnob;

    juce::ComboBox zoomBox;

    Theme theme;
    bool updatingList = false;
};

} // namespace pt
