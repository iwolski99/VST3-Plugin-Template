#include "DrumPanel.h"

namespace pt
{

DrumPanel::DrumPanel (FoundryAudioProcessor& p)
    : processor (p)
{
    const char* lockNames[] = { "KICK", "CLAP", "HAT", "RIDE" };
    for (int i = 0; i < 4; ++i)
    {
        lockButtons[i].setButtonText ("LOCK");
        lockButtons[i].setClickingTogglesState (true);
        lockLabels[i].setText (lockNames[i] + juce::String ("  Random"), juce::dontSendNotification);
        lockLabels[i].setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (lockButtons[i]);
        addAndMakeVisible (lockLabels[i]);
        lockButtons[i].onClick = [this, i]
        {
            auto& lk = processor.getLock (i);
            lk.locked = lockButtons[i].getToggleState();
            if (! lk.locked)
            {
                lk.path.clear();
                lk.name.clear();
            }
            refreshLockLabels();
        };
    }

    styleBox.addItem ("Riddim", 1);
    styleBox.addItem ("Dubstep", 2);
    styleBox.setSelectedId (processor.getUiGenre() == foundry::Genre::Dubstep ? 2 : 1,
                            juce::dontSendNotification);
    styleBox.onChange = [this]
    {
        processor.setUiGenre (styleBox.getSelectedId() == 2 ? foundry::Genre::Dubstep
                                                            : foundry::Genre::Riddim);
        if (tempoBox.getSelectedId() == 2)
            bpmBox.setText (juce::String (foundry::genreDefaultBpm (processor.getUiGenre()), 0),
                            juce::dontSendNotification);
    };

    tempoBox.addItem ("Host", 1);
    tempoBox.addItem ("Genre", 2);
    tempoBox.addItem ("Custom", 3);
    tempoBox.setSelectedId ((int) processor.getConfig().get().tempoMode + 1, juce::dontSendNotification);
    tempoBox.onChange = [this]
    {
        processor.getConfig().get().tempoMode = (foundry::TempoMode) (tempoBox.getSelectedId() - 1);
        bpmBox.setEnabled (tempoBox.getSelectedId() == 3);
    };

    bpmBox.setInputRestrictions (6, "0123456789.");
    bpmBox.setText (juce::String (processor.getConfig().get().customBpm, 0), juce::dontSendNotification);
    bpmBox.setEnabled (tempoBox.getSelectedId() == 3);
    bpmBox.onReturnKey = bpmBox.onFocusLost = [this]
    {
        processor.getConfig().get().customBpm = juce::jlimit (40.0, 300.0, bpmBox.getText().getDoubleValue());
    };

    barsBox.addItem ("1 bar", 1);
    barsBox.addItem ("2 bars", 2);
    barsBox.addItem ("4 bars", 3);
    barsBox.setSelectedId (processor.getUiBars() == 4 ? 3 : processor.getUiBars(), juce::dontSendNotification);
    barsBox.onChange = [this]
    {
        const int bars = barsBox.getSelectedId() == 3 ? 4 : barsBox.getSelectedId();
        processor.setUiBars (bars);
        grid.setPattern (processor.getDrumPattern());
    };

    quantity.setValue (processor.getUiQuantity(), false);
    quantity.onChanged = [this] (int v) { processor.setUiQuantity (v); };

    fullToggle.setClickingTogglesState (true);
    stemsToggle.setClickingTogglesState (true);
    fullToggle.setToggleState (processor.renderFull, juce::dontSendNotification);
    stemsToggle.setToggleState (processor.renderStems, juce::dontSendNotification);
    fullToggle.onClick = [this] { processor.renderFull = fullToggle.getToggleState(); };
    stemsToggle.onClick = [this] { processor.renderStems = stemsToggle.getToggleState(); };

    generateButton.onClick = [this] { generate(); };
    resetButton.onClick = [this]
    {
        processor.getDrumPattern() = foundry::defaultDrumPattern (processor.getUiGenre(), processor.getUiBars());
        grid.setPattern (processor.getDrumPattern());
    };
    saveDefaultButton.onClick = [this]
    {
        processor.getDrumPattern() = grid.getPattern();
        processor.getConfig().get().savedDrumPatternJson = grid.getPattern().toJson();
        juce::String err;
        processor.persistConfig (err);
        banner.setMessage ("Pattern saved as your default.", false, false);
    };
    savePatternButton.setTooltip ("Save this groove to Patterns > Drums, where the browser shows it");
    savePatternButton.onClick = [this]
    {
        const auto initial = "Drum Pattern " + juce::String (processor.listPatterns (foundry::PatternKind::Drums).size() + 1);
        juce::Component::SafePointer<DrumPanel> safe (this);
        askForName ("Save drum pattern", "Name this pattern:", initial, [safe] (const juce::String& name)
        {
            if (safe == nullptr)
                return;
            foundry::PatternFile pf;
            pf.kind = foundry::PatternKind::Drums;
            pf.grid = safe->grid.getPattern();
            pf.genre = safe->processor.getUiGenre();
            juce::String err;
            const auto f = safe->processor.savePattern (pf, name, err);
            safe->banner.setMessage (f.existsAsFile() ? "Saved " + f.getFileNameWithoutExtension() + " to Patterns > Drums." : err,
                                     ! f.existsAsFile(), false);
            safe->refreshPatternList();
            if (safe->onPatternsChanged)
                safe->onPatternsChanged();
        });
    };

    loadPatternBox.setTooltip ("Patterns saved in Patterns > Drums");
    loadPatternBox.onChange = [this]
    {
        const int idx = loadPatternBox.getSelectedId() - 2;
        if (idx < 0 || idx >= patternFiles.size())
            return;
        foundry::PatternFile pf;
        juce::String err;
        if (processor.loadPattern (patternFiles[idx], pf, err))
            applyPatternFile (pf, patternFiles[idx].getFileNameWithoutExtension());
        else
            banner.setMessage (err, true, false);
        loadPatternBox.setSelectedId (1, juce::dontSendNotification);
    };
    refreshPatternList();

    grid.setPattern (processor.getDrumPattern());
    grid.onChanged = [this] { processor.getDrumPattern() = grid.getPattern(); };

    banner.onOpenSettings = [this] { if (onOpenSettings) onOpenSettings(); };

    addAndMakeVisible (styleBox);
    addAndMakeVisible (tempoBox);
    addAndMakeVisible (bpmBox);
    addAndMakeVisible (barsBox);
    addAndMakeVisible (quantity);
    addAndMakeVisible (fullToggle);
    addAndMakeVisible (stemsToggle);
    addAndMakeVisible (generateButton);
    addAndMakeVisible (resetButton);
    addAndMakeVisible (saveDefaultButton);
    addAndMakeVisible (savePatternButton);
    addAndMakeVisible (loadPatternBox);
    addAndMakeVisible (grid);
    addChildComponent (banner);

    refreshLockLabels();
}

void DrumPanel::setTheme (const Theme& t)
{
    theme = t;
    grid.setTheme (t);
    quantity.setTheme (t);
    banner.setTheme (t);
    generateButton.setColour (juce::TextButton::buttonColourId, t.accent);
    generateButton.setColour (juce::TextButton::buttonOnColourId, t.accent);
    for (auto* b : { &fullToggle, &stemsToggle, &resetButton, &saveDefaultButton, &savePatternButton })
        b->setColour (juce::TextButton::buttonColourId, t.accent);
    for (auto& b : lockButtons)
        b.setColour (juce::TextButton::buttonColourId, t.accentAlt);
    for (auto& l : lockLabels)
        l.setColour (juce::Label::textColourId, t.text);
    repaint();
}

void DrumPanel::syncFromProcessor()
{
    quantity.setValue (processor.getUiQuantity(), false);
    barsBox.setSelectedId (processor.getUiBars() == 4 ? 3 : processor.getUiBars(), juce::dontSendNotification);
    grid.setPattern (processor.getDrumPattern());
    refreshLockLabels();
    refreshPatternList();
}

void DrumPanel::refreshPatternList()
{
    patternFiles = processor.listPatterns (foundry::PatternKind::Drums);
    loadPatternBox.clear (juce::dontSendNotification);
    loadPatternBox.addItem (patternFiles.isEmpty() ? "No saved patterns yet" : "Load pattern...", 1);
    for (int i = 0; i < patternFiles.size(); ++i)
        loadPatternBox.addItem (patternFiles[i].getFileNameWithoutExtension(), i + 2);
    loadPatternBox.setSelectedId (1, juce::dontSendNotification);
}

void DrumPanel::applyPatternFile (const foundry::PatternFile& pf, const juce::String& name)
{
    processor.setUiBars (pf.grid.bars);
    processor.getDrumPattern() = pf.grid;
    syncFromProcessor();

    // A loop's own pattern brings its sounds with it: lock them, so
    // regenerating keeps the sounds and only the groove changes.
    int locked = 0;
    if (pf.hasSamples())
    {
        for (int r = 0; r < 4; ++r)
        {
            auto& lk = processor.getLock (r);
            const juce::File f (pf.samples[(size_t) r]);
            lk.locked = f.existsAsFile();
            lk.path = lk.locked ? f.getFullPathName() : juce::String();
            lk.name = lk.locked ? f.getFileName() : juce::String();
            locked += lk.locked ? 1 : 0;
        }
        refreshLockLabels();
    }

    banner.setMessage (locked > 0
                           ? "Opened " + name + " with its " + juce::String (locked)
                                 + " sounds locked. Edit the grid, then GENERATE (quantity 1 remakes just this loop)."
                           : "Loaded pattern " + name + ".",
                       false, false);
}

void DrumPanel::applyLock (const foundry::LibraryItem& item)
{
    int row = -1;
    switch (item.category)
    {
        case foundry::Category::OneShotKick:  row = 0; break;
        case foundry::Category::OneShotClap:
        case foundry::Category::OneShotSnare: row = 1; break;
        case foundry::Category::OneShotHat:   row = 2; break;
        case foundry::Category::OneShotRide:  row = 3; break;
        case foundry::Category::DrumLoopsRiddim:
        case foundry::Category::DrumLoopsDubstep:
        case foundry::Category::BassLoopsBase:
        case foundry::Category::BassLoopsSpliced:
        case foundry::Category::OneShotBass:
        case foundry::Category::Riff:
        case foundry::Category::Unknown:
            break;
    }
    if (row < 0)
        return;

    auto& lk = processor.getLock (row);
    lk.locked = true;
    lk.path = item.path;
    lk.name = item.filename;
    lockButtons[row].setToggleState (true, juce::dontSendNotification);
    refreshLockLabels();
}

void DrumPanel::refreshLockLabels()
{
    const char* names[] = { "Kick", "Clap", "Hat", "Ride" };
    for (int i = 0; i < 4; ++i)
    {
        const auto& lk = processor.getLock (i);
        lockButtons[i].setToggleState (lk.locked, juce::dontSendNotification);
        lockLabels[i].setText (lk.locked && lk.name.isNotEmpty()
                                   ? juce::String (names[i]) + "  " + lk.name
                                   : juce::String (names[i]) + "  Random",
                               juce::dontSendNotification);
    }
}

void DrumPanel::generate()
{
    processor.getDrumPattern() = grid.getPattern();
    processor.getConfig().get().customBpm = juce::jlimit (40.0, 300.0, bpmBox.getText().getDoubleValue() > 0
                                                                          ? bpmBox.getText().getDoubleValue()
                                                                          : processor.getConfig().get().customBpm);
    auto err = processor.startDrumGenerate();
    if (err.isNotEmpty())
        banner.setMessage (err, true, err.containsIgnoreCase ("folder") || err.containsIgnoreCase ("sample"));
    else
        banner.setMessage ({}, false, false);
}

void DrumPanel::resized()
{
    auto b = getLocalBounds().reduced (16);
    if (banner.isVisible())
        banner.setBounds (b.removeFromTop (34)), b.removeFromTop (8);

    b.removeFromTop (4);
    auto left = b.removeFromLeft (280);
    auto row = left.removeFromTop (22);
    styleBox.setBounds (row.removeFromLeft (130));
    row.removeFromLeft (8);
    barsBox.setBounds (row);

    left.removeFromTop (8);
    row = left.removeFromTop (22);
    tempoBox.setBounds (row.removeFromLeft (90));
    row.removeFromLeft (8);
    bpmBox.setBounds (row.removeFromLeft (70));
    row.removeFromLeft (8);
    quantity.setBounds (row.withHeight (40));

    left.removeFromTop (28);
    row = left.removeFromTop (24);
    fullToggle.setBounds (row.removeFromLeft (130));
    row.removeFromLeft (8);
    stemsToggle.setBounds (row);

    left.removeFromTop (14);
    for (int i = 0; i < 4; ++i)
    {
        row = left.removeFromTop (24);
        lockButtons[i].setBounds (row.removeFromRight (64));
        lockLabels[i].setBounds (row);
        left.removeFromTop (4);
    }

    left.removeFromTop (10);
    generateButton.setBounds (left.removeFromTop (36));
    left.removeFromTop (8);
    row = left.removeFromTop (24);
    resetButton.setBounds (row.removeFromLeft (70));
    row.removeFromLeft (6);
    saveDefaultButton.setBounds (row.removeFromLeft (110));
    row.removeFromLeft (6);
    savePatternButton.setBounds (row);

    left.removeFromTop (8);
    loadPatternBox.setBounds (left.removeFromTop (24));

    b.removeFromLeft (12);
    grid.setBounds (b);
}

void DrumPanel::paint (juce::Graphics& g)
{
    FoundryLookAndFeel::drawPanel (g, getLocalBounds().toFloat().reduced (6.0f), theme);


}

} // namespace pt
