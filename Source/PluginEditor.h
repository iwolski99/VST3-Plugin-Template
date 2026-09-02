#pragma once

#include "PluginProcessor.h"
#include "GUI/BassPanel.h"
#include "GUI/DrumPanel.h"
#include "GUI/FooterBar.h"
#include "GUI/HeaderBar.h"
#include "GUI/LibraryPanel.h"
#include "GUI/LookAndFeel.h"
#include "GUI/ProgressOverlay.h"
#include "GUI/SettingsPanel.h"
#include "GUI/SplicerPanel.h"
#include "GUI/TabBar.h"
#include "GUI/Widgets.h"

//==============================================================================
class FoundryAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    public juce::DragAndDropContainer,
                                    private juce::ChangeListener
{
public:
    explicit FoundryAudioProcessorEditor (FoundryAudioProcessor&);
    ~FoundryAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int kBaseWidth  = 1120;
    static constexpr int kBaseHeight = 740;

private:
    void applyTheme (const pt::Theme& t);
    void showTab (int index);
    void setZoomPercent (int percent, bool store);
    int  loadStoredZoom() const;
    void storeZoom (int percent);
    int  clampZoomToDisplay (int percent) const;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    FoundryAudioProcessor& processor;

    pt::FoundryLookAndFeel lookAndFeel;

    juce::Component content;
    pt::HeaderBar header;
    pt::TabBar tabs;
    pt::LibraryPanel libraryPanel;
    pt::DrumPanel drumPanel;
    pt::BassPanel bassPanel;
    pt::SplicerPanel splicerPanel;
    pt::SettingsPanel settingsPanel;
    pt::FooterBar footer;
    pt::ProgressOverlay overlay;
    pt::StatusBanner banner;

    juce::TooltipWindow tooltips { this, 700 };

    int zoomPercent = 100;
    int currentTab = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FoundryAudioProcessorEditor)
};
