#include "PluginEditor.h"

namespace
{
    juce::PropertiesFile::Options zoomStoreOptions()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "BinauralBeats";
        o.filenameSuffix = "settings";
        o.folderName = "BinauralBeats";
        o.osxLibrarySubFolder = "Application Support";
        return o;
    }
}

//==============================================================================
BinauralBeatsAudioProcessorEditor::BinauralBeatsAudioProcessorEditor (BinauralBeatsAudioProcessor& p)
    : AudioProcessorEditor (&p),
      plugin (p),
      mainPanel (p),
      header (p.getAPVTS(), pt::pid::bypass),
      footer (p.getAPVTS(), p.getPresetManager()),
      presetBrowser (p.getPresetManager())
{
    setLookAndFeel (&lookAndFeel);
    header.setTitle ("BinauralBeats", "pure tones  ·  binaural beats  ·  export");

    addAndMakeVisible (content);
    content.addAndMakeVisible (mainPanel);
    content.addAndMakeVisible (header);
    content.addAndMakeVisible (footer);

    // Children of content, not of the editor directly, so the same zoom
    // transform that scales the rest of the UI scales these overlays too.
    content.addChildComponent (savePanel);
    content.addChildComponent (presetBrowser);

    savePanel.onSave = [this] (const juce::String& name, const juce::String& category)
    {
        plugin.getPresetManager().saveUser (name, category);
        footer.refreshPresetList();
    };

    presetBrowser.onPresetLoaded = [this] { footer.refreshPresetList(); };

    footer.onRequestSave = [this]
    {
        savePanel.show (plugin.getPresetManager().getCurrentName());
    };

    footer.onBrowsePresets = [this] { presetBrowser.show(); };

    footer.onZoomChanged = [this] (int percent) { setZoomPercent (percent, true); };

    applyTheme (pt::getTheme());

    setResizable (false, false);
    setZoomPercent (clampZoomToDisplay (loadStoredZoom()), false);
}

BinauralBeatsAudioProcessorEditor::~BinauralBeatsAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void BinauralBeatsAudioProcessorEditor::applyTheme (const pt::Theme& t)
{
    lookAndFeel.setTheme (t);
    mainPanel.setTheme (t);
    header.setTheme (t);
    footer.setTheme (t);
    presetBrowser.setTheme (t);
    savePanel.setTheme (t);
}

//==============================================================================
int BinauralBeatsAudioProcessorEditor::loadStoredZoom() const
{
    juce::ApplicationProperties props;
    props.setStorageParameters (zoomStoreOptions());

    if (auto* file = props.getUserSettings())
        return file->getIntValue (pt::stateID::zoom, 100);

    return 100;
}

void BinauralBeatsAudioProcessorEditor::storeZoom (int percent) const
{
    juce::ApplicationProperties props;
    props.setStorageParameters (zoomStoreOptions());

    if (auto* file = props.getUserSettings())
    {
        file->setValue (pt::stateID::zoom, percent);
        file->saveIfNeeded();
    }
}

int BinauralBeatsAudioProcessorEditor::clampZoomToDisplay (int percent) const
{
    const auto& options = pt::FooterBar::zoomOptions();
    int idx = options.indexOf (percent);
    if (idx < 0)
        idx = options.indexOf (100);

    if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto area = display->userBounds.toNearestInt();

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

void BinauralBeatsAudioProcessorEditor::setZoomPercent (int percent, bool store)
{
    zoomPercent = percent;
    const float scale = (float) zoomPercent / 100.0f;

    // Content itself always stays the fixed design size; only its transform
    // scales, so lay it out before resizing the editor - the editor's
    // resized() below reads content's bounds to place the overlays.
    content.setBounds (0, 0, kBaseWidth, kBaseHeight);
    content.setTransform (juce::AffineTransform::scale (scale));
    setSize (juce::roundToInt (kBaseWidth * scale), juce::roundToInt (kBaseHeight * scale));

    footer.setZoomPercent (zoomPercent);

    if (store)
        storeZoom (zoomPercent);
}

//==============================================================================
void BinauralBeatsAudioProcessorEditor::resized()
{
    auto b = content.getLocalBounds();

    header.setBounds (b.removeFromTop (56));
    footer.setBounds (b.removeFromBottom (76));
    mainPanel.setBounds (b.reduced (10));

    savePanel.setBounds (content.getLocalBounds());
    presetBrowser.setBounds (content.getLocalBounds());
}

void BinauralBeatsAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto& t = lookAndFeel.getCurrentTheme();

    juce::ColourGradient bg (t.bgTop, 0.0f, 0.0f, t.bgBottom, 0.0f, (float) getHeight(), false);
    g.setGradientFill (bg);
    g.fillAll();
}
