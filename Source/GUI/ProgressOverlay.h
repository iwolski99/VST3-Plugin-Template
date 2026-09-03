#pragma once

#include "LookAndFeel.h"
#include "../Core/JobWorker.h"

namespace pt
{

class ProgressOverlay : public juce::Component,
                        private juce::Timer
{
public:
    std::function<void()> onCancel;

    explicit ProgressOverlay (foundry::JobWorker& w);

    void setTheme (const Theme& t);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    foundry::JobWorker& worker;
    juce::TextButton cancel { "CANCEL" };
    Theme theme { getTheme() };
};

} // namespace pt
