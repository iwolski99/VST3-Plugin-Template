#include "PluginEditor.h"

namespace
{
    juce::PropertiesFile::Options zoomStoreOptions()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "Foundry";
        o.filenameSuffix = "settings";
        o.folderName = "Foundry";
        o.osxLibrarySubFolder = "Application Support";
        return o;
    }
}

FoundryAudioProcessorEditor::FoundryAudioProcessorEditor (FoundryAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      libraryPanel (p),
      drumPanel (p),
      bassPanel (p),
      splicerPanel (p),
      settingsPanel (p),
      footer (p),
      overlay (p.getWorker())
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (content);
    content.addAndMakeVisible (header);
    content.addAndMakeVisible (tabs);
    content.addAndMakeVisible (libraryPanel);
    content.addChildComponent (drumPanel);
    content.addChildComponent (bassPanel);
    content.addChildComponent (splicerPanel);
    content.addChildComponent (settingsPanel);
    content.addAndMakeVisible (footer);
    content.addChildComponent (banner);
    content.addChildComponent (overlay);

    header.onSettings = [this]
    {
        settingsPanel.setVisible (true);
        libraryPanel.setVisible (false);
        drumPanel.setVisible (false);
        bassPanel.setVisible (false);
        splicerPanel.setVisible (false);
    };

    auto openSettings = [this] { header.onSettings(); };
    libraryPanel.onOpenSettings = openSettings;
    drumPanel.onOpenSettings = openSettings;
    bassPanel.onOpenSettings = openSettings;
    splicerPanel.onOpenSettings = openSettings;
    banner.onOpenSettings = openSettings;

    libraryPanel.onSendToSplicer = [this] (const foundry::LibraryItem& it)
    {
        splicerPanel.addSource (it);
        tabs.setSelected (3);
        showTab (3);
    };
    libraryPanel.onLockFrom = [this] (const foundry::LibraryItem& it)
    {
        drumPanel.applyLock (it);
        tabs.setSelected (1);
        showTab (1);
    };

    settingsPanel.onClose = [this]
    {
        settingsPanel.setVisible (false);
        showTab (tabs.getSelected());
        libraryPanel.refresh();
    };
    settingsPanel.onLibraryChanged = [this]
    {
        libraryPanel.refresh();
        splicerPanel.refresh();
    };

    tabs.onTabChanged = [this] (int i) { showTab (i); };
    tabs.setSelected (juce::jlimit (0, 3, p.getConfig().get().lastTab));

    footer.onZoomChanged = [this] (int percent) { setZoomPercent (percent, true); };

    overlay.onCancel = [this] { processor.getWorker().requestCancel(); };

    processor.getWorker().addChangeListener (this);
    processor.addChangeListener (this);

    applyTheme (pt::getTheme());
    showTab (tabs.getSelected());

    setResizable (false, false);
    setZoomPercent (clampZoomToDisplay (loadStoredZoom()), false);
}

FoundryAudioProcessorEditor::~FoundryAudioProcessorEditor()
{
    processor.getWorker().removeChangeListener (this);
    processor.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void FoundryAudioProcessorEditor::applyTheme (const pt::Theme& t)
{
    lookAndFeel.setTheme (t);
    header.setTheme (t);
    tabs.setTheme (t);
    libraryPanel.setTheme (t);
    drumPanel.setTheme (t);
    bassPanel.setTheme (t);
    splicerPanel.setTheme (t);
    settingsPanel.setTheme (t);
    footer.setTheme (t);
    overlay.setTheme (t);
    banner.setTheme (t);
}

void FoundryAudioProcessorEditor::showTab (int index)
{
    currentTab = juce::jlimit (0, 3, index);
    processor.getConfig().get().lastTab = currentTab;
    settingsPanel.setVisible (false);
    libraryPanel.setVisible (currentTab == 0);
    drumPanel.setVisible (currentTab == 1);
    bassPanel.setVisible (currentTab == 2);
    splicerPanel.setVisible (currentTab == 3);

    if (currentTab == 0)
        libraryPanel.refresh();
    if (currentTab == 1)
        drumPanel.syncFromProcessor();
    if (currentTab == 3)
        splicerPanel.refresh();
}

void FoundryAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    libraryPanel.refresh();
    splicerPanel.refresh();
    footer.refresh();

    auto& w = processor.getWorker();
    if (! w.isBusy())
    {
        const auto msg = w.lastMessage();
        const bool err = ! w.lastSucceeded();

        // A finished batch arms its newest loop, so PLAY has something to play
        // the moment the progress bar disappears. A library refresh leaves the
        // loaded loop alone, and neither ever interrupts something sounding.
        if (! err && w.lastJobGenerated() && ! processor.getPreview().isPlaying())
        {
            juce::String loadErr;
            processor.loadNewestGenerated (loadErr);
        }

        banner.setMessage (msg, err, err && (msg.containsIgnoreCase ("folder") || msg.containsIgnoreCase ("sample")
                                             || msg.containsIgnoreCase ("Settings")));
        resized();
    }
}

int FoundryAudioProcessorEditor::loadStoredZoom() const
{
    if (processor.getConfig().get().uiZoom >= 50)
        return processor.getConfig().get().uiZoom;

    juce::ApplicationProperties props;
    props.setStorageParameters (zoomStoreOptions());
    if (auto* file = props.getUserSettings())
        return file->getIntValue (pt::stateID::zoom, 100);
    return 100;
}

void FoundryAudioProcessorEditor::storeZoom (int percent)
{
    processor.getConfig().get().uiZoom = percent;
    juce::String err;
    processor.persistConfig (err);

    juce::ApplicationProperties props;
    props.setStorageParameters (zoomStoreOptions());
    if (auto* file = props.getUserSettings())
    {
        file->setValue (pt::stateID::zoom, percent);
        file->saveIfNeeded();
    }
}

int FoundryAudioProcessorEditor::clampZoomToDisplay (int percent) const
{
    const auto& options = pt::FooterBar::zoomOptions();
    int idx = options.indexOf (percent);
    if (idx < 0)
        idx = options.indexOf (100);

    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto area = display->userBounds;

        while (idx > 0)
        {
            const float scale = (float) options[idx] / 100.0f;
            const int w = juce::roundToInt (kBaseWidth * scale);
            const int h = juce::roundToInt (kBaseHeight * scale);

            if (w <= area.getWidth() - 40 && h <= area.getHeight() - 80)
                break;

            --idx;
        }
    }

    return options[juce::jmax (0, idx)];
}

void FoundryAudioProcessorEditor::setZoomPercent (int percent, bool store)
{
    zoomPercent = percent;
    const float scale = (float) zoomPercent / 100.0f;

    content.setBounds (0, 0, kBaseWidth, kBaseHeight);
    content.setTransform (juce::AffineTransform::scale (scale));
    setSize (juce::roundToInt (kBaseWidth * scale), juce::roundToInt (kBaseHeight * scale));
    footer.setZoomPercent (zoomPercent);

    if (store)
        storeZoom (zoomPercent);
}

void FoundryAudioProcessorEditor::resized()
{
    auto b = content.getLocalBounds();
    header.setBounds (b.removeFromTop (56));
    tabs.setBounds (b.removeFromTop (38));
    footer.setBounds (b.removeFromBottom (84));

    if (banner.isVisible())
        banner.setBounds (b.removeFromTop (32).reduced (10, 4));

    libraryPanel.setBounds (b);
    drumPanel.setBounds (b);
    bassPanel.setBounds (b);
    splicerPanel.setBounds (b);
    settingsPanel.setBounds (b);
    overlay.setBounds (content.getLocalBounds());
}

void FoundryAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto& t = lookAndFeel.getCurrentTheme();
    juce::ColourGradient bg (t.bgTop, 0.0f, 0.0f, t.bgBottom, 0.0f, (float) getHeight(), false);
    g.setGradientFill (bg);
    g.fillAll();
}
