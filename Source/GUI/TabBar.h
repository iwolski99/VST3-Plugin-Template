#pragma once

#include "LookAndFeel.h"

namespace pt
{

class TabBar : public juce::Component
{
public:
    std::function<void (int)> onTabChanged;

    explicit TabBar (juce::StringArray tabLabels);

    void setTheme (const Theme& t);
    void setSelected (int index);
    int  getSelected() const noexcept { return selected; }

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    int indexAt (float x) const noexcept;

    juce::StringArray labels;
    int selected = 0;
    int hovered = -1;
    Theme theme;
};

} // namespace pt
