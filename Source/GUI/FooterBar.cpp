#include "FooterBar.h"

namespace pt
{

//==============================================================================
PagerArrow::PagerArrow (bool pointsRight)
    : juce::Button ({}), right (pointsRight),
      accentCol (getTheme().accent), dimCol (getTheme().textDim)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void PagerArrow::setColours (juce::Colour accent, juce::Colour dim)
{
    accentCol = accent;
    dimCol = dim;
    repaint();
}

void PagerArrow::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto b = getLocalBounds().toFloat().reduced (2.0f);
    const auto c = highlighted ? accentCol : dimCol.withAlpha (0.75f);

    if (highlighted)
    {
        g.setColour (accentCol.withAlpha (0.12f));
        g.fillEllipse (b);
    }

    const float cx = b.getCentreX() + (down ? (right ? 1.0f : -1.0f) : 0.0f);
    const float cy = b.getCentreY();
    const float s  = b.getWidth() * 0.22f;
    const float d  = right ? 1.0f : -1.0f;

    juce::Path p;
    p.startNewSubPath (cx - d * s * 0.6f, cy - s);
    p.lineTo (cx + d * s * 0.6f, cy);
    p.lineTo (cx - d * s * 0.6f, cy + s);

    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));
}

//==============================================================================
const juce::Array<int>& FooterBar::zoomOptions()
{
    static const juce::Array<int> options { 50, 60, 70, 75, 80, 90, 100, 110, 125, 150, 175, 200 };
    return options;
}

FooterBar::FooterBar (juce::AudioProcessorValueTreeState& state, PresetManager& presetManager)
    : apvts (state), presets (presetManager),
      mixKnob    (state, pid::mix,    "Mix",    0.82f),
      outputKnob (state, pid::output, "Output", 0.82f),
      theme (getTheme())
{
    addAndMakeVisible (prevPreset);
    addAndMakeVisible (nextPreset);
    addAndMakeVisible (presetButton);
    addAndMakeVisible (saveButton);
    addAndMakeVisible (slotA);
    addAndMakeVisible (slotB);
    addAndMakeVisible (mixKnob);
    addAndMakeVisible (outputKnob);
    addAndMakeVisible (zoomBox);

    // Opens the full-window browser rather than a dropdown - see
    // PresetBrowser. A single-column dropdown is the thing this replaces.
    presetButton.onClick = [this] { if (onBrowsePresets) onBrowsePresets(); };

    prevPreset.onClick = [this] { presets.step (-1); };
    nextPreset.onClick = [this] { presets.step (+1); };

    saveButton.setComponentID ("saveButton");
    presetButton.setComponentID ("presetButton");
    saveButton.setTooltip ("Save the current settings as a user preset");
    saveButton.onClick = [this] { if (onRequestSave) onRequestSave(); };

    slotA.setClickingTogglesState (false);
    slotB.setClickingTogglesState (false);
    slotA.setTooltip ("A/B compare - click to switch, double-click to copy the current sound across");
    slotB.setTooltip (slotA.getTooltip());
    slotA.onClick = [this] { presets.setActiveSlot (0); updateSlotButtons(); };
    slotB.onClick = [this] { presets.setActiveSlot (1); updateSlotButtons(); };

    for (int i = 0; i < zoomOptions().size(); ++i)
        zoomBox.addItem (juce::String (zoomOptions()[i]) + " %", i + 1);

    zoomBox.setTooltip ("Interface scale - the plugin remembers your choice");
    zoomBox.onChange = [this]
    {
        const int idx = zoomBox.getSelectedId() - 1;
        if (idx >= 0 && idx < zoomOptions().size() && onZoomChanged)
            onZoomChanged (zoomOptions()[idx]);
    };

    presets.addChangeListener (this);
    refreshPresetList();
    updateSlotButtons();
}

FooterBar::~FooterBar()
{
    presets.removeChangeListener (this);
}

//==============================================================================
void FooterBar::setTheme (const Theme& t)
{
    theme = t;

    prevPreset.setColours (t.accent, t.textDim);
    nextPreset.setColours (t.accent, t.textDim);

    mixKnob.setThemeColours (t);
    outputKnob.setThemeColours (t);

    for (auto* b : { &saveButton, &slotA, &slotB })
    {
        b->setColour (juce::TextButton::buttonColourId, t.accent);
        b->setColour (juce::TextButton::buttonOnColourId, t.accent);
    }

    presetButton.setColour (juce::TextButton::buttonColourId, t.bgBottom.brighter (0.10f));
    presetButton.setColour (juce::TextButton::textColourOffId, t.text);
    zoomBox.setColour (juce::ComboBox::textColourId, t.text);
    zoomBox.setColour (juce::ComboBox::arrowColourId, t.accent);
    zoomBox.setColour (juce::ComboBox::outlineColourId, t.panelEdge);

    repaint();
}

void FooterBar::setZoomPercent (int percent)
{
    const int idx = zoomOptions().indexOf (percent);
    zoomBox.setSelectedId (idx >= 0 ? idx + 1 : zoomOptions().indexOf (100) + 1,
                           juce::dontSendNotification);
}

//==============================================================================
void FooterBar::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshPresetList();
    updateSlotButtons();
}

void FooterBar::refreshPresetList()
{
    const juce::ScopedValueSetter<bool> guard (updatingList, true);

    auto name = presets.getCurrentName();

    if (presets.isModified())
        name += " *";

    presetButton.setButtonText (name);
}

void FooterBar::updateSlotButtons()
{
    const bool aActive = presets.getActiveSlot() == 0;
    slotA.setToggleState (aActive, juce::dontSendNotification);
    slotB.setToggleState (! aActive, juce::dontSendNotification);
}

//==============================================================================
void FooterBar::resized()
{
    auto b = getLocalBounds().reduced (14, 6);

    // --- preset browser -------------------------------------------------------
    auto presetArea = b.removeFromLeft (330);
    presetArea.removeFromTop (12);
    auto presetRow = presetArea.removeFromTop (26);

    prevPreset.setBounds (presetRow.removeFromLeft (24).reduced (0, 1));
    presetRow.removeFromLeft (3);
    presetButton.setBounds (presetRow.removeFromLeft (168));
    presetRow.removeFromLeft (3);
    nextPreset.setBounds (presetRow.removeFromLeft (24).reduced (0, 1));
    presetRow.removeFromLeft (6);
    saveButton.setBounds (presetRow.removeFromLeft (44));
    presetRow.removeFromLeft (6);
    slotA.setBounds (presetRow.removeFromLeft (26));
    presetRow.removeFromLeft (2);
    slotB.setBounds (presetRow.removeFromLeft (26));

    constexpr int kRowTop    = 14;
    constexpr int kRowHeight = 24;

    // --- zoom (far right) -----------------------------------------------------
    auto zoomArea = b.removeFromRight (92);
    zoomArea.removeFromTop (kRowTop);
    zoomBox.setBounds (zoomArea.removeFromTop (kRowHeight));

    b.removeFromRight (10);

    // --- global knobs -----------------------------------------------------------
    auto knobs = b.reduced (12, 0);
    const int kw = juce::jmin (72, knobs.getWidth() / 2);
    auto centred = knobs.withSizeKeepingCentre (kw * 2, knobs.getHeight());

    mixKnob   .setBounds (centred.removeFromLeft (kw));
    outputKnob.setBounds (centred.removeFromLeft (kw));
}

void FooterBar::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();

    juce::ColourGradient bar (theme.bgTop.darker (0.22f), 0.0f, 0.0f,
                              theme.bgBottom.darker (0.20f), 0.0f, b.getBottom(), false);
    bar.addColour (0.35, theme.bgTop.darker (0.34f));
    g.setGradientFill (bar);
    g.fillRect (b);

    // Shadow cast upward onto the content, mirroring the header's downward one.
    juce::ColourGradient cast (juce::Colours::black.withAlpha (0.24f), 0.0f, 0.0f,
                               juce::Colours::transparentBlack, 0.0f, 9.0f, false);
    g.setGradientFill (cast);
    g.fillRect (0.0f, 0.0f, b.getWidth(), 9.0f);

    juce::ColourGradient spill (theme.accent.withAlpha (0.12f), 0.0f, 0.0f,
                                juce::Colours::transparentBlack, 0.0f, 13.0f, false);
    g.setGradientFill (spill);
    g.fillRect (0.0f, 0.0f, b.getWidth(), 13.0f);

    g.setColour (theme.accent.withAlpha (0.45f));
    g.fillRect (0.0f, 0.0f, b.getWidth(), 1.0f);

    g.setFont (PluginTemplateLookAndFeel::labelFont (8.5f, true, 0.13f));
    g.setColour (theme.textDim.withAlpha (0.72f));
    g.drawText ("PRESET", 16, 8, 90, 12, juce::Justification::centredLeft);
    g.drawText ("ZOOM", getWidth() - 92, 8, 80, 12, juce::Justification::centredLeft);
}

} // namespace pt
