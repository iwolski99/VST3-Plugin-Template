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
      browser (p),
      riffPanel (p),
      generatorPanel (p),
      settingsPanel (p),
      footer (p),
      overlay (p.getWorker())
{
    setLookAndFeel (&lookAndFeel);
    setWantsKeyboardFocus (true);

    addAndMakeVisible (content);
    content.addAndMakeVisible (header);
    content.addAndMakeVisible (tabs);
    content.addAndMakeVisible (riffPanel);
    content.addChildComponent (generatorPanel);
    content.addChildComponent (settingsPanel);
    content.addAndMakeVisible (browser);
    content.addAndMakeVisible (footer);
    content.addChildComponent (banner);
    content.addChildComponent (overlay);

    auto openSettings = [this] { showSettings (true); };
    header.onSettings = openSettings;
    generatorPanel.drumPanel.onOpenSettings = openSettings;
    generatorPanel.bassPanel.onOpenSettings = openSettings;
    generatorPanel.splicerPanel.onOpenSettings = openSettings;
    banner.onOpenSettings = openSettings;
    browser.onOpenSettings = openSettings;

    // Browser -> Riff Maker
    browser.onLoadIntoSlot = [this] (int slot, const juce::File& f)
    {
        showTab (riffMakerTab);
        riffPanel.loadIntoSlot (slot, f);
    };
    browser.onOpenRiff = [this] (const juce::File& f)
    {
        showTab (riffMakerTab);
        juce::String err;
        if (processor.openRiff (f, err))
            riffPanel.showMessage ("Opened " + f.getFileNameWithoutExtension()
                                   + " for editing. RENDER saves your changes as a new riff.", false);
        else
            riffPanel.showMessage (err, true);
    };
    browser.onMessage = [this] (const juce::String& m, bool isError)
    {
        banner.setMessage (m, isError, isError && m.containsIgnoreCase ("Settings"));
        resized();
    };

    // Browser -> Generator tools
    browser.onSendToSplicer = [this] (const foundry::LibraryItem& it)
    {
        showTab (generatorTab);
        generatorPanel.showPage (pt::GeneratorPanel::splicer);
        generatorPanel.splicerPanel.addSource (it);
    };
    browser.onLockInDrums = [this] (const foundry::LibraryItem& it)
    {
        showTab (generatorTab);
        generatorPanel.showPage (pt::GeneratorPanel::drums);
        generatorPanel.drumPanel.applyLock (it);
    };

    riffPanel.getDicePool = [this] { return browser.dicePool(); };
    riffPanel.onRendered = [this] (const juce::File&) { browser.refresh(); };

    settingsPanel.onClose = [this] { showSettings (false); };
    settingsPanel.onLibraryChanged = [this]
    {
        browser.refresh();
        generatorPanel.refresh();
    };

    tabs.onTabChanged = [this] (int i) { showTab (i); };
    footer.onZoomChanged = [this] (int percent) { setZoomPercent (percent, true); };
    overlay.onCancel = [this] { processor.getWorker().requestCancel(); };

    processor.getWorker().addChangeListener (this);
    processor.addChangeListener (this);

    applyTheme (pt::getTheme());
    showTab (juce::jlimit (0, 1, p.getConfig().get().lastTab));

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
    browser.setTheme (t);
    riffPanel.setTheme (t);
    generatorPanel.setTheme (t);
    settingsPanel.setTheme (t);
    footer.setTheme (t);
    overlay.setTheme (t);
    banner.setTheme (t);
}

void FoundryAudioProcessorEditor::showTab (int index)
{
    currentTab = juce::jlimit (0, 1, index);
    processor.getConfig().get().lastTab = currentTab;
    tabs.setSelected (currentTab);

    if (settingsOpen)
        showSettings (false);

    riffPanel.setVisible (currentTab == riffMakerTab);
    generatorPanel.setVisible (currentTab == generatorTab);
    if (currentTab == generatorTab)
        generatorPanel.showPage (generatorPanel.getPage());
}

void FoundryAudioProcessorEditor::showSettings (bool show)
{
    settingsOpen = show;
    settingsPanel.setVisible (show);
    browser.setVisible (! show);
    riffPanel.setVisible (! show && currentTab == riffMakerTab);
    generatorPanel.setVisible (! show && currentTab == generatorTab);

    if (! show)
    {
        browser.refresh();
        generatorPanel.refresh();
    }
    resized();
}

bool FoundryAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    if (mods.isCommandDown() && key.getKeyCode() == 'Z')
        return mods.isShiftDown() ? processor.redo() : processor.undo();
    if (mods.isCommandDown() && key.getKeyCode() == 'Y')
        return processor.redo();
    return false;
}

bool FoundryAudioProcessorEditor::shouldDropFilesWhenDraggedExternally (const juce::DragAndDropTarget::SourceDetails& details,
                                                                         juce::StringArray& files, bool& canMoveFiles)
{
    canMoveFiles = false;
    const auto desc = details.description.toString();

    if (desc.startsWith (pt::dnd::samplePrefix))
    {
        const juce::File f (desc.fromFirstOccurrenceOf (pt::dnd::samplePrefix, false, false));
        if (! f.existsAsFile())
            return false;
        files.add (f.getFullPathName());
        return true;
    }

    if (desc == pt::dnd::riff)
    {
        juce::String err;
        bool reused = false;
        const auto f = processor.renderRiff (err, reused);
        if (! f.existsAsFile())
        {
            riffPanel.showMessage (err, true);
            return false;
        }

        files.add (f.getFullPathName());
        riffPanel.showMessage ("Dropped " + f.getFileName() + " - it's also saved in My Riffs.", false);
        browser.refresh();
        return true;
    }

    return false;
}

void FoundryAudioProcessorEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    browser.refresh();
    generatorPanel.refresh();
    footer.refresh();

    auto& w = processor.getWorker();
    const bool busy = w.isBusy();

    if (! busy)
    {
        const auto msg = w.lastMessage();
        const bool err = ! w.lastSucceeded();

        // A batch just finished: open its output folder so the new loops are
        // right there as tiles, ready to preview or drop into a slot.
        if (workerWasBusy && ! err && w.lastJobGenerated())
        {
            auto& cfg = processor.getConfig();
            switch (w.lastKind())
            {
                case foundry::JobWorker::Kind::Drums:  browser.showFolder (cfg.generatedDrumDir (processor.getUiGenre())); break;
                case foundry::JobWorker::Kind::Bass:   browser.showFolder (cfg.generatedBassDir (false)); break;
                case foundry::JobWorker::Kind::Splice: browser.showFolder (cfg.generatedBassDir (true)); break;
                case foundry::JobWorker::Kind::Idle:
                case foundry::JobWorker::Kind::Scan:
                    break;
            }
        }

        if (workerWasBusy || err)
            banner.setMessage (msg, err, err && (msg.containsIgnoreCase ("folder") || msg.containsIgnoreCase ("sample")
                                                 || msg.containsIgnoreCase ("Settings")));
        resized();
    }

    workerWasBusy = busy;
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
    processor.persistConfig (err, true);

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

    auto head = b.removeFromTop (kHeaderH);
    header.setBounds (head);
    tabs.setBounds (head.withSizeKeepingCentre (340, 34).translated (0, -1));

    footer.setBounds (b.removeFromBottom (kFooterH));

    if (settingsOpen)
    {
        settingsPanel.setBounds (b);
    }
    else
    {
        browser.setBounds (b.removeFromBottom (kBrowserH).reduced (6, 2));
        if (banner.isVisible())
            banner.setBounds (b.removeFromTop (32).reduced (10, 4));
        riffPanel.setBounds (b);
        generatorPanel.setBounds (b);
    }

    overlay.setBounds (content.getLocalBounds());
}

void FoundryAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto& t = lookAndFeel.getCurrentTheme();
    juce::ColourGradient bg (t.bgTop, 0.0f, 0.0f, t.bgBottom, 0.0f, (float) getHeight(), false);
    g.setGradientFill (bg);
    g.fillAll();
}
