#include "GeneratorPanel.h"

namespace pt
{

GeneratorPanel::GeneratorPanel (FoundryAudioProcessor& p)
    : drumPanel (p), bassPanel (p), splicerPanel (p)
{
    addAndMakeVisible (tabs);
    addChildComponent (drumPanel);
    addChildComponent (bassPanel);
    addChildComponent (splicerPanel);

    tabs.onTabChanged = [this] (int i) { showPage (i); };
    showPage (drums);
}

void GeneratorPanel::setTheme (const Theme& t)
{
    theme = t;
    tabs.setTheme (t);
    drumPanel.setTheme (t);
    bassPanel.setTheme (t);
    splicerPanel.setTheme (t);
    repaint();
}

void GeneratorPanel::showPage (int newPage)
{
    page = juce::jlimit (0, 2, newPage);
    tabs.setSelected (page);
    drumPanel.setVisible (page == drums);
    bassPanel.setVisible (page == bass);
    splicerPanel.setVisible (page == splicer);

    if (page == drums)
        drumPanel.syncFromProcessor();
    if (page == bass)
        bassPanel.syncFromProcessor();
    if (page == splicer)
        splicerPanel.refresh();
}

void GeneratorPanel::refresh()
{
    splicerPanel.refresh();
}

void GeneratorPanel::resized()
{
    auto b = getLocalBounds();
    tabs.setBounds (b.removeFromTop (32).reduced (180, 0));
    drumPanel.setBounds (b);
    bassPanel.setBounds (b);
    splicerPanel.setBounds (b);
}

void GeneratorPanel::paint (juce::Graphics&)
{
}

} // namespace pt
