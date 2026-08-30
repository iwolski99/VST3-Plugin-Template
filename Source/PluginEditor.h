#pragma once

#include "PluginProcessor.h"
#include "GUI/FooterBar.h"
#include "GUI/HeaderBar.h"
#include "GUI/LookAndFeel.h"
#include "GUI/MainPanel.h"
#include "GUI/PresetBrowser.h"
#include "GUI/SavePresetPanel.h"

//==============================================================================
class BinauralBeatsAudioProcessorEditor : public juce::AudioProcessorEditor,
                                          public juce::DragAndDropContainer
{
public:
    explicit BinauralBeatsAudioProcessorEditor (BinauralBeatsAudioProcessor&);
    ~BinauralBeatsAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Design size at 100 %. Deliberately compact so 100 % already fits a
        1366 x 768 laptop panel with room for the host's window frame - the
        zoom list covers everything from a netbook to a 4K desktop. */
    static constexpr int kBaseWidth  = 720;
    static constexpr int kBaseHeight = 560;

private:
    void applyTheme (const pt::Theme& t);

    void setZoomPercent (int percent, bool store);
    int  loadStoredZoom() const;
    void storeZoom (int percent) const;
    int  clampZoomToDisplay (int percent) const;

    BinauralBeatsAudioProcessor& plugin;

    pt::BinauralBeatsLookAndFeel lookAndFeel;

    juce::Component content;
    pt::MainPanel mainPanel;

    pt::HeaderBar header;
    pt::FooterBar footer;
    pt::SavePresetPanel savePanel;
    pt::PresetBrowser   presetBrowser;

    juce::TooltipWindow tooltips { this, 750 };

    int zoomPercent = 100;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BinauralBeatsAudioProcessorEditor)
};
