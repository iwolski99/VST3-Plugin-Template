#include "TabBar.h"

namespace pt
{

TabBar::TabBar (juce::StringArray tabLabels)
    : labels (std::move (tabLabels)), theme (getTheme())
{
}

void TabBar::setTheme (const Theme& t)
{
    theme = t;
    repaint();
}

void TabBar::setSelected (int index)
{
    selected = juce::jlimit (0, labels.size() - 1, index);
    repaint();
}

int TabBar::indexAt (float x) const noexcept
{
    const float w = (float) getWidth() / (float) juce::jmax (1, labels.size());
    return juce::jlimit (0, labels.size() - 1, (int) (x / w));
}

void TabBar::mouseMove (const juce::MouseEvent& e)
{
    const int h = indexAt ((float) e.x);
    if (h != hovered)
    {
        hovered = h;
        repaint();
    }
}

void TabBar::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}

void TabBar::mouseUp (const juce::MouseEvent& e)
{
    if (! getLocalBounds().contains (e.getPosition()))
        return;

    const int i = indexAt ((float) e.x);
    if (i != selected)
    {
        selected = i;
        if (onTabChanged)
            onTabChanged (selected);
        repaint();
    }
}

void TabBar::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    juce::ColourGradient bar (theme.bgTop.darker (0.08f), 0.0f, 0.0f,
                              theme.bgBottom.brighter (0.02f), 0.0f, b.getBottom(), false);
    g.setGradientFill (bar);
    g.fillRect (b);

    g.setColour (theme.panelEdge.withAlpha (0.35f));
    g.fillRect (0.0f, b.getBottom() - 1.0f, b.getWidth(), 1.0f);

    const float w = b.getWidth() / (float) juce::jmax (1, labels.size());

    for (int i = 0; i < labels.size(); ++i)
    {
        auto cell = juce::Rectangle<float> (w * (float) i, 4.0f, w, b.getHeight() - 6.0f).reduced (6.0f, 3.0f);
        const bool on = i == selected;

        if (on)
        {
            FoundryLookAndFeel::drawActivePill (g, cell, ui::controlCorner, theme.accent, 0.95f);
            g.setColour (FoundryLookAndFeel::pillTextColour (theme.accent));
        }
        else
        {
            if (i == hovered)
            {
                g.setColour (theme.accent.withAlpha (0.12f));
                g.fillRoundedRectangle (cell, ui::controlCorner);
            }
            g.setColour (theme.textDim.brighter (i == hovered ? 0.35f : 0.05f));
        }

        g.setFont (FoundryLookAndFeel::labelFont (11.0f, true, 0.16f));
        FoundryLookAndFeel::drawCentredTracked (g, labels[i], cell);
    }
}

} // namespace pt
