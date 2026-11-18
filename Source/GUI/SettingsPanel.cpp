#include "SettingsPanel.h"

namespace pt
{

SettingsPanel::SettingsPanel (FoundryAudioProcessor& p)
    : processor (p)
{
    closeButton.onClick = [this] { persist(); if (onClose) onClose(); };
    rootButton.onClick = [this] { pickRoot(); };
    createButton.onClick = [this]
    {
        juce::String err;
        if (! processor.getConfig().createRecommendedStructure (err))
            banner.setMessage (err, true, false);
        else
        {
            persist();
            juce::String dbErr;
            processor.reopenDatabase (dbErr);
            banner.setMessage ("Folder structure created and mapped.", false, false);
            refreshLabels();
            if (onLibraryChanged)
                onLibraryChanged();
        }
    };
    refreshButton.onClick = [this]
    {
        persist();
        auto err = processor.startLibraryScan();
        banner.setMessage (err.isEmpty() ? "Scanning..." : err, err.isNotEmpty(), false);
    };
    rebuildButton.onClick = [this]
    {
        juce::String err;
        if (! processor.getLibrary().rebuild (err))
            banner.setMessage (err, true, false);
        else
        {
            auto scanErr = processor.startLibraryScan();
            banner.setMessage (scanErr.isEmpty() ? "Database cleared. Rescanning..." : scanErr,
                               scanErr.isNotEmpty(), false);
        }
    };
    openDbButton.onClick = [this]
    {
        processor.getLibrary().file().getParentDirectory().revealToUser();
    };

    for (int i = 0; i < foundry::kNumFolderRoles; ++i)
    {
        folderButtons[i].setButtonText ("MAP");
        folderButtons[i].onClick = [this, i] { pickFolder ((foundry::FolderRole) i); };
        addAndMakeVisible (folderButtons[i]);
        addAndMakeVisible (folderLabels[i]);
    }

    rateBox.addItem ("Host sample rate", 1);
    rateBox.addItem ("44.1 kHz", 2);
    rateBox.addItem ("48 kHz", 3);
    const auto& mode = processor.getConfig().get().sampleRateMode;
    rateBox.setSelectedId (mode == "44100" ? 2 : mode == "48000" ? 3 : 1, juce::dontSendNotification);
    rateBox.onChange = [this]
    {
        auto& m = processor.getConfig().get().sampleRateMode;
        m = rateBox.getSelectedId() == 2 ? "44100" : rateBox.getSelectedId() == 3 ? "48000" : "host";
    };

    genreBox.addItem ("Riddim", 1);
    genreBox.addItem ("Dubstep", 2);
    genreBox.setSelectedId (processor.getConfig().get().defaultGenre == foundry::Genre::Dubstep ? 2 : 1,
                            juce::dontSendNotification);
    genreBox.onChange = [this]
    {
        processor.getConfig().get().defaultGenre = genreBox.getSelectedId() == 2 ? foundry::Genre::Dubstep
                                                                                 : foundry::Genre::Riddim;
    };

    protectBox.addItem ("Peak protection on", 1);
    protectBox.addItem ("Peak protection off", 2);
    protectBox.setSelectedId (processor.getConfig().get().renderProtection ? 1 : 2, juce::dontSendNotification);
    protectBox.onChange = [this]
    {
        processor.getConfig().get().renderProtection = protectBox.getSelectedId() == 1;
    };

    for (int i = 0; i < themeNames().size(); ++i)
        themeBox.addItem (themeNames()[i] + " theme", i + 1);
    themeBox.setSelectedId (juce::jmax (0, themeNames().indexOf (processor.getConfig().get().themeName, true)) + 1,
                            juce::dontSendNotification);
    themeBox.setTooltip ("Colour scheme. Your choice becomes the default for every Foundry you open.");
    themeBox.onChange = [this]
    {
        const auto name = themeNames()[themeBox.getSelectedId() - 1];
        processor.getConfig().get().themeName = name;
        persist();
        if (onThemeChanged)
            onThemeChanged (name);
    };
    addAndMakeVisible (themeBox);

    bpmBox.setInputRestrictions (6, "0123456789.");
    bpmBox.setText (juce::String (processor.getConfig().get().customBpm, 0), juce::dontSendNotification);

    addAndMakeVisible (closeButton);
    addAndMakeVisible (rootButton);
    addAndMakeVisible (createButton);
    addAndMakeVisible (refreshButton);
    addAndMakeVisible (rebuildButton);
    addAndMakeVisible (openDbButton);
    addAndMakeVisible (rootLabel);
    addAndMakeVisible (rateBox);
    addAndMakeVisible (genreBox);
    addAndMakeVisible (protectBox);
    addAndMakeVisible (bpmBox);
    addChildComponent (banner);

    refreshLabels();
}

void SettingsPanel::setTheme (const Theme& t)
{
    theme = t;
    banner.setTheme (t);
    closeButton.setColour (juce::TextButton::buttonColourId, t.accent);
    createButton.setColour (juce::TextButton::buttonColourId, t.accent);
    refreshButton.setColour (juce::TextButton::buttonColourId, t.accent);
    rootButton.setColour (juce::TextButton::buttonColourId, t.accentAlt);
    rebuildButton.setColour (juce::TextButton::buttonColourId, t.accentAlt);
    openDbButton.setColour (juce::TextButton::buttonColourId, t.accent);
    for (auto& b : folderButtons)
        b.setColour (juce::TextButton::buttonColourId, t.accent);
    for (auto& l : folderLabels)
        l.setColour (juce::Label::textColourId, t.text);
    rootLabel.setColour (juce::Label::textColourId, t.text);
    repaint();
}

void SettingsPanel::refreshLabels()
{
    auto root = processor.getConfig().root();
    rootLabel.setText (root.getFullPathName().isEmpty() ? "No root folder selected"
                                                        : root.getFullPathName(),
                       juce::dontSendNotification);

    for (int i = 0; i < foundry::kNumFolderRoles; ++i)
    {
        const auto role = (foundry::FolderRole) i;
        auto f = processor.getConfig().folder (role);
        auto path = f.getFullPathName();
        if (path.isEmpty() && role == foundry::FolderRole::Riffs)
            path = processor.getConfig().riffDir().getFullPathName() + "  (default)";
        folderLabels[i].setText (foundry::folderRoleName (role) + "  -  "
                                     + (path.isEmpty() ? juce::String ("not mapped") : path),
                                 juce::dontSendNotification);
    }
}

void SettingsPanel::persist()
{
    processor.getConfig().get().customBpm = juce::jlimit (40.0, 300.0, bpmBox.getText().getDoubleValue() > 0
                                                                          ? bpmBox.getText().getDoubleValue()
                                                                          : processor.getConfig().get().customBpm);
    juce::String err;
    processor.persistConfig (err);
}

void SettingsPanel::pickRoot()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose Foundry library root",
                                                   processor.getConfig().root(), "*");
    const auto browserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories;
    chooser->launchAsync (browserFlags, [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f.isDirectory())
        {
            processor.getConfig().setRoot (f);
            juce::String err;
            processor.reopenDatabase (err);
            persist();
            refreshLabels();
        }
    });
}

void SettingsPanel::pickFolder (foundry::FolderRole role)
{
    auto start = processor.getConfig().folder (role);
    if (! start.isDirectory())
        start = processor.getConfig().root();

    chooser = std::make_unique<juce::FileChooser> ("Map " + foundry::folderRoleName (role), start, "*");
    const auto browserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories;
    chooser->launchAsync (browserFlags, [this, role] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f.isDirectory())
        {
            processor.getConfig().setFolder (role, f);
            persist();
            refreshLabels();
        }
    });
}

void SettingsPanel::resized()
{
    auto b = getLocalBounds().reduced (20);
    auto top = b.removeFromTop (28);
    closeButton.setBounds (top.removeFromRight (80));

    if (banner.isVisible())
        banner.setBounds (b.removeFromTop (32)), b.removeFromTop (8);

    b.removeFromTop (28);
    auto row = b.removeFromTop (24);
    rootButton.setBounds (row.removeFromLeft (130));
    row.removeFromLeft (8);
    rootLabel.setBounds (row);

    b.removeFromTop (8);
    createButton.setBounds (b.removeFromTop (26).removeFromLeft (280));
    b.removeFromTop (12);

    for (int i = 0; i < foundry::kNumFolderRoles; ++i)
    {
        row = b.removeFromTop (22);
        folderButtons[i].setBounds (row.removeFromRight (70));
        folderLabels[i].setBounds (row);
        b.removeFromTop (4);
    }

    b.removeFromTop (10);
    row = b.removeFromTop (24);
    rateBox.setBounds (row.removeFromLeft (180));
    row.removeFromLeft (10);
    protectBox.setBounds (row.removeFromLeft (180));

    b.removeFromTop (8);
    row = b.removeFromTop (24);
    genreBox.setBounds (row.removeFromLeft (180));
    row.removeFromLeft (10);
    bpmBox.setBounds (row.removeFromLeft (80));
    row.removeFromLeft (10);
    themeBox.setBounds (row.removeFromLeft (180));

    b.removeFromTop (12);
    row = b.removeFromTop (26);
    refreshButton.setBounds (row.removeFromLeft (150));
    row.removeFromLeft (8);
    rebuildButton.setBounds (row.removeFromLeft (170));
    row.removeFromLeft (8);
    openDbButton.setBounds (row.removeFromLeft (180));
}

void SettingsPanel::paint (juce::Graphics& g)
{
    FoundryLookAndFeel::drawPanel (g, getLocalBounds().toFloat().reduced (6.0f), theme);
    g.setFont (FoundryLookAndFeel::titleFont (18.0f, 0.16f));
    g.setColour (theme.text);
    g.drawText ("SETTINGS", 22, 18, 240, 24, juce::Justification::centredLeft);

    g.setFont (FoundryLookAndFeel::labelFont (8.5f, true, 0.14f));
    g.setColour (theme.textDim);
    g.drawText ("LIBRARY ROOT", 22, 48, 200, 12, juce::Justification::centredLeft);
}

} // namespace pt
