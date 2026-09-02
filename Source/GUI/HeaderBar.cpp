#include "HeaderBar.h"

namespace pt
{

HeaderBar::HeaderBar()
    : theme (getTheme())
{
    settingsButton.setClickingTogglesState (false);
    settingsButton.setTooltip ("Library folders, audio defaults, database");
    settingsButton.onClick = [this] { if (onSettings) onSettings(); };
    addAndMakeVisible (settingsButton);
}

void HeaderBar::setTheme (const Theme& t)
{
    theme = t;
    settingsButton.setColour (juce::TextButton::buttonColourId, t.accent);
    settingsButton.setColour (juce::TextButton::buttonOnColourId, t.accent);
    repaint();
}

void HeaderBar::resized()
{
    auto b = getLocalBounds();
    settingsButton.setBounds (b.getWidth() - 108, (b.getHeight() - 26) / 2, 90, 26);
}

void HeaderBar::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    juce::ColourGradient bar (theme.bgTop.brighter (0.12f), 0.0f, 0.0f,
                              theme.bgTop.darker (0.38f), 0.0f, b.getBottom(), false);
    bar.addColour (0.62, theme.bgTop.darker (0.10f));
    g.setGradientFill (bar);
    g.fillRect (b);

    juce::ColourGradient cast (juce::Colours::black.withAlpha (0.28f), 0.0f, b.getBottom(),
                               juce::Colours::transparentBlack, 0.0f, b.getBottom() + 10.0f, false);
    g.setGradientFill (cast);
    g.fillRect (0.0f, b.getBottom() - 10.0f, b.getWidth(), 10.0f);

    juce::ColourGradient spill (juce::Colours::transparentBlack, 0.0f, b.getBottom() - 16.0f,
                                theme.accent.withAlpha (0.16f), 0.0f, b.getBottom(), false);
    g.setGradientFill (spill);
    g.fillRect (0.0f, b.getBottom() - 16.0f, b.getWidth(), 16.0f);

    g.setColour (theme.accent.withAlpha (0.55f));
    g.fillRect (0.0f, b.getBottom() - 1.0f, b.getWidth(), 1.0f);

    // Ingot / anvil mark — a trapezoid block with a striking face.
    {
        auto mark = juce::Rectangle<float> (18.0f, b.getCentreY() - 13.0f, 28.0f, 26.0f);

        juce::Path ingot;
        ingot.startNewSubPath (mark.getX() + 4.0f, mark.getY() + 6.0f);
        ingot.lineTo (mark.getRight() - 4.0f, mark.getY() + 6.0f);
        ingot.lineTo (mark.getRight(), mark.getBottom() - 3.0f);
        ingot.lineTo (mark.getX(), mark.getBottom() - 3.0f);
        ingot.closeSubPath();

        g.setGradientFill (juce::ColourGradient (theme.accent, mark.getX(), mark.getBottom(),
                                                 theme.accentAlt, mark.getRight(), mark.getY(), false));
        g.fillPath (ingot);

        g.setColour (juce::Colours::white.withAlpha (0.28f));
        g.drawLine (mark.getX() + 6.0f, mark.getY() + 8.0f,
                    mark.getRight() - 6.0f, mark.getY() + 8.0f, 1.2f);

        FoundryLookAndFeel::bloom (g, mark.getCentre(), 20.0f, theme.accentAlt, 0.18f);
    }

    {
        auto titleArea = juce::Rectangle<float> (52.0f, 8.0f, 280.0f, 26.0f);
        g.setFont (FoundryLookAndFeel::titleFont (22.0f, 0.42f));

        g.setColour (juce::Colours::black.withAlpha (0.40f));
        g.drawText ("FOUNDRY", titleArea.translated (0.0f, 1.5f), juce::Justification::centredLeft);

        g.setColour (theme.text);
        g.drawText ("FOUNDRY", titleArea, juce::Justification::centredLeft);

        g.setColour (theme.textDim.withAlpha (0.92f));
        g.setFont (FoundryLookAndFeel::labelFont (9.0f, false, 0.18f));
        g.drawText ("YOUR SOUNDS IN. IDEAS OUT.",
                    juce::Rectangle<float> (52.0f, 34.0f, 320.0f, 14.0f),
                    juce::Justification::centredLeft);
    }
}

} // namespace pt
