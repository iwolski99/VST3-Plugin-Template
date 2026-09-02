#include "FooterBar.h"

namespace pt
{

const juce::Array<int>& FooterBar::zoomOptions()
{
    static const juce::Array<int> options { 50, 60, 70, 75, 80, 90, 100, 110, 125, 150, 175, 200 };
    return options;
}

FooterBar::FooterBar (juce::AudioProcessorValueTreeState& state, foundry::PreviewEngine& p)
    : preview (p),
      volumeKnob (state, pid::previewVolume, "VOL", 0.78f),
      theme (getTheme())
{
    addAndMakeVisible (playButton);
    addAndMakeVisible (stopButton);
    addAndMakeVisible (volumeKnob);
    addAndMakeVisible (zoomBox);

    playButton.setClickingTogglesState (false);
    stopButton.setClickingTogglesState (false);
    playButton.setTooltip ("Play / retrigger the selected sample");
    stopButton.setTooltip ("Stop preview");

    playButton.onClick = [this]
    {
        if (preview.currentFile().existsAsFile())
            preview.retrigger();
    };
    stopButton.onClick = [this] { preview.stop(); };

    for (int i = 0; i < zoomOptions().size(); ++i)
        zoomBox.addItem (juce::String (zoomOptions()[i]) + " %", i + 1);

    zoomBox.setTooltip ("Interface scale — Foundry remembers your choice");
    zoomBox.onChange = [this]
    {
        const int idx = zoomBox.getSelectedId() - 1;
        if (idx >= 0 && idx < zoomOptions().size() && onZoomChanged)
            onZoomChanged (zoomOptions()[idx]);
    };

    startTimerHz (24);
}

FooterBar::~FooterBar()
{
    stopTimer();
}

void FooterBar::setTheme (const Theme& t)
{
    theme = t;
    volumeKnob.setThemeColours (t);
    playButton.setColour (juce::TextButton::buttonColourId, t.accent);
    playButton.setColour (juce::TextButton::buttonOnColourId, t.accent);
    stopButton.setColour (juce::TextButton::buttonColourId, t.accentAlt);
    zoomBox.setColour (juce::ComboBox::textColourId, t.text);
    zoomBox.setColour (juce::ComboBox::arrowColourId, t.accent);
    zoomBox.setColour (juce::ComboBox::outlineColourId, t.panelEdge);
    repaint();
}

void FooterBar::setZoomPercent (int percent)
{
    const int idx = zoomOptions().indexOf (percent);
    zoomBox.setSelectedId (idx >= 0 ? idx + 1 : zoomOptions().indexOf (100) + 1,
                           juce::dontSendNotification);
}

void FooterBar::refresh()
{
    repaint();
}

void FooterBar::timerCallback()
{
    preview.copyPeaks (peaks, 96);
    playButton.setToggleState (preview.isPlaying(), juce::dontSendNotification);
    repaint (200, 0, getWidth() - 340, getHeight());
}

void FooterBar::resized()
{
    auto b = getLocalBounds().reduced (14, 8);

    playButton.setBounds (b.removeFromLeft (64).withHeight (28).withY (b.getY() + 22));
    b.removeFromLeft (6);
    stopButton.setBounds (b.removeFromLeft (64).withHeight (28).withY (b.getY() + 22));

    auto zoomArea = b.removeFromRight (88);
    zoomArea.removeFromTop (18);
    zoomBox.setBounds (zoomArea.removeFromTop (24));

    b.removeFromRight (8);
    volumeKnob.setBounds (b.removeFromRight (72));
}

void FooterBar::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    juce::ColourGradient bar (theme.bgTop.darker (0.22f), 0.0f, 0.0f,
                              theme.bgBottom.darker (0.20f), 0.0f, b.getBottom(), false);
    bar.addColour (0.35, theme.bgTop.darker (0.34f));
    g.setGradientFill (bar);
    g.fillRect (b);

    juce::ColourGradient cast (juce::Colours::black.withAlpha (0.24f), 0.0f, 0.0f,
                               juce::Colours::transparentBlack, 0.0f, 9.0f, false);
    g.setGradientFill (cast);
    g.fillRect (0.0f, 0.0f, b.getWidth(), 9.0f);

    juce::ColourGradient spill (theme.accent.withAlpha (0.14f), 0.0f, 0.0f,
                                juce::Colours::transparentBlack, 0.0f, 13.0f, false);
    g.setGradientFill (spill);
    g.fillRect (0.0f, 0.0f, b.getWidth(), 13.0f);

    g.setColour (theme.accent.withAlpha (0.50f));
    g.fillRect (0.0f, 0.0f, b.getWidth(), 1.0f);

    g.setFont (FoundryLookAndFeel::labelFont (8.5f, true, 0.14f));
    g.setColour (theme.textDim.withAlpha (0.75f));
    g.drawText ("PREVIEW", 16, 8, 90, 12, juce::Justification::centredLeft);
    g.drawText ("ZOOM", getWidth() - 92, 8, 80, 12, juce::Justification::centredLeft);

    const auto name = preview.currentName();
    auto wave = juce::Rectangle<float> (160.0f, 18.0f, (float) getWidth() - 160.0f - 180.0f, 52.0f);

    g.setColour (theme.text.withAlpha (name.isEmpty() ? 0.35f : 0.92f));
    g.setFont (FoundryLookAndFeel::labelFont (12.0f, false, 0.02f));
    g.drawText (name.isEmpty() ? "Nothing loaded — click a file to audition"
                               : name,
                wave.removeFromTop (16.0f).toNearestInt(),
                juce::Justification::centredLeft, true);

    auto strip = wave.reduced (0.0f, 2.0f);
    FoundryLookAndFeel::drawInsetWell (g, strip, theme, 4.0f);

    if (! peaks.empty())
    {
        const float binW = strip.getWidth() / (float) peaks.size();
        const float mid = strip.getCentreY();

        for (size_t i = 0; i < peaks.size(); ++i)
        {
            const float mag = juce::jlimit (0.0f, 1.0f, peaks[i]);
            const float h = juce::jmax (1.0f, mag * (strip.getHeight() * 0.42f));
            auto col = theme.accent.interpolatedWith (theme.accentAlt, mag);
            g.setColour (col.withAlpha (preview.isPlaying() ? 0.90f : 0.45f));
            g.fillRect (strip.getX() + binW * (float) i + 0.4f, mid - h, juce::jmax (1.0f, binW - 0.8f), h * 2.0f);
        }
    }
}

} // namespace pt
