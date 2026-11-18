#pragma once

#include "PluginProcessor.h"
#include "GUI/FooterBar.h"
#include "GUI/GeneratorPanel.h"
#include "GUI/HeaderBar.h"
#include "GUI/LookAndFeel.h"
#include "GUI/ProgressOverlay.h"
#include "GUI/RiffMakerPanel.h"
#include "GUI/SettingsPanel.h"
#include "GUI/TabBar.h"
#include "GUI/TileBrowser.h"
#include "GUI/Widgets.h"

//==============================================================================
/** Layout, top to bottom: header with the RIFF MAKER / GENERATOR tabs, the
    active page, the sample browser dock, and the preview / volume / zoom bar. */
class FoundryAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    public juce::DragAndDropContainer,
                                    private juce::ChangeListener
{
public:
    explicit FoundryAudioProcessorEditor (FoundryAudioProcessor&);
    ~FoundryAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    static constexpr int kBaseWidth  = 1120;    // default logical size
    static constexpr int kBaseHeight = 860;
    static constexpr int kMinWidth   = 1000;    // smallest logical size
    static constexpr int kMinHeight  = 740;

    static constexpr int kHeaderH   = 52;
    static constexpr int kFooterH   = 64;
    static constexpr int kSplitterH = 8;
    static constexpr int kMinBrowserH = 170;
    static constexpr int kMinMainH    = 420;   // room for the generator pages

    enum Tab { riffMakerTab = 0, generatorTab = 1 };
    void showTab (int index);

    pt::TileBrowser&    getBrowser() noexcept   { return browser; }
    pt::RiffMakerPanel& getRiffPanel() noexcept { return riffPanel; }
    pt::GeneratorPanel& getGeneratorPanel() noexcept { return generatorPanel; }

    void applyThemeByName (const juce::String& name);
    void openPattern (const juce::File& file);

protected:
    /** Dragging a browser tile or the riff handle out of the window hands the
        DAW a real file - the riff is rendered on the spot. */
    bool shouldDropFilesWhenDraggedExternally (const juce::DragAndDropTarget::SourceDetails&,
                                               juce::StringArray& files, bool& canMoveFiles) override;

private:
    void applyTheme (const pt::Theme& t);
    void showSettings (bool show);
    float scale() const noexcept { return (float) zoomPercent / 100.0f; }
    void updateResizeLimits();

    class Splitter;
    void setZoomPercent (int percent, bool store);
    int  loadStoredZoom() const;
    void storeZoom (int percent);
    int  clampZoomToDisplay (int percent) const;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    FoundryAudioProcessor& processor;

    pt::FoundryLookAndFeel lookAndFeel;

    juce::Component content;
    pt::HeaderBar header;
    pt::TabBar tabs { { "RIFF MAKER", "GENERATOR" } };
    pt::TileBrowser browser;
    pt::RiffMakerPanel riffPanel;
    pt::GeneratorPanel generatorPanel;
    pt::SettingsPanel settingsPanel;
    pt::FooterBar footer;
    std::unique_ptr<Splitter> splitter;
    int browserHeight = 240;
    pt::ProgressOverlay overlay;
    pt::StatusBanner banner;

    juce::TooltipWindow tooltips { this, 700 };

    int zoomPercent = 100;
    int currentTab = riffMakerTab;
    bool settingsOpen = false;
    bool workerWasBusy = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FoundryAudioProcessorEditor)
};
