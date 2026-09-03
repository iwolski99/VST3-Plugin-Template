#pragma once

#include "BrowserList.h"
#include "Widgets.h"
#include "../PluginProcessor.h"

namespace pt
{

class LibraryPanel : public juce::Component
{
public:
    std::function<void()> onOpenSettings;
    std::function<void (const foundry::LibraryItem&)> onSendToSplicer;
    std::function<void (const foundry::LibraryItem&)> onLockFrom;

    explicit LibraryPanel (FoundryAudioProcessor& p);

    void setTheme (const Theme& t);
    void refresh();
    void setSearch (const juce::String& s);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

    BrowserList list;

private:
    void rebuild();
    void paintNav (juce::Graphics&);
    int  navHit (juce::Point<int>) const;

    FoundryAudioProcessor& processor;
    juce::TextEditor search;
    juce::ComboBox sortBox;
    juce::TextButton refreshButton { "REFRESH" };
    StatusBanner banner;
    int navIndex = 3; // Riddim drum loops
    Theme theme { getTheme() };
};

} // namespace pt
