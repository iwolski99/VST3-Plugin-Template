#pragma once

#include "KnobComponent.h"

namespace pt
{

//==============================================================================
/** The plugin's one content page: a single smoked-glass panel holding the
    effect's controls. Swap the set of Knob members for whatever your real
    DSP needs - the panel chrome and layout pattern stay the same. */
class MainPanel : public juce::Component
{
public:
    explicit MainPanel (juce::AudioProcessorValueTreeState& state);

    void setTheme (const Theme& t);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    Knob inputKnob, driveKnob, toneKnob;

    Theme theme;
};

} // namespace pt
