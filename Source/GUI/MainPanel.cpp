#include "MainPanel.h"

namespace pt
{

MainPanel::MainPanel (juce::AudioProcessorValueTreeState& state)
    : inputKnob (state, pid::input, "Input"),
      driveKnob (state, pid::drive, "Drive"),
      toneKnob  (state, pid::tone,  "Tone"),
      theme (getTheme())
{
    addAndMakeVisible (inputKnob);
    addAndMakeVisible (driveKnob);
    addAndMakeVisible (toneKnob);
}

void MainPanel::setTheme (const Theme& t)
{
    theme = t;
    inputKnob.setThemeColours (t);
    driveKnob.setThemeColours (t);
    toneKnob.setThemeColours (t);
    repaint();
}

void MainPanel::resized()
{
    auto b = getLocalBounds().reduced (24);

    const int kw = juce::jmin (128, b.getWidth() / 3);
    auto row = b.withSizeKeepingCentre (kw * 3, juce::jmin (b.getHeight(), 180));

    inputKnob.setBounds (row.removeFromLeft (kw));
    driveKnob.setBounds (row.removeFromLeft (kw));
    toneKnob .setBounds (row.removeFromLeft (kw));
}

void MainPanel::paint (juce::Graphics& g)
{
    PluginTemplateLookAndFeel::drawPanel (g, getLocalBounds().toFloat().reduced (10.0f), theme);
}

} // namespace pt
