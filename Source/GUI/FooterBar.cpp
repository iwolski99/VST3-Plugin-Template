#include "FooterBar.h"

namespace pt
{

const juce::Array<int>& FooterBar::zoomOptions()
{
    static const juce::Array<int> options { 50, 60, 70, 75, 80, 90, 100, 110, 125, 150, 175, 200 };
    return options;
}

FooterBar::FooterBar (FoundryAudioProcessor& p)
    : processor (p),
      preview (p.getPreview()),
      volumeKnob (p.getAPVTS(), pid::previewVolume, "VOL", 0.78f),
      theme (getTheme())
{
    addAndMakeVisible (playButton);
    addAndMakeVisible (stopButton);
    addAndMakeVisible (volumeKnob);
    addAndMakeVisible (zoomBox);

    playButton.setClickingTogglesState (false);
    stopButton.setClickingTogglesState (false);
    playButton.setTooltip ("Play the previewed sample again");
    stopButton.setTooltip ("Stop the preview");

    playButton.onClick = [this] { preview.retrigger(); };
    stopButton.onClick = [this] { preview.stop(); };

    for (int i = 0; i < zoomOptions().size(); ++i)
        zoomBox.addItem (juce::String (zoomOptions()[i]) + " %", i + 1);

    zoomBox.setTooltip ("Interface scale - Foundry remembers your choice");
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
    repaint (juce::Rectangle<int> (140, 0, juce::jmax (0, getWidth() - 320), getHeight()));
}

void FooterBar::resized()
{
    auto b = getLocalBounds().reduced (14, 6);
    playButton.setBounds (b.removeFromLeft (56).withHeight (26).withY (b.getY() + 16));
    b.removeFromLeft (5);
    stopButton.setBounds (b.removeFromLeft (56).withHeight (26).withY (b.getY() + 16));

    auto zoomArea = b.removeFromRight (88);
    zoomArea.removeFromTop (14);
    zoomBox.setBounds (zoomArea.removeFromTop (24));
    b.removeFromRight (8);
    volumeKnob.setBounds (b.removeFromRight (60));
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
    g.drawText ("SAMPLE PREVIEW", 16, 4, 120, 12, juce::Justification::centredLeft);
    g.drawText ("ZOOM", getWidth() - 92, 4, 80, 12, juce::Justification::centredLeft);

    const auto name = preview.currentName();
    auto wave = juce::Rectangle<float> (150.0f, 4.0f,
                                        juce::jmax (40.0f, (float) getWidth() - 150.0f - 180.0f), (float) getHeight() - 8.0f);

    g.setColour (theme.text.withAlpha (name.isEmpty() ? 0.35f : 0.92f));
    g.setFont (FoundryLookAndFeel::labelFont (12.0f, false, 0.02f));
    g.drawText (name.isEmpty() ? "Click any sample in the browser to hear it"
                               : name,
                wave.removeFromTop (14.0f).toNearestInt(),
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
