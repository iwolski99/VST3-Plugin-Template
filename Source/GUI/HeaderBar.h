#pragma once

#include "LookAndFeel.h"

namespace pt
{

class HeaderBar : public juce::Component
{
public:
    std::function<void()> onSettings;

    HeaderBar();

    void setTheme (const Theme& t);
    void paint (juce::Graphics&) override;
    void resized() override;

    juce::TextButton settingsButton { "SETTINGS" };

private:
    Theme theme;
};

} // namespace pt
