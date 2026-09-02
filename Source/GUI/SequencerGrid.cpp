#include "SequencerGrid.h"

namespace pt
{

SequencerGrid::SequencerGrid()
{
    pattern = foundry::defaultDrumPattern (foundry::Genre::Riddim, 2);
}

void SequencerGrid::setTheme (const Theme& t)
{
    theme = t;
    repaint();
}

void SequencerGrid::setPattern (foundry::DrumPattern p)
{
    pattern = std::move (p);
    repaint();
}

void SequencerGrid::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (2.0f);
    FoundryLookAndFeel::drawPanel (g, bounds, theme, ui::panelCorner);

    auto inner = bounds.reduced (12.0f, 10.0f);
    const char* names[] = { "KICK", "CLAP", "HAT", "RIDE" };
    const int steps = pattern.numSteps();
    const int bars = juce::jmax (1, pattern.bars);
    const float labelW = 52.0f;
    auto grid = inner;
    grid.removeFromLeft (labelW);

    const float rowH = grid.getHeight() / 4.0f;
    const float stepW = grid.getWidth() / (float) juce::jmax (1, steps);

    g.setFont (FoundryLookAndFeel::labelFont (8.5f, true, 0.08f));
    g.setColour (theme.textDim);
    for (int s = 0; s < steps; ++s)
    {
        if (s % 4 == 0)
        {
            const int beat = (s / 4) % 4 + 1;
            auto cell = juce::Rectangle<float> (grid.getX() + stepW * (float) s, inner.getY() - 2.0f,
                                                stepW * 4.0f, 12.0f);
            g.drawText (juce::String (beat), cell.toNearestInt(), juce::Justification::centredLeft);
        }
    }

    for (int r = 0; r < 4; ++r)
    {
        auto row = juce::Rectangle<float> (inner.getX(), grid.getY() + rowH * (float) r, inner.getWidth(), rowH);
        g.setColour (theme.textDim);
        g.setFont (FoundryLookAndFeel::labelFont (9.5f, true, 0.12f));
        g.drawText (names[r], row.removeFromLeft (labelW).toNearestInt(), juce::Justification::centredLeft);

        for (int s = 0; s < steps; ++s)
        {
            auto cell = juce::Rectangle<float> (grid.getX() + stepW * (float) s,
                                                grid.getY() + rowH * (float) r,
                                                stepW, rowH).reduced (1.6f, 3.0f);
            const bool on = pattern.getStep (r, s);
            const bool barStart = (s % foundry::kStepsPerBar) == 0;
            const bool beat = (s % 4) == 0;

            if (on)
            {
                const auto acc = (r == 0 || r == 1) ? theme.accentAlt : theme.accent;
                FoundryLookAndFeel::drawActivePill (g, cell, 3.0f, acc, 0.95f);
            }
            else
            {
                FoundryLookAndFeel::drawInsetWell (g, cell, theme, 3.0f);
                if (beat)
                {
                    g.setColour (theme.panelEdge.withAlpha (barStart ? 0.55f : 0.28f));
                    g.drawRoundedRectangle (cell, 3.0f, 1.0f);
                }
            }
        }

        juce::ignoreUnused (bars);
    }
}

void SequencerGrid::hit (juce::Point<int> p, bool toggleOnce)
{
    auto bounds = getLocalBounds().toFloat().reduced (2.0f).reduced (12.0f, 10.0f);
    const float labelW = 52.0f;
    auto grid = bounds;
    grid.removeFromLeft (labelW);

    const int steps = pattern.numSteps();
    const float rowH = grid.getHeight() / 4.0f;
    const float stepW = grid.getWidth() / (float) juce::jmax (1, steps);

    const int r = (int) ((p.y - grid.getY()) / rowH);
    const int s = (int) ((p.x - grid.getX()) / stepW);

    if (r < 0 || r > 3 || s < 0 || s >= steps)
        return;

    if (toggleOnce && r == lastRow && s == lastStep)
        return;

    lastRow = r;
    lastStep = s;
    pattern.setStep (r, s, ! pattern.getStep (r, s));
    if (onChanged)
        onChanged();
    repaint();
}

void SequencerGrid::mouseDown (const juce::MouseEvent& e)
{
    lastRow = lastStep = -1;
    hit (e.getPosition(), false);
}

void SequencerGrid::mouseDrag (const juce::MouseEvent& e)
{
    hit (e.getPosition(), true);
}

} // namespace pt
