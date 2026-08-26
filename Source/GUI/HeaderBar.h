#pragma once

#include "KnobComponent.h"

namespace pt
{

//==============================================================================
/** Persistent chrome above the main content: wordmark, plugin title, and a
    Bypass toggle. Kept deliberately simple - if your plugin has more than one
    page or mode, this is the place to add a page switcher, following
    PrismReverbVST's HeaderBar (pager arrows + jump-dots) as a reference. */
class HeaderBar : public juce::Component
{
public:
    HeaderBar (juce::AudioProcessorValueTreeState& state, const juce::String& parameterID);

    void setTheme (const Theme& t);
    void setTitle (const juce::String& title, const juce::String& tagline = {});

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    Toggle bypassToggle;
    juce::String pluginTitle, pluginTagline;
    Theme theme;
};

} // namespace pt
