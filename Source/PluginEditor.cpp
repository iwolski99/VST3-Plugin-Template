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

/** The bar between the page and the sample dock: drag it to give the browser
    more or less room, like resizing a dock in a game engine. */
class FoundryAudioProcessorEditor::Splitter : public juce::Component,
                                              public juce::SettableTooltipClient
{
public:
    explicit Splitter (FoundryAudioProcessorEditor& o) : owner (o)
    {
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        setRepaintsOnMouseActivity (true);
        setTooltip ("Drag to resize the sample browser");
    }

    void paint (juce::Graphics& g) override
    {
        const auto& t = pt::getTheme();
        auto r = getLocalBounds().toFloat().withSizeKeepingCentre (60.0f, 3.0f);
        g.setColour ((isMouseOverOrDragging() ? t.accent : t.panelEdge).withAlpha (0.9f));
        g.fillRoundedRectangle (r, 1.5f);
    }

    void mouseDown (const juce::MouseEvent&) override { startHeight = owner.browserHeight; }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        owner.browserHeight = startHeight - e.getDistanceFromDragStartY();
        owner.resized();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        owner.processor.getConfig().get().browserHeight = owner.browserHeight;
    }

private:
    FoundryAudioProcessorEditor& owner;
    int startHeight = 0;
};

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
    pt::setCurrentTheme (p.getConfig().get().themeName);
    setLookAndFeel (&lookAndFeel);
    setWantsKeyboardFocus (true);

    splitter = std::make_unique<Splitter> (*this);
    browserHeight = p.getConfig().get().browserHeight;

    addAndMakeVisible (content);
    content.addAndMakeVisible (header);
    content.addAndMakeVisible (tabs);
    content.addAndMakeVisible (riffPanel);
    content.addChildComponent (generatorPanel);
    content.addChildComponent (settingsPanel);
    content.addAndMakeVisible (browser);
    content.addAndMakeVisible (*splitter);
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
    browser.onOpenPattern = [this] (const juce::File& f) { openPattern (f); };
    browser.onLockBassSource = [this] (int row, const foundry::LibraryItem& it)
    {
        showTab (generatorTab);
        generatorPanel.showPage (pt::GeneratorPanel::bass);
        generatorPanel.bassPanel.applyLock (row, it);
    };
    browser.onFilesChanged = [this] { riffPanel.syncFromProcessor(); };
    generatorPanel.drumPanel.onPatternsChanged = [this] { browser.refresh(); };
    generatorPanel.bassPanel.onPatternsChanged = [this] { browser.refresh(); };
    settingsPanel.onThemeChanged = [this] (const juce::String& name) { applyThemeByName (name); };

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

    // Resizable: the window can be any size above a minimum; ZOOM scales the
    // whole interface on top of that (handy on high-resolution screens).
    setResizable (true, true);
    zoomPercent = clampZoomToDisplay (loadStoredZoom());
    footer.setZoomPercent (zoomPercent);
    updateResizeLimits();

    const auto& cfg = p.getConfig().get();
    const int logicalW = cfg.uiWidth  >= kMinWidth  ? cfg.uiWidth  : kBaseWidth;
    const int logicalH = cfg.uiHeight >= kMinHeight ? cfg.uiHeight : kBaseHeight;
    setSize (juce::roundToInt ((float) logicalW * scale()), juce::roundToInt ((float) logicalH * scale()));
}

void FoundryAudioProcessorEditor::updateResizeLimits()
{
    setResizeLimits (juce::roundToInt ((float) kMinWidth * scale()), juce::roundToInt ((float) kMinHeight * scale()),
                     8000, 6000);
}

void FoundryAudioProcessorEditor::applyThemeByName (const juce::String& name)
{
    pt::setCurrentTheme (name);
    applyTheme (pt::getTheme());
    content.repaint();
    repaint();
}

void FoundryAudioProcessorEditor::openPattern (const juce::File& file)
{
    foundry::PatternFile pf;
    juce::String err;
    if (! processor.loadPattern (file, pf, err))
    {
        banner.setMessage (err, true, false);
        resized();
        return;
    }

    showTab (generatorTab);
    const auto name = file.getFileNameWithoutExtension();
    if (pf.kind == foundry::PatternKind::Bass)
    {
        generatorPanel.showPage (pt::GeneratorPanel::bass);
        generatorPanel.bassPanel.applyPatternFile (pf, name);
    }
    else
    {
        generatorPanel.showPage (pt::GeneratorPanel::drums);
        generatorPanel.drumPanel.applyPatternFile (pf, name);
    }
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
    splitter->setVisible (! show);
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
        for (const auto& path : pt::dnd::pathsFrom (desc))
            if (juce::File (path).existsAsFile() && foundry::isSupportedAudioFile (juce::File (path)))
                files.add (path);
        return ! files.isEmpty();
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
            const int w = juce::roundToInt (kMinWidth * scale);
            const int h = juce::roundToInt (kMinHeight * scale);
            if (w <= area.getWidth() - 40 && h <= area.getHeight() - 80)
                break;
            --idx;
        }
    }

    return options[juce::jmax (0, idx)];
}

void FoundryAudioProcessorEditor::setZoomPercent (int percent, bool store)
{
    // Keep the logical layout, scale everything.
    const float oldScale = scale();
    const int logicalW = juce::roundToInt ((float) getWidth() / oldScale);
    const int logicalH = juce::roundToInt ((float) getHeight() / oldScale);

    zoomPercent = percent;
    footer.setZoomPercent (zoomPercent);
    updateResizeLimits();
    setSize (juce::roundToInt ((float) juce::jmax (kMinWidth, logicalW) * scale()),
             juce::roundToInt ((float) juce::jmax (kMinHeight, logicalH) * scale()));

    if (store)
        storeZoom (zoomPercent);
}

void FoundryAudioProcessorEditor::resized()
{
    const int logicalW = juce::jmax (kMinWidth, juce::roundToInt ((float) getWidth() / scale()));
    const int logicalH = juce::jmax (kMinHeight, juce::roundToInt ((float) getHeight() / scale()));
    content.setTransform ({});
    content.setBounds (0, 0, logicalW, logicalH);
    content.setTransform (juce::AffineTransform::scale (scale()));

    auto& cfg = processor.getConfig().get();
    cfg.uiWidth = logicalW;
    cfg.uiHeight = logicalH;

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
        browserHeight = juce::jlimit (kMinBrowserH, juce::jmax (kMinBrowserH, b.getHeight() - kMinMainH - kSplitterH),
                                      browserHeight);
        browser.setBounds (b.removeFromBottom (browserHeight).reduced (6, 2));
        splitter->setBounds (b.removeFromBottom (kSplitterH));
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
