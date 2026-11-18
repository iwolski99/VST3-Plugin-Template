#include "BassPanel.h"

namespace pt
{

BassPanel::BassPanel (FoundryAudioProcessor& p)
    : processor (p)
{
    styleBox.addItem ("Riddim", 1);
    styleBox.addItem ("Dubstep", 2);
    styleBox.setSelectedId (processor.getUiGenre() == foundry::Genre::Dubstep ? 2 : 1, juce::dontSendNotification);
    styleBox.onChange = [this]
    {
        processor.setUiGenre (styleBox.getSelectedId() == 2 ? foundry::Genre::Dubstep : foundry::Genre::Riddim);
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
    bpmBox.onFocusLost = bpmBox.onReturnKey = [this]
    {
        processor.getConfig().get().customBpm = juce::jlimit (40.0, 300.0, bpmBox.getText().getDoubleValue());
    };

    barsBox.addItem ("1 bar", 1);
    barsBox.addItem ("2 bars", 2);
    barsBox.addItem ("4 bars", 3);
    barsBox.onChange = [this]
    {
        processor.setUiBars (barsBox.getSelectedId() == 3 ? 4 : barsBox.getSelectedId());
        grid.setPattern (processor.getBassPattern());
    };

    variationBox.addItem ("Conservative", 1);
    variationBox.addItem ("Normal", 2);
    variationBox.addItem ("Wild", 3);
    variationBox.setSelectedId (2, juce::dontSendNotification);
    variationBox.setTooltip ("How far the rhythm templates stray (template mode)");

    quantity.onChanged = [this] (int v) { processor.setUiQuantity (v); };
    sources.setValue (3, false);

    templatesButton.setClickingTogglesState (false);
    patternButton.setClickingTogglesState (false);
    templatesButton.setTooltip ("Each loop picks one of Foundry's rhythm templates at random");
    patternButton.setTooltip ("Every loop plays the pattern in the grid (rows A-D are the sources)");
    templatesButton.onClick = [this] { setUsePattern (false); };
    patternButton.onClick   = [this] { setUsePattern (true); };

    for (int i = 0; i < 4; ++i)
    {
        lockButtons[i].setButtonText ("LOCK");
        lockButtons[i].setClickingTogglesState (true);
        lockButtons[i].setTooltip ("Keep this source's sample for every loop. Lock one from the browser: right-click a bass sample.");
        lockLabels[i].setJustificationType (juce::Justification::centredLeft);
        lockButtons[i].onClick = [this, i]
        {
            auto& lk = processor.getBassLock (i);
            lk.locked = lockButtons[i].getToggleState();
            if (lk.locked && lk.path.isEmpty())
            {
                lk.locked = false;
                banner.setMessage ("Pick a sound for source " + juce::String::charToString ((juce::juce_wchar) ('A' + i))
                                   + " first: right-click a bass sample in the browser > Lock as bass source.", false, false);
            }
            if (! lk.locked)
            {
                lk.path.clear();
                lk.name.clear();
            }
            refreshLockLabels();
        };
        addAndMakeVisible (lockButtons[i]);
        addAndMakeVisible (lockLabels[i]);
    }

    generateButton.onClick = [this]
    {
        processor.getBassPattern() = grid.getPattern();
        processor.getConfig().get().customBpm = juce::jlimit (40.0, 300.0,
            bpmBox.getText().getDoubleValue() > 0 ? bpmBox.getText().getDoubleValue()
                                                  : processor.getConfig().get().customBpm);
        const auto var = (foundry::Variation) (variationBox.getSelectedId() - 1);
        auto err = processor.startBassGenerate (sources.getValue(), var);
        banner.setMessage (err, err.isNotEmpty(), err.containsIgnoreCase ("folder") || err.containsIgnoreCase ("sample"));
    };

    clearButton.onClick = [this]
    {
        auto pat = grid.getPattern();
        pat.clear();
        grid.setPattern (pat);
        processor.getBassPattern() = pat;
        setUsePattern (true);
    };

    savePatternButton.setTooltip ("Save this groove to Patterns > Bass");
    savePatternButton.onClick = [this]
    {
        const auto initial = "Bass Pattern " + juce::String (processor.listPatterns (foundry::PatternKind::Bass).size() + 1);
        juce::Component::SafePointer<BassPanel> safe (this);
        askForName ("Save bass pattern", "Name this pattern:", initial, [safe] (const juce::String& name)
        {
            if (safe == nullptr)
                return;
            foundry::PatternFile pf;
            pf.kind = foundry::PatternKind::Bass;
            pf.grid = safe->grid.getPattern();
            pf.genre = safe->processor.getUiGenre();
            juce::String err;
            const auto f = safe->processor.savePattern (pf, name, err);
            safe->banner.setMessage (f.existsAsFile() ? "Saved " + f.getFileNameWithoutExtension() + " to Patterns > Bass." : err,
                                     ! f.existsAsFile(), false);
            safe->refreshPatternList();
            if (safe->onPatternsChanged)
                safe->onPatternsChanged();
        });
    };

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

    grid.setRowNames ({ "A", "B", "C", "D" });
    grid.setExclusive (true);
    grid.onChanged = [this]
    {
        processor.getBassPattern() = grid.getPattern();
        if (! processor.bassUsePattern)
            setUsePattern (true);   // drawing in the grid means you want it used
    };

    banner.onOpenSettings = [this] { if (onOpenSettings) onOpenSettings(); };

    for (auto* c : std::initializer_list<juce::Component*> { &styleBox, &tempoBox, &bpmBox, &barsBox, &variationBox,
                                                             &quantity, &sources, &templatesButton, &patternButton,
                                                             &generateButton, &clearButton, &savePatternButton,
                                                             &loadPatternBox, &grid })
        addAndMakeVisible (c);
    addChildComponent (banner);

    syncFromProcessor();
}

void BassPanel::setTheme (const Theme& t)
{
    theme = t;
    quantity.setTheme (t);
    sources.setTheme (t);
    banner.setTheme (t);
    grid.setTheme (t);
    generateButton.setColour (juce::TextButton::buttonColourId, t.accent);
    generateButton.setColour (juce::TextButton::buttonOnColourId, t.accent);
    for (auto* b : { &clearButton, &savePatternButton })
        b->setColour (juce::TextButton::buttonColourId, t.accent);
    for (auto& b : lockButtons)
    {
        b.setColour (juce::TextButton::buttonColourId, t.accentAlt);
        b.setColour (juce::TextButton::buttonOnColourId, t.accent);
    }
    for (auto& l : lockLabels)
        l.setColour (juce::Label::textColourId, t.text);
    setUsePattern (processor.bassUsePattern);
    repaint();
}

void BassPanel::setUsePattern (bool use)
{
    processor.bassUsePattern = use;
    templatesButton.setColour (juce::TextButton::buttonColourId, use ? theme.panelEdge : theme.accent);
    patternButton.setColour (juce::TextButton::buttonColourId, use ? theme.accent : theme.panelEdge);
    variationBox.setEnabled (! use);
    sources.setEnabled (! use);
    grid.setAlpha (use ? 1.0f : 0.55f);
}

void BassPanel::syncFromProcessor()
{
    quantity.setValue (processor.getUiQuantity(), false);
    barsBox.setSelectedId (processor.getUiBars() == 4 ? 3 : processor.getUiBars(), juce::dontSendNotification);
    grid.setPattern (processor.getBassPattern());
    setUsePattern (processor.bassUsePattern);
    refreshLockLabels();
    refreshPatternList();
}

void BassPanel::refreshLockLabels()
{
    const char* names[] = { "A", "B", "C", "D" };
    for (int i = 0; i < 4; ++i)
    {
        const auto& lk = processor.getBassLock (i);
        lockButtons[i].setToggleState (lk.locked, juce::dontSendNotification);
        lockLabels[i].setText (juce::String (names[i]) + "  " + (lk.locked && lk.name.isNotEmpty() ? lk.name : juce::String ("Random")),
                               juce::dontSendNotification);
    }
}

void BassPanel::refreshPatternList()
{
    patternFiles = processor.listPatterns (foundry::PatternKind::Bass);
    loadPatternBox.clear (juce::dontSendNotification);
    loadPatternBox.addItem (patternFiles.isEmpty() ? "No saved patterns yet" : "Load pattern...", 1);
    for (int i = 0; i < patternFiles.size(); ++i)
        loadPatternBox.addItem (patternFiles[i].getFileNameWithoutExtension(), i + 2);
    loadPatternBox.setSelectedId (1, juce::dontSendNotification);
}

void BassPanel::applyLock (int row, const foundry::LibraryItem& item)
{
    auto& lk = processor.getBassLock (row);
    lk.locked = true;
    lk.path = item.path;
    lk.name = item.filename;
    refreshLockLabels();
    banner.setMessage ("Source " + juce::String::charToString ((juce::juce_wchar) ('A' + row)) + " locked to "
                       + item.filename + ".", false, false);
}

void BassPanel::applyPatternFile (const foundry::PatternFile& pf, const juce::String& name)
{
    processor.setUiBars (pf.grid.bars);
    processor.getBassPattern() = pf.grid;
    processor.bassUsePattern = true;

    int locked = 0;
    if (pf.hasSamples())
    {
        for (int r = 0; r < 4; ++r)
        {
            auto& lk = processor.getBassLock (r);
            const juce::File f (pf.samples[(size_t) r]);
            lk.locked = f.existsAsFile();
            lk.path = lk.locked ? f.getFullPathName() : juce::String();
            lk.name = lk.locked ? f.getFileName() : juce::String();
            locked += lk.locked ? 1 : 0;
        }
    }

    syncFromProcessor();
    banner.setMessage (locked > 0
                           ? "Opened " + name + " with its " + juce::String (locked)
                                 + " sounds locked. Edit the grid, then GENERATE (quantity 1 remakes just this loop)."
                           : "Loaded pattern " + name + ".",
                       false, false);
}

void BassPanel::resized()
{
    auto b = getLocalBounds().reduced (16, 12);
    if (banner.isVisible())
    {
        banner.setBounds (b.removeFromTop (30));
        b.removeFromTop (6);
    }

    auto left = b.removeFromLeft (300);

    auto row = left.removeFromTop (22);
    styleBox.setBounds (row.removeFromLeft (104));
    row.removeFromLeft (6);
    barsBox.setBounds (row.removeFromLeft (88));
    row.removeFromLeft (6);
    tempoBox.setBounds (row);
    left.removeFromTop (6);

    row = left.removeFromTop (40);
    quantity.setBounds (row.removeFromLeft (110));
    row.removeFromLeft (8);
    sources.setBounds (row.removeFromLeft (110));
    row.removeFromLeft (8);
    bpmBox.setBounds (row.removeFromBottom (24));
    left.removeFromTop (8);

    row = left.removeFromTop (24);
    templatesButton.setBounds (row.removeFromLeft (row.getWidth() / 2).withTrimmedRight (2));
    patternButton.setBounds (row.withTrimmedLeft (2));
    left.removeFromTop (6);
    variationBox.setBounds (left.removeFromTop (22));
    left.removeFromTop (8);

    for (int i = 0; i < 4; ++i)
    {
        row = left.removeFromTop (24);
        lockButtons[i].setBounds (row.removeFromRight (60));
        lockLabels[i].setBounds (row);
        left.removeFromTop (3);
    }
    left.removeFromTop (6);

    generateButton.setBounds (left.removeFromTop (34));
    left.removeFromTop (6);
    row = left.removeFromTop (24);
    clearButton.setBounds (row.removeFromLeft (row.getWidth() / 2).withTrimmedRight (3));
    savePatternButton.setBounds (row.withTrimmedLeft (3));
    left.removeFromTop (6);
    loadPatternBox.setBounds (left.removeFromTop (22));

    b.removeFromLeft (12);
    grid.setBounds (b);
}

void BassPanel::paint (juce::Graphics& g)
{
    FoundryLookAndFeel::drawPanel (g, getLocalBounds().toFloat().reduced (6.0f), theme);
}

} // namespace pt
