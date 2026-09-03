#pragma once

#include "LookAndFeel.h"
#include "../Core/Types.h"

namespace pt
{

class SequencerGrid : public juce::Component
{
public:
    std::function<void()> onChanged;

    SequencerGrid();

    void setTheme (const Theme& t);
    void setPattern (foundry::DrumPattern p);
    const foundry::DrumPattern& getPattern() const noexcept { return pattern; }
    foundry::DrumPattern& getPattern() noexcept { return pattern; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    void hit (juce::Point<int> p, bool toggleOnlyOnce);

    foundry::DrumPattern pattern;
    Theme theme { getTheme() };
    int lastRow = -1, lastStep = -1;
};

} // namespace pt
