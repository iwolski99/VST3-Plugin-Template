#pragma once

#include "KnobComponent.h"
#include "../Core/PreviewEngine.h"

namespace pt
{

//==============================================================================
/** Preview transport, filename, waveform strip, volume, zoom. No A/B. */
class FooterBar : public juce::Component,
                  private juce::Timer
{
public:
    FooterBar (juce::AudioProcessorValueTreeState& state, foundry::PreviewEngine& preview);
    ~FooterBar() override;

    std::function<void (int)> onZoomChanged;

    void setTheme (const Theme& t);
    void setZoomPercent (int percent);
    void refresh();

    static const juce::Array<int>& zoomOptions();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    foundry::PreviewEngine& preview;

    juce::TextButton playButton { "PLAY" };
    juce::TextButton stopButton { "STOP" };
    Knob volumeKnob;
    juce::ComboBox zoomBox;

    std::vector<float> peaks;
    Theme theme;
};

} // namespace pt
