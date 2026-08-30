#include "HeaderBar.h"

namespace pt
{

HeaderBar::HeaderBar (juce::AudioProcessorValueTreeState& state, const juce::String& parameterID)
    : bypassToggle (state, parameterID, "Bypass"),
      pluginTitle ("PLUGIN NAME"),
      theme (getTheme())
{
    bypassToggle.setAccent (theme.accentAlt);
    addAndMakeVisible (bypassToggle);
}

void HeaderBar::setTitle (const juce::String& title, const juce::String& tagline)
{
    pluginTitle = title;
    pluginTagline = tagline;
    repaint();
}

void HeaderBar::setTheme (const Theme& t)
{
    theme = t;
    bypassToggle.setAccent (t.accentAlt);
    repaint();
}

void HeaderBar::resized()
{
    auto b = getLocalBounds();
    bypassToggle.setBounds (b.getWidth() - 90, (b.getHeight() - 24) / 2, 72, 24);
}

void HeaderBar::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    juce::ColourGradient bar (theme.bgTop.brighter (0.14f), 0.0f, 0.0f,
                              theme.bgTop.darker (0.32f), 0.0f, b.getBottom(), false);
    bar.addColour (0.62, theme.bgTop.darker (0.08f));
    g.setGradientFill (bar);
    g.fillRect (b);

    // The header's own shadow onto the content below it, so the bar reads as
    // a layer in front rather than as the same surface with a line drawn on
    // it.
    juce::ColourGradient cast (juce::Colours::black.withAlpha (0.26f), 0.0f, b.getBottom(),
                               juce::Colours::transparentBlack, 0.0f, b.getBottom() + 9.0f, false);
    g.setGradientFill (cast);
    g.fillRect (0.0f, b.getBottom() - 9.0f, b.getWidth(), 9.0f);

    juce::ColourGradient spill (juce::Colours::transparentBlack, 0.0f, b.getBottom() - 14.0f,
                                theme.accent.withAlpha (0.13f), 0.0f, b.getBottom(), false);
    g.setGradientFill (spill);
    g.fillRect (0.0f, b.getBottom() - 14.0f, b.getWidth(), 14.0f);

    g.setColour (theme.accent.withAlpha (0.5f));
    g.fillRect (0.0f, b.getBottom() - 1.0f, b.getWidth(), 1.0f);

    // Two overlapping circles: left ear / right ear. Stroke-only so the
    // vesica in the middle stays the background, which is what makes it
    // read as a mark rather than a blob.
    {
        const float cy = b.getCentreY();
        auto left  = juce::Rectangle<float> (14.0f, cy - 10.0f, 20.0f, 20.0f);
        auto right = left.translated (8.0f, 0.0f);

        g.setColour (theme.accent.withAlpha (0.95f));
        g.drawEllipse (left, 1.7f);
        g.setColour (theme.accentAlt.withAlpha (0.95f));
        g.drawEllipse (right, 1.7f);

        BinauralBeatsLookAndFeel::bloom (g, { left.getCentreX() + 4.0f, cy },
                                         16.0f, theme.accent, 0.12f);
    }

    // --- title ----------------------------------------------------------------
    {
        auto titleArea = juce::Rectangle<float> (b.getWidth() * 0.5f - 220.0f, 8.0f, 440.0f, 30.0f);

        g.setFont (BinauralBeatsLookAndFeel::titleFont (20.0f, 0.30f));

        g.setColour (juce::Colours::black.withAlpha (0.35f));
        BinauralBeatsLookAndFeel::drawCentredTracked (g, pluginTitle, titleArea.translated (0.0f, 1.4f));

        g.setColour (theme.text);
        BinauralBeatsLookAndFeel::drawCentredTracked (g, pluginTitle, titleArea);

        if (pluginTagline.isNotEmpty())
        {
            g.setColour (theme.textDim.withAlpha (0.88f));
            g.setFont (BinauralBeatsLookAndFeel::labelFont (9.5f, false, 0.14f));
            BinauralBeatsLookAndFeel::drawCentredTracked (g, pluginTagline,
                juce::Rectangle<float> (b.getWidth() * 0.5f - 220.0f, 36.0f, 440.0f, 14.0f));
        }
    }
}

} // namespace pt
