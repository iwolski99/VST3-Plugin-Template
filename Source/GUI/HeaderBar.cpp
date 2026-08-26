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

    // --- wordmark -------------------------------------------------------------
    // A simple triangular glyph as a placeholder mark - swap this path for
    // your own logo. It exists mainly to prove the pattern: a small shape
    // filled with an accent gradient plus a touch of bloom reads as a mark
    // rather than a decoration, at almost no drawing cost.
    {
        auto mark = juce::Rectangle<float> (16.0f, b.getCentreY() - 11.0f, 22.0f, 22.0f);

        juce::Path glyph;
        glyph.startNewSubPath (mark.getCentreX(), mark.getY());
        glyph.lineTo (mark.getRight(), mark.getBottom());
        glyph.lineTo (mark.getX(), mark.getBottom());
        glyph.closeSubPath();

        g.setGradientFill (juce::ColourGradient (theme.accent, mark.getX(), mark.getBottom(),
                                                 theme.accentAlt, mark.getRight(), mark.getY(), false));
        g.fillPath (glyph);

        PluginTemplateLookAndFeel::bloom (g, mark.getCentre(), 18.0f, theme.accent, 0.14f);
    }

    // --- title ----------------------------------------------------------------
    {
        auto titleArea = juce::Rectangle<float> (b.getWidth() * 0.5f - 220.0f, 8.0f, 440.0f, 30.0f);

        g.setFont (PluginTemplateLookAndFeel::titleFont (20.0f, 0.30f));

        g.setColour (juce::Colours::black.withAlpha (0.35f));
        PluginTemplateLookAndFeel::drawCentredTracked (g, pluginTitle, titleArea.translated (0.0f, 1.4f));

        g.setColour (theme.text);
        PluginTemplateLookAndFeel::drawCentredTracked (g, pluginTitle, titleArea);

        if (pluginTagline.isNotEmpty())
        {
            g.setColour (theme.textDim.withAlpha (0.88f));
            g.setFont (PluginTemplateLookAndFeel::labelFont (9.5f, false, 0.14f));
            PluginTemplateLookAndFeel::drawCentredTracked (g, pluginTagline,
                juce::Rectangle<float> (b.getWidth() * 0.5f - 220.0f, 36.0f, 440.0f, 14.0f));
        }
    }
}

} // namespace pt
