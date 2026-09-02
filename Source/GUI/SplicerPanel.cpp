#include "SplicerPanel.h"

namespace pt
{

SplicerPanel::SplicerPanel (FoundryAudioProcessor& p)
    : processor (p)
{
    sliceBox.addItem ("1/2", 1);
    sliceBox.addItem ("1/4", 2);
    sliceBox.addItem ("1/8", 3);
    sliceBox.addItem ("1/16", 4);
    sliceBox.addItem ("1/8T", 5);
    sliceBox.addItem ("1/16T", 6);
    sliceBox.addItem ("Mixed", 7);
    sliceBox.setSelectedId (2, juce::dontSendNotification);

    patternBox.addItem ("Alternating", 1);
    patternBox.addItem ("Block Swap", 2);
    patternBox.addItem ("Call & Response", 3);
    patternBox.addItem ("Syncopated", 4);
    patternBox.addItem ("Burst", 5);
    patternBox.addItem ("Weighted", 6);
    patternBox.setSelectedId (3, juce::dontSendNotification);

    variationBox.addItem ("Conservative", 1);
    variationBox.addItem ("Normal", 2);
    variationBox.addItem ("Wild", 3);
    variationBox.setSelectedId (2, juce::dontSendNotification);

    barsBox.addItem ("1 bar", 1);
    barsBox.addItem ("2 bars", 2);
    barsBox.addItem ("4 bars", 3);
    barsBox.setSelectedId (2, juce::dontSendNotification);

    quantity.setValue (20, false);

    generateButton.onClick = [this] { generate(); };
    clearButton.onClick = [this]
    {
        selectedPaths.clear();
        rebuildPool();
    };

    pool.onPreview = [this] (const foundry::LibraryItem& it)
    {
        juce::String err;
        processor.previewItem (it, err);
    };
    pool.onSelectionChanged = [this] (const foundry::LibraryItem& it)
    {
        if (! selectedPaths.contains (it.path))
        {
            if (selectedPaths.size() >= 4)
                selectedPaths.remove (0);
            selectedPaths.add (it.path);
        }
        else
        {
            selectedPaths.removeString (it.path);
        }
        rebuildPool();
        pool.setSelectedPath (it.path);
    };
    pool.onToggleFavorite = [this] (const foundry::LibraryItem& it)
    {
        juce::String err;
        processor.getLibrary().setFavorite (it.id, ! it.favorite, err);
        rebuildPool();
    };

    banner.onOpenSettings = [this] { if (onOpenSettings) onOpenSettings(); };

    addAndMakeVisible (pool);
    addAndMakeVisible (sliceBox);
    addAndMakeVisible (patternBox);
    addAndMakeVisible (variationBox);
    addAndMakeVisible (barsBox);
    addAndMakeVisible (quantity);
    addAndMakeVisible (generateButton);
    addAndMakeVisible (clearButton);
    addChildComponent (banner);

    rebuildPool();
}

void SplicerPanel::setTheme (const Theme& t)
{
    theme = t;
    pool.setTheme (t);
    quantity.setTheme (t);
    banner.setTheme (t);
    generateButton.setColour (juce::TextButton::buttonColourId, t.accent);
    generateButton.setColour (juce::TextButton::buttonOnColourId, t.accent);
    clearButton.setColour (juce::TextButton::buttonColourId, t.accentAlt);
    repaint();
}

void SplicerPanel::refresh() { rebuildPool(); }

void SplicerPanel::addSource (const foundry::LibraryItem& item)
{
    if (! selectedPaths.contains (item.path))
    {
        if (selectedPaths.size() >= 4)
            selectedPaths.remove (0);
        selectedPaths.add (item.path);
    }
    rebuildPool();
}

void SplicerPanel::rebuildPool()
{
    auto base = processor.getLibrary().query (foundry::Category::BassLoopsBase, {}, foundry::SortMode::Newest, false);
    auto spliced = processor.getLibrary().query (foundry::Category::BassLoopsSpliced, {}, foundry::SortMode::Newest, false);
    base.insert (base.end(), spliced.begin(), spliced.end());
    pool.setItems (std::move (base));
    repaint();
}

void SplicerPanel::generate()
{
    if (selectedPaths.size() < 2)
    {
        banner.setMessage ("Select at least two bass loops from the list (click to toggle).", true, false);
        return;
    }

    std::vector<juce::File> files;
    juce::StringArray ids;
    int depth = 0;
    for (const auto& p : selectedPaths)
    {
        files.push_back (juce::File (p));
        if (auto it = processor.getLibrary().findByPath (p))
        {
            ids.add (juce::String (it->id));
            depth = juce::jmax (depth, it->generationDepth);
        }
    }

    const int bars = barsBox.getSelectedId() == 3 ? 4 : barsBox.getSelectedId();
    auto err = processor.startSpliceGenerate (files, ids.joinIntoString (","), depth,
                                              (foundry::SliceRes) (sliceBox.getSelectedId() - 1),
                                              (foundry::SpliceMode) (patternBox.getSelectedId() - 1),
                                              (foundry::Variation) (variationBox.getSelectedId() - 1),
                                              quantity.getValue(), bars);
    banner.setMessage (err, err.isNotEmpty(), false);
}

void SplicerPanel::resized()
{
    auto b = getLocalBounds().reduced (16);
    if (banner.isVisible())
        banner.setBounds (b.removeFromTop (34)), b.removeFromTop (8);

    auto right = b.removeFromRight (280);
    b.removeFromRight (12);
    pool.setBounds (b);

    auto row = right.removeFromTop (24);
    sliceBox.setBounds (row);
    right.removeFromTop (8);
    row = right.removeFromTop (24);
    patternBox.setBounds (row);
    right.removeFromTop (8);
    row = right.removeFromTop (24);
    variationBox.setBounds (row);
    right.removeFromTop (8);
    row = right.removeFromTop (24);
    barsBox.setBounds (row);
    right.removeFromTop (10);
    quantity.setBounds (right.removeFromTop (44));
    right.removeFromTop (12);
    generateButton.setBounds (right.removeFromTop (36));
    right.removeFromTop (8);
    clearButton.setBounds (right.removeFromTop (26));
}

void SplicerPanel::paint (juce::Graphics& g)
{
    FoundryLookAndFeel::drawPanel (g, getLocalBounds().toFloat().reduced (6.0f), theme);

    g.setFont (FoundryLookAndFeel::labelFont (8.5f, true, 0.14f));
    g.setColour (theme.textDim);
    g.drawText ("BASS LOOPS  —  click to select 2–4 sources", 22, 14, 420, 12, juce::Justification::centredLeft);

    juce::String sel = "SELECTED  " + juce::String (selectedPaths.size()) + " / 4";
    for (int i = 0; i < selectedPaths.size(); ++i)
        sel += "\n" + juce::String::charToString ((juce::juce_wchar) ('A' + i)) + "  "
               + juce::File (selectedPaths[i]).getFileName();

    g.setFont (FoundryLookAndFeel::labelFont (12.0f, false));
    g.setColour (theme.text);
    auto info = juce::Rectangle<int> (getWidth() - 286, getHeight() - 150, 250, 110);
    g.drawFittedText (sel, info, juce::Justification::topLeft, 6);

    g.setFont (FoundryLookAndFeel::labelFont (8.5f, true, 0.12f));
    g.setColour (theme.textDim);
    const int rx = getWidth() - 286;
    g.drawText ("SLICE", rx, 16, 80, 12, juce::Justification::centredLeft);
}

} // namespace pt
