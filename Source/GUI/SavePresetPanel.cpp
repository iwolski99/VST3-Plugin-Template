#include "SavePresetPanel.h"
#include "../Presets/FactoryPresets.h"

namespace pt
{

static constexpr int kPanelWidth  = 340;
static constexpr int kPanelHeight = 176;

SavePresetPanel::SavePresetPanel()
{
    setComponentID ("savePanel");
    setWantsKeyboardFocus (true);
    setVisible (false);

    nameEditor.setJustification (juce::Justification::centredLeft);
    nameEditor.setFont (BinauralBeatsLookAndFeel::labelFont (14.0f));
    nameEditor.setSelectAllWhenFocused (true);
    nameEditor.setEscapeAndReturnKeysConsumed (true);
    nameEditor.onReturnKey = [this] { commit(); };
    nameEditor.onEscapeKey = [this] { dismiss(); };
    addAndMakeVisible (nameEditor);

    // Filed straight into a folder at save time. Without this every saved
    // preset lands in "User" and the browser's folders only ever organise the
    // factory bank, which is the half that needed it least.
    categoryBox.addItemList (presetCategories(), 1);
    categoryBox.setSelectedId (presetCategories().indexOf ("User") + 1,
                               juce::dontSendNotification);
    addAndMakeVisible (categoryBox);

    saveButton.onClick   = [this] { commit(); };
    cancelButton.onClick = [this] { dismiss(); };
    addAndMakeVisible (saveButton);
    addAndMakeVisible (cancelButton);
}

void SavePresetPanel::setTheme (const Theme& t)
{
    theme = t;

    nameEditor.setColour (juce::TextEditor::backgroundColourId, t.bgBottom.darker (0.2f));
    nameEditor.setColour (juce::TextEditor::textColourId, t.text);
    nameEditor.setColour (juce::TextEditor::outlineColourId, t.panelEdge);
    nameEditor.setColour (juce::TextEditor::focusedOutlineColourId, t.accent);
    nameEditor.setColour (juce::TextEditor::highlightColourId, t.accent.withAlpha (0.35f));
    nameEditor.setColour (juce::CaretComponent::caretColourId, t.accent);

    categoryBox.setColour (juce::ComboBox::backgroundColourId, t.bgBottom.darker (0.2f));
    categoryBox.setColour (juce::ComboBox::textColourId, t.text);
    categoryBox.setColour (juce::ComboBox::outlineColourId, t.panelEdge);
    categoryBox.setColour (juce::ComboBox::arrowColourId, t.accent);

    for (auto* b : { &saveButton, &cancelButton })
    {
        b->setColour (juce::TextButton::buttonColourId, t.accent);
        b->setColour (juce::TextButton::buttonOnColourId, t.accent);
    }

    repaint();
}

void SavePresetPanel::show (const juce::String& initialName)
{
    nameEditor.setText (initialName, false);
    setVisible (true);
    toFront (true);
    nameEditor.grabKeyboardFocus();
    nameEditor.selectAll();
    repaint();
}

void SavePresetPanel::dismiss()
{
    setVisible (false);
}

void SavePresetPanel::commit()
{
    const auto name = nameEditor.getText().trim();

    if (name.isNotEmpty() && onSave != nullptr)
        onSave (name, categoryBox.getText());

    dismiss();
}

//==============================================================================
juce::Rectangle<int> SavePresetPanel::panelBounds() const
{
    return getLocalBounds().withSizeKeepingCentre (kPanelWidth, kPanelHeight);
}

void SavePresetPanel::resized()
{
    auto p = panelBounds().reduced (18, 16);
    p.removeFromTop (34);                       // heading

    nameEditor.setBounds (p.removeFromTop (28));
    p.removeFromTop (10);
    p.removeFromTop (14);                       // "FOLDER" caption
    categoryBox.setBounds (p.removeFromTop (26));
    p.removeFromTop (12);

    auto row = p.removeFromTop (26);
    cancelButton.setBounds (row.removeFromRight (74));
    row.removeFromRight (8);
    saveButton.setBounds (row.removeFromRight (74));
}

void SavePresetPanel::mouseDown (const juce::MouseEvent& e)
{
    if (! panelBounds().contains (e.getPosition()))
        dismiss();
}

bool SavePresetPanel::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        dismiss();
        return true;
    }

    if (key == juce::KeyPress::returnKey)
    {
        commit();
        return true;
    }

    return false;
}

void SavePresetPanel::paint (juce::Graphics& g)
{
    g.fillAll (theme.bgBottom.withAlpha (0.72f));

    const auto p = panelBounds().toFloat();

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (p.translated (0.0f, 4.0f), 10.0f);

    g.setColour (theme.bgTop);
    g.fillRoundedRectangle (p, 10.0f);
    g.setColour (theme.panel.withAlpha (0.7f));
    g.fillRoundedRectangle (p, 10.0f);

    g.setColour (theme.accent.withAlpha (0.75f));
    g.drawRoundedRectangle (p.reduced (0.5f), 10.0f, 1.2f);

    g.setFont (BinauralBeatsLookAndFeel::labelFont (12.0f, true));
    g.setColour (theme.accent);
    g.drawText ("SAVE PRESET", p.reduced (18.0f, 14.0f).removeFromTop (16.0f),
                juce::Justification::topLeft);

    g.setFont (BinauralBeatsLookAndFeel::labelFont (10.5f));
    g.setColour (theme.textDim);
    g.drawText ("Documents / BinauralBeats / Presets",
                p.reduced (18.0f, 14.0f).removeFromTop (16.0f),
                juce::Justification::topRight);

    g.setFont (BinauralBeatsLookAndFeel::labelFont (8.5f, true, 0.12f));
    g.setColour (theme.textDim.withAlpha (0.8f));
    g.drawText ("FOLDER", (int) p.getX() + 18, (int) p.getY() + 82, 120, 12,
                juce::Justification::centredLeft);
}

} // namespace pt
