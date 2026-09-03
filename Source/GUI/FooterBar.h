#pragma once

#include "KnobComponent.h"
#include "../PluginProcessor.h"

namespace pt
{

//==============================================================================
/** Preview transport, filename, waveform strip, volume, zoom. No A/B. */
class FooterBar : public juce::Component,
                  private juce::Timer
{
public:
    FooterBar (FoundryAudioProcessor& processor);
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

    FoundryAudioProcessor& processor;
    foundry::PreviewEngine& preview;

    juce::TextButton playButton { "PLAY" };
    juce::TextButton stopButton { "STOP" };
    juce::TextButton syncButton { "SYNC" };
    Knob volumeKnob;
    juce::ComboBox zoomBox;

    std::vector<float> peaks;
    Theme theme;
};

} // namespace pt
