#include "MainPanel.h"
#include "../PluginProcessor.h"

namespace pt
{

//==============================================================================
ExportTile::ExportTile (juce::String titleText, juce::String hintText)
    : title (std::move (titleText)), hint (std::move (hintText))
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    setTooltip ("Drag onto a track or desktop to export, or click to save");
}

void ExportTile::setTheme (const Theme& t)
{
    theme = t;
    repaint();
}

void ExportTile::setActive (bool shouldBeActive)
{
    active = shouldBeActive;
    repaint();
}

void ExportTile::mouseDown (const juce::MouseEvent&)
{
    dragStarted = false;
}

void ExportTile::mouseDrag (const juce::MouseEvent& e)
{
    if (dragStarted || e.getDistanceFromDragStart() < 8)
        return;

    dragStarted = true;

    if (onDragOut)
        onDragOut();
}

void ExportTile::mouseUp (const juce::MouseEvent& e)
{
    if (! dragStarted && e.mouseWasClicked() && onSaveClick)
        onSaveClick();
}

void ExportTile::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    const float r = ui::controlCorner;
    const float dim = active ? 1.0f : 0.55f;

    juce::ColourGradient fill (theme.bgBottom.brighter (hot ? 0.16f : 0.08f).withAlpha (0.92f * dim),
                               b.getCentreX(), b.getY(),
                               theme.bgBottom.withAlpha (0.90f * dim),
                               b.getCentreX(), b.getBottom(), false);
    g.setGradientFill (fill);
    g.fillRoundedRectangle (b, r);

    g.setColour ((hot ? theme.accent : theme.panelEdge).withAlpha (0.85f * dim));
    g.drawRoundedRectangle (b, r, 1.0f);

    // Tiny waveform glyph so the tile reads as a file, not a button.
    {
        auto wave = juce::Rectangle<float> (b.getX() + 12.0f, b.getCentreY() - 10.0f, 22.0f, 20.0f);
        juce::Path p;
        p.startNewSubPath (wave.getX(), wave.getCentreY());
        const int steps = 8;
        for (int i = 1; i <= steps; ++i)
        {
            const float x = wave.getX() + wave.getWidth() * (float) i / (float) steps;
            const float y = wave.getCentreY()
                          + std::sin ((float) i * 1.15f) * wave.getHeight() * 0.42f;
            p.lineTo (x, y);
        }
        g.setColour (theme.accent.withAlpha (0.9f * dim));
        g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }

    g.setFont (BinauralBeatsLookAndFeel::labelFont (11.0f, true, 0.06f));
    g.setColour (theme.text.withAlpha (dim));
    g.drawText (title, juce::roundToInt (b.getX() + 40.0f), juce::roundToInt (b.getY() + 12.0f),
                juce::roundToInt (b.getWidth() - 50.0f), 18, juce::Justification::centredLeft);

    g.setFont (BinauralBeatsLookAndFeel::labelFont (9.0f, false, 0.04f));
    g.setColour (theme.textDim.withAlpha (0.9f * dim));
    g.drawText (hint, juce::roundToInt (b.getX() + 40.0f), juce::roundToInt (b.getY() + 30.0f),
                juce::roundToInt (b.getWidth() - 50.0f), 16, juce::Justification::centredLeft);
}

//==============================================================================
MainPanel::MainPanel (BinauralBeatsAudioProcessor& processor)
    : proc (processor),
      apvts (processor.getAPVTS()),
      modeSwitch (apvts, pid::mode, juce::StringArray { "Tone", "Binaural", "Both" }),
      leftField     (apvts, pid::leftFreq,  "Left",     "Hz", 1),
      rightField    (apvts, pid::rightFreq, "Right",    "Hz", 1),
      beatField     (apvts, pid::beatFreq,  "Beat",     "Hz", 2),
      durationField (apvts, pid::duration,  "Duration", "s",  1),
      fadeInField   (apvts, pid::fadeIn,    "Fade In",  "s",  1),
      fadeOutField  (apvts, pid::fadeOut,   "Fade Out", "s",  1),
      theme (getTheme())
{
    addAndMakeVisible (modeSwitch);
    addAndMakeVisible (leftField);
    addAndMakeVisible (rightField);
    addAndMakeVisible (beatField);
    addAndMakeVisible (durationField);
    addAndMakeVisible (fadeInField);
    addAndMakeVisible (fadeOutField);
    addAndMakeVisible (playButton);
    addAndMakeVisible (timeLabel);
    addAndMakeVisible (statusLabel);
    addAndMakeVisible (binauralTile);
    addAndMakeVisible (toneTile);
    addAndMakeVisible (exportBinauralButton);
    addAndMakeVisible (exportToneButton);
    addAndMakeVisible (exportBothButton);

    leftField.setTooltip  ("Left-ear carrier. Changing it keeps the beat and moves the right ear.");
    rightField.setTooltip ("Right-ear carrier. Changing it updates the beat frequency.");
    beatField.setTooltip  ("|Right - Left|. The pulsing difference you hear with headphones.");
    durationField.setTooltip ("Length of playback and of the exported WAV. Accepts seconds or mm:ss.");
    fadeInField.setTooltip  ("Raised-cosine fade at the start of the file.");
    fadeOutField.setTooltip ("Raised-cosine fade at the end of the file.");
    modeSwitch.setTooltip ("Pure tone is mono. Binaural is stereo L/R. Both mixes them on preview and exports two files.");

    playButton.setTriggeredOnMouseDown (false);
    playButton.onClick = [this] { togglePlayback(); };

    timeLabel.setJustificationType (juce::Justification::centredLeft);
    timeLabel.setInterceptsMouseClicks (false, false);

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setInterceptsMouseClicks (false, false);

    binauralTile.onDragOut  = [this] { dragExport (ExportKind::binaural); };
    binauralTile.onSaveClick = [this] { saveExport (ExportKind::binaural); };
    toneTile.onDragOut      = [this] { dragExport (ExportKind::pureTone); };
    toneTile.onSaveClick    = [this] { saveExport (ExportKind::pureTone); };

    exportBinauralButton.onClick = [this] { saveExport (ExportKind::binaural); };
    exportToneButton.onClick     = [this] { saveExport (ExportKind::pureTone); };
    exportBothButton.onClick     = [this] { saveBoth(); };

    apvts.addParameterListener (pid::leftFreq,  this);
    apvts.addParameterListener (pid::rightFreq, this);
    apvts.addParameterListener (pid::beatFreq,  this);
    apvts.addParameterListener (pid::mode,      this);

    setWantsKeyboardFocus (true);
    updateBeatCaption();
    updatePlayButton();
    startTimerHz (20);
}

MainPanel::~MainPanel()
{
    stopTimer();
    apvts.removeParameterListener (pid::leftFreq,  this);
    apvts.removeParameterListener (pid::rightFreq, this);
    apvts.removeParameterListener (pid::beatFreq,  this);
    apvts.removeParameterListener (pid::mode,      this);
}

void MainPanel::setTheme (const Theme& t)
{
    theme = t;
    modeSwitch.setThemeColours (t);
    leftField.setThemeColours (t);
    rightField.setThemeColours (t);
    beatField.setThemeColours (t);
    durationField.setThemeColours (t);
    fadeInField.setThemeColours (t);
    fadeOutField.setThemeColours (t);
    binauralTile.setTheme (t);
    toneTile.setTheme (t);

    playButton.setColour (juce::TextButton::buttonColourId, t.accent);
    playButton.setColour (juce::TextButton::buttonOnColourId, t.accent);
    for (auto* b : { &exportBinauralButton, &exportToneButton, &exportBothButton })
    {
        b->setColour (juce::TextButton::buttonColourId, t.accent);
        b->setColour (juce::TextButton::buttonOnColourId, t.accent);
    }

    timeLabel.setColour (juce::Label::textColourId, t.text);
    statusLabel.setColour (juce::Label::textColourId, t.textDim);
    timeLabel.setFont (BinauralBeatsLookAndFeel::labelFont (12.0f, true, 0.04f));
    statusLabel.setFont (BinauralBeatsLookAndFeel::labelFont (10.0f, false, 0.02f));
    repaint();
}

void MainPanel::setParamValue (const juce::String& id, float actual)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (actual));
}

void MainPanel::syncFrequencies (const juce::String& sourceId)
{
    if (syncing)
        return;

    const juce::ScopedValueSetter<bool> guard (syncing, true);

    const float left  = apvts.getRawParameterValue (pid::leftFreq)->load();
    const float right = apvts.getRawParameterValue (pid::rightFreq)->load();
    const float beat  = apvts.getRawParameterValue (pid::beatFreq)->load();

    if (sourceId == pid::leftFreq || sourceId == pid::beatFreq)
    {
        const float newRight = juce::jlimit (20.0f, 20000.0f, left + beat);
        if (std::abs (newRight - right) > 0.001f)
            setParamValue (pid::rightFreq, newRight);
    }
    else if (sourceId == pid::rightFreq)
    {
        const float newBeat = juce::jlimit (0.0f, 60.0f, std::abs (right - left));
        if (std::abs (newBeat - beat) > 0.001f)
            setParamValue (pid::beatFreq, newBeat);
    }

    updateBeatCaption();
}

void MainPanel::updateBeatCaption()
{
    const float beat = apvts.getRawParameterValue (pid::beatFreq)->load();
    beatField.setCaption (bandNameForBeat (beat));
}

void MainPanel::parameterChanged (const juce::String& parameterID, float)
{
    if (parameterID == pid::mode)
    {
        const int mode = juce::roundToInt (apvts.getRawParameterValue (pid::mode)->load());
        binauralTile.setActive (mode != (int) OutputMode::pureTone);
        toneTile.setActive (mode != (int) OutputMode::binaural);
        return;
    }

    syncFrequencies (parameterID);
}

void MainPanel::togglePlayback()
{
    if (proc.isPlaying())
        proc.stopPlayback();
    else
        proc.startPlayback();

    updatePlayButton();
}

void MainPanel::updatePlayButton()
{
    const bool on = proc.isPlaying();
    playButton.setButtonText (on ? "STOP" : "PLAY");
    playButton.setToggleState (on, juce::dontSendNotification);
}

void MainPanel::timerCallback()
{
    updatePlayButton();

    const float dur = juce::jmax (0.5f, apvts.getRawParameterValue (pid::duration)->load());
    const double t  = proc.getPlaybackSeconds();
    progress = proc.isPlaying() ? (float) juce::jlimit (0.0, 1.0, t / (double) dur) : 0.0f;

    auto fmt = [] (double sec)
    {
        const int s = juce::jmax (0, (int) std::floor (sec));
        return juce::String (s / 60) + ":" + juce::String (s % 60).paddedLeft ('0', 2);
    };

    timeLabel.setText (fmt (proc.isPlaying() ? t : 0.0) + "  /  " + fmt ((double) dur),
                       juce::dontSendNotification);
    repaint();
}

void MainPanel::dragExport (ExportKind kind)
{
    juce::String error;
    juce::MouseCursor::showWaitCursor();
    const auto file = proc.renderTempWav (kind, error);
    juce::MouseCursor::hideWaitCursor();

    if (! file.existsAsFile())
    {
        showStatus (error.isNotEmpty() ? error : "Export failed", true);
        return;
    }

    if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
        c->performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
    else
        showStatus ("Ready: " + file.getFileName(), false);
}

void MainPanel::saveExport (ExportKind kind)
{
    auto spec = proc.currentSpec();
    const auto suggested = juce::File::getSpecialLocation (juce::File::userDesktopDirectory)
                               .getChildFile (makeExportFileName (spec, kind));

    chooser = std::make_unique<juce::FileChooser> ("Save WAV", suggested, "*.wav");
    constexpr auto flags = juce::FileBrowserComponent::saveMode
                         | juce::FileBrowserComponent::canSelectFiles
                         | juce::FileBrowserComponent::warnAboutOverwriting;

    chooser->launchAsync (flags, [this, kind] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file == juce::File())
            return;

        if (! file.hasFileExtension (".wav"))
            file = file.withFileExtension ("wav");

        juce::String error;
        if (proc.renderWavToFile (file, kind, error))
            showStatus ("Saved " + file.getFileName(), false);
        else
            showStatus (error, true);
    });
}

void MainPanel::saveBoth()
{
    const auto start = juce::File::getSpecialLocation (juce::File::userDesktopDirectory);

    chooser = std::make_unique<juce::FileChooser> ("Choose a folder for both WAVs", start);
    constexpr auto flags = juce::FileBrowserComponent::openMode
                         | juce::FileBrowserComponent::canSelectDirectories;

    chooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
    {
        auto dir = fc.getResult();
        if (dir == juce::File())
            return;

        if (! dir.isDirectory())
            dir = dir.getParentDirectory();

        auto spec = proc.currentSpec();
        juce::String error;
        const auto binaural = dir.getChildFile (makeExportFileName (spec, ExportKind::binaural));
        const auto tone     = dir.getChildFile (makeExportFileName (spec, ExportKind::pureTone));

        const bool okB = proc.renderWavToFile (binaural, ExportKind::binaural, error);
        const bool okT = proc.renderWavToFile (tone,     ExportKind::pureTone, error);

        if (okB && okT)
            showStatus ("Saved binaural + tone WAVs", false);
        else
            showStatus (error.isNotEmpty() ? error : "Export failed", true);
    });
}

void MainPanel::showStatus (const juce::String& text, bool isError)
{
    statusLabel.setColour (juce::Label::textColourId,
                           isError ? juce::Colour (0xffe07a7a) : theme.textDim);
    statusLabel.setText (text, juce::dontSendNotification);
}

bool MainPanel::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey)
    {
        togglePlayback();
        return true;
    }

    return false;
}

void MainPanel::resized()
{
    auto b = getLocalBounds().reduced (28, 20);

    modeSwitch.setBounds (b.removeFromTop (28));
    b.removeFromTop (18);

    auto freqRow = b.removeFromTop (68);
    const int col = freqRow.getWidth() / 3;
    leftField .setBounds (freqRow.removeFromLeft (col).reduced (6, 0));
    rightField.setBounds (freqRow.removeFromLeft (col).reduced (6, 0));
    beatField .setBounds (freqRow.reduced (6, 0));

    b.removeFromTop (16);
    auto envRow = b.removeFromTop (68);
    durationField.setBounds (envRow.removeFromLeft (col).reduced (6, 0));
    fadeInField  .setBounds (envRow.removeFromLeft (col).reduced (6, 0));
    fadeOutField .setBounds (envRow.reduced (6, 0));

    b.removeFromTop (14);
    auto playRow = b.removeFromTop (34);
    playButton.setBounds (playRow.removeFromLeft (108));
    playRow.removeFromLeft (12);
    timeLabel.setBounds (playRow.removeFromLeft (110));
    playRow.removeFromLeft (8);
    statusLabel.setBounds (playRow);

    b.removeFromTop (16);
    auto tileRow = b.removeFromTop (58);
    binauralTile.setBounds (tileRow.removeFromLeft (tileRow.getWidth() / 2).reduced (4, 0));
    toneTile.setBounds (tileRow.reduced (4, 0));

    b.removeFromTop (8);
    auto btnRow = b.removeFromTop (26);
    const int bw = (btnRow.getWidth() - 16) / 3;
    exportBinauralButton.setBounds (btnRow.removeFromLeft (bw));
    btnRow.removeFromLeft (8);
    exportToneButton.setBounds (btnRow.removeFromLeft (bw));
    btnRow.removeFromLeft (8);
    exportBothButton.setBounds (btnRow);
}

void MainPanel::paint (juce::Graphics& g)
{
    const auto panel = getLocalBounds().toFloat().reduced (10.0f);
    BinauralBeatsLookAndFeel::drawPanel (g, panel, theme);

    auto b = getLocalBounds().reduced (28, 20);
    b.removeFromTop (28);

    auto drawSection = [&g, this] (juce::Rectangle<int> area, const juce::String& text)
    {
        g.setFont (BinauralBeatsLookAndFeel::labelFont (8.5f, true, 0.14f));
        g.setColour (theme.textDim.withAlpha (0.75f));
        g.drawText (text, area, juce::Justification::centredLeft);
    };

    drawSection (b.removeFromTop (18), "FREQUENCIES");
    b.removeFromTop (68);
    b.removeFromTop (16);
    drawSection (juce::Rectangle<int> (b.getX(), b.getY() - 16, b.getWidth(), 16), "DURATION  &  FADES");

    // Playback progress, sitting under the PLAY button row.
    b.removeFromTop (68);
    b.removeFromTop (14);
    auto playRow = b.removeFromTop (34);
    auto barArea = juce::Rectangle<float> ((float) (playRow.getX() + 120),
                                           (float) playRow.getBottom() - 4.0f,
                                           110.0f, 3.0f);
    g.setColour (theme.bgBottom.darker (0.2f).withAlpha (0.9f));
    g.fillRoundedRectangle (barArea, 1.5f);
    if (progress > 0.001f)
    {
        g.setColour (theme.accent);
        g.fillRoundedRectangle (barArea.withWidth (barArea.getWidth() * progress), 1.5f);
    }

    b.removeFromTop (16);
    drawSection (juce::Rectangle<int> (b.getX(), b.getY() - 14, b.getWidth(), 14), "EXPORT  ·  DRAG A TILE OR SAVE");
}

} // namespace pt
