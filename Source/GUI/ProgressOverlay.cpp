#include "ProgressOverlay.h"

namespace pt
{

ProgressOverlay::ProgressOverlay (foundry::JobWorker& w)
    : worker (w)
{
    cancel.onClick = [this]
    {
        worker.requestCancel();
        if (onCancel)
            onCancel();
    };
    addAndMakeVisible (cancel);
    startTimerHz (20);
    setVisible (false);
}

void ProgressOverlay::setTheme (const Theme& t)
{
    theme = t;
    cancel.setColour (juce::TextButton::buttonColourId, t.accentAlt);
    repaint();
}

void ProgressOverlay::timerCallback()
{
    const bool busy = worker.isBusy();
    if (busy != isVisible())
        setVisible (busy);
    if (busy)
        repaint();
}

void ProgressOverlay::resized()
{
    auto b = getLocalBounds().withSizeKeepingCentre (420, 160);
    cancel.setBounds (b.removeFromBottom (36).reduced (140, 0));
}

void ProgressOverlay::paint (juce::Graphics& g)
{
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillAll();

    auto card = getLocalBounds().withSizeKeepingCentre (420, 160).toFloat();
    FoundryLookAndFeel::drawPanel (g, card, theme);

    auto inner = card.reduced (24.0f, 18.0f);
    g.setFont (FoundryLookAndFeel::titleFont (16.0f, 0.08f));
    g.setColour (theme.text);
    g.drawText (worker.title(), inner.removeFromTop (24.0f).toNearestInt(), juce::Justification::centredLeft);

    g.setFont (FoundryLookAndFeel::labelFont (13.0f, false));
    g.setColour (theme.textDim);
    g.drawText (worker.lastMessage(), inner.removeFromTop (22.0f).toNearestInt(), juce::Justification::centredLeft);

    inner.removeFromTop (8.0f);
    auto bar = inner.removeFromTop (14.0f);
    FoundryLookAndFeel::drawInsetWell (g, bar, theme, 4.0f);

    const float t = (float) worker.progressDone() / (float) worker.progressTotal();
    auto fill = bar.reduced (2.0f).withWidth (juce::jmax (4.0f, (bar.getWidth() - 4.0f) * juce::jlimit (0.0f, 1.0f, t)));
    FoundryLookAndFeel::drawActivePill (g, fill, 3.0f, theme.accent, 1.0f);
}

} // namespace pt
