#include "BassPanel.h"

namespace pt
{

BassPanel::BassPanel (FoundryAudioProcessor& p)
    : processor (p)
{
    styleBox.addItem ("Riddim", 1);
    styleBox.addItem ("Dubstep", 2);
    styleBox.setSelectedId (processor.getUiGenre() == foundry::Genre::Dubstep ? 2 : 1, juce::dontSendNotification);
    styleBox.onChange = [this]
    {
        processor.setUiGenre (styleBox.getSelectedId() == 2 ? foundry::Genre::Dubstep : foundry::Genre::Riddim);
    };

    tempoBox.addItem ("Host", 1);
    tempoBox.addItem ("Genre", 2);
    tempoBox.addItem ("Custom", 3);
    tempoBox.setSelectedId ((int) processor.getConfig().get().tempoMode + 1, juce::dontSendNotification);
    tempoBox.onChange = [this]
    {
        processor.getConfig().get().tempoMode = (foundry::TempoMode) (tempoBox.getSelectedId() - 1);
        bpmBox.setEnabled (tempoBox.getSelectedId() == 3);
    };

    bpmBox.setInputRestrictions (6, "0123456789.");
    bpmBox.setText (juce::String (processor.getConfig().get().customBpm, 0), juce::dontSendNotification);
    bpmBox.setEnabled (tempoBox.getSelectedId() == 3);
    bpmBox.onFocusLost = [this]
    {
        processor.getConfig().get().customBpm = juce::jlimit (40.0, 300.0, bpmBox.getText().getDoubleValue());
    };

    barsBox.addItem ("1 bar", 1);
    barsBox.addItem ("2 bars", 2);
    barsBox.addItem ("4 bars", 3);
    barsBox.setSelectedId (processor.getUiBars() == 4 ? 3 : processor.getUiBars(), juce::dontSendNotification);
    barsBox.onChange = [this]
    {
        processor.setUiBars (barsBox.getSelectedId() == 3 ? 4 : barsBox.getSelectedId());
    };

    variationBox.addItem ("Conservative", 1);
    variationBox.addItem ("Normal", 2);
    variationBox.addItem ("Wild", 3);
    variationBox.setSelectedId (2, juce::dontSendNotification);

    quantity.setValue (processor.getUiQuantity(), false);
    quantity.onChanged = [this] (int v) { processor.setUiQuantity (v); };

    generateButton.onClick = [this]
    {
        processor.getConfig().get().customBpm = juce::jlimit (40.0, 300.0,
            bpmBox.getText().getDoubleValue() > 0 ? bpmBox.getText().getDoubleValue()
                                                  : processor.getConfig().get().customBpm);
        const auto var = (foundry::Variation) (variationBox.getSelectedId() - 1);
        auto err = processor.startBassGenerate (sources.getValue(), var);
        banner.setMessage (err, err.isNotEmpty(), err.containsIgnoreCase ("folder") || err.containsIgnoreCase ("sample"));
    };

    banner.onOpenSettings = [this] { if (onOpenSettings) onOpenSettings(); };

    addAndMakeVisible (styleBox);
    addAndMakeVisible (tempoBox);
    addAndMakeVisible (bpmBox);
    addAndMakeVisible (barsBox);
    addAndMakeVisible (variationBox);
    addAndMakeVisible (quantity);
    addAndMakeVisible (sources);
    addAndMakeVisible (generateButton);
    addChildComponent (banner);
}

void BassPanel::setTheme (const Theme& t)
{
    theme = t;
    quantity.setTheme (t);
    sources.setTheme (t);
    banner.setTheme (t);
    generateButton.setColour (juce::TextButton::buttonColourId, t.accent);
    generateButton.setColour (juce::TextButton::buttonOnColourId, t.accent);
    repaint();
}

void BassPanel::resized()
{
    auto b = getLocalBounds().reduced (28, 22);
    if (banner.isVisible())
        banner.setBounds (b.removeFromTop (34)), b.removeFromTop (10);

    auto col = b.removeFromLeft (320);
    auto row = col.removeFromTop (24);
    styleBox.setBounds (row.removeFromLeft (150));
    row.removeFromLeft (10);
    barsBox.setBounds (row);

    col.removeFromTop (10);
    row = col.removeFromTop (24);
    tempoBox.setBounds (row.removeFromLeft (100));
    row.removeFromLeft (8);
    bpmBox.setBounds (row.removeFromLeft (70));

    col.removeFromTop (10);
    row = col.removeFromTop (44);
    quantity.setBounds (row.removeFromLeft (150));
    row.removeFromLeft (12);
    sources.setBounds (row);

    col.removeFromTop (10);
    variationBox.setBounds (col.removeFromTop (24));
    col.removeFromTop (16);
    generateButton.setBounds (col.removeFromTop (40));
}

void BassPanel::paint (juce::Graphics& g)
{
    FoundryLookAndFeel::drawPanel (g, getLocalBounds().toFloat().reduced (6.0f), theme);

    auto copy = getLocalBounds().reduced (28, 22);
    copy.removeFromLeft (340);
    g.setFont (FoundryLookAndFeel::titleFont (18.0f, 0.12f));
    g.setColour (theme.text);
    g.drawText ("Bass Riff Generator", copy.removeFromTop (28), juce::Justification::centredLeft);

    g.setFont (FoundryLookAndFeel::labelFont (13.0f, false));
    g.setColour (theme.textDim);
    g.drawFittedText ("Sequences 2–4 of your bass one-shots into rhythmic phrases. "
                      "Riddim stays mostly straight; Dubstep adds syncopation and triplet bursts. "
                      "Favourite the ones that work, then send them to the Riff Splicer.",
                      copy.removeFromTop (90), juce::Justification::topLeft, 5);

    g.setFont (FoundryLookAndFeel::labelFont (8.5f, true, 0.14f));
    g.drawText ("VARIATION", 28, 168, 120, 12, juce::Justification::centredLeft);
}

} // namespace pt
