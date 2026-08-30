#include "KnobComponent.h"

namespace pt
{

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state,
            const juce::String& parameterID,
            const juce::String& displayName,
            float knobScale)
    : name (displayName), scale (knobScale)
{
    const auto theme = getTheme();
    accent  = theme.accent;
    textCol = theme.text;
    dimCol  = theme.textDim;
    wellCol = theme.bgBottom;

    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.22f,
                                juce::MathConstants<float>::pi * 2.78f, true);
    slider.setMouseDragSensitivity (170);
    slider.setVelocityBasedMode (false);
    slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    slider.addListener (this);

    if (auto* p = state.getParameter (parameterID))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));

    addAndMakeVisible (slider);

    // The slider covers most of the component, so hover has to be watched on
    // the child as well or the label would never light up.
    slider.addMouseListener (&hoverWatcher, false);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                     (state, parameterID, slider);
}

Knob::~Knob()
{
    slider.removeMouseListener (&hoverWatcher);
    slider.removeListener (this);
}

void Knob::setHot (bool shouldBeHot)
{
    if (hot == shouldBeHot)
        return;

    hot = shouldBeHot;
    repaint();
}

void Knob::setAccent (juce::Colour c)
{
    accent = c;
    slider.setColour (juce::Slider::rotarySliderFillColourId, c);
    repaint();
}

void Knob::setThemeColours (const Theme& t)
{
    textCol = t.text;
    dimCol  = t.textDim;
    wellCol = t.bgBottom;
    setAccent (t.accent);
}

void Knob::resized()
{
    auto b = getLocalBounds();
    const int nameH  = juce::roundToInt (14.0f * scale);
    const int valueH = juce::roundToInt (14.0f * scale);

    b.removeFromTop (nameH);
    b.removeFromBottom (valueH);

    const int d = juce::jmin (b.getWidth(), b.getHeight());
    slider.setBounds (b.withSizeKeepingCentre (d, d));
}

void Knob::paint (juce::Graphics& g)
{
    auto b = getLocalBounds();
    const int nameH  = juce::roundToInt (14.0f * scale);
    const int valueH = juce::roundToInt (15.0f * scale);
    const float enabledAlpha = slider.isEnabled() ? 1.0f : 0.45f;
    const bool live = hot || slider.isMouseOverOrDragging();

    // --- name ---------------------------------------------------------------
    g.setFont (BinauralBeatsLookAndFeel::labelFont (juce::jmax (8.5f, 10.0f * scale), true, 0.085f));
    g.setColour ((live ? dimCol.brighter (0.45f) : dimCol.withAlpha (0.82f))
                     .withMultipliedAlpha (enabledAlpha));
    g.drawFittedText (name.toUpperCase(), b.removeFromTop (nameH),
                      juce::Justification::centred, 1);

    // --- readout ------------------------------------------------------------
    // Slightly larger and lit under the pointer; quiet the rest of the time.
    auto valueArea = b.removeFromBottom (valueH);

    g.setFont (BinauralBeatsLookAndFeel::labelFont (juce::jmax (9.0f, (live ? 11.9f : 11.0f) * scale),
                                                     live, 0.010f));

    // Recessed well behind the digits, sized to the text rather than the full
    // knob width so it reads as a small display rather than a bar under every
    // knob. Fitted to the *live* (larger) font so the well doesn't resize as
    // the text grows on hover - only the digits inside it do.
    {
        const auto valueText = slider.getTextFromValue (slider.getValue());
        const float textW = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), valueText);
        const float wellW = juce::jmin ((float) valueArea.getWidth(), textW + 13.0f * scale);
        const float wellH = juce::jmin ((float) valueArea.getHeight() - 1.0f, 14.0f * scale);
        const auto well = juce::Rectangle<float> (wellW, wellH)
                              .withCentre (valueArea.toFloat().getCentre());
        const float wellCorner = wellH * 0.32f;

        juce::ColourGradient trough (wellCol.darker (0.30f).withAlpha (0.85f * enabledAlpha),
                                     well.getCentreX(), well.getY(),
                                     wellCol.brighter (0.05f).withAlpha (0.70f * enabledAlpha),
                                     well.getCentreX(), well.getBottom(), false);
        g.setGradientFill (trough);
        g.fillRoundedRectangle (well, wellCorner);

        g.setColour (juce::Colours::black.withAlpha (0.35f * enabledAlpha));
        g.drawRoundedRectangle (well.reduced (0.5f), wellCorner, 1.0f);

        // A hairline top highlight is what turns "dark rounded rect" into
        // "recessed", the same trick the panel and mode-switch trough use.
        g.setColour (juce::Colours::white.withAlpha (0.06f * enabledAlpha));
        g.drawLine (well.getX() + wellCorner * 0.6f, well.getY() + 1.0f,
                   well.getRight() - wellCorner * 0.6f, well.getY() + 1.0f, 1.0f);
    }

    if (live)
    {
        g.setColour (accent.withAlpha (0.30f * enabledAlpha));
        g.drawFittedText (slider.getTextFromValue (slider.getValue()),
                          valueArea.translated (0, 1), juce::Justification::centred, 1);
    }

    g.setColour ((live ? accent.brighter (0.30f) : textCol.withAlpha (0.62f))
                     .withMultipliedAlpha (enabledAlpha));
    g.drawFittedText (slider.getTextFromValue (slider.getValue()),
                      valueArea, juce::Justification::centred, 1);
}

//==============================================================================
Combo::Combo (juce::AudioProcessorValueTreeState& state,
              const juce::String& parameterID,
              const juce::String& displayName)
    : name (displayName)
{
    dimCol = getTheme().textDim;

    box.setJustificationType (juce::Justification::centredLeft);

    // The attachment syncs the selection but never fills the list, so the
    // choices have to be pulled off the parameter first.
    if (auto* p = state.getParameter (parameterID))
        box.addItemList (p->getAllValueStrings(), 1);

    addAndMakeVisible (box);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
                     (state, parameterID, box);
}

void Combo::setThemeColours (const Theme& t)
{
    dimCol = t.textDim;
    box.setColour (juce::ComboBox::textColourId, t.text);
    box.setColour (juce::ComboBox::arrowColourId, t.accent);
    box.setColour (juce::ComboBox::outlineColourId, t.panelEdge);
    repaint();
}

void Combo::resized()
{
    auto b = getLocalBounds();
    b.removeFromTop (16);
    box.setBounds (b.removeFromTop (juce::jmin (24, b.getHeight())).reduced (2, 0));
}

void Combo::paint (juce::Graphics& g)
{
    g.setFont (BinauralBeatsLookAndFeel::labelFont (10.0f, true, 0.085f));
    g.setColour (dimCol.withAlpha (box.isEnabled() ? 0.82f : 0.4f));
    g.drawFittedText (name.toUpperCase(), getLocalBounds().removeFromTop (14),
                      juce::Justification::centred, 1);
}

//==============================================================================
Toggle::Toggle (juce::AudioProcessorValueTreeState& state,
                const juce::String& parameterID,
                const juce::String& displayName)
{
    button.setButtonText (displayName.toUpperCase());
    button.setClickingTogglesState (true);
    addAndMakeVisible (button);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>
                     (state, parameterID, button);
}

void Toggle::setAccent (juce::Colour c)
{
    button.setColour (juce::TextButton::buttonColourId, c);
    button.setColour (juce::TextButton::buttonOnColourId, c);
    repaint();
}

void Toggle::resized()
{
    button.setBounds (getLocalBounds());
}

//==============================================================================
ModeSwitch::ModeSwitch (juce::AudioProcessorValueTreeState& state,
                        const juce::String& parameterID,
                        juce::String leftLabel, juce::String rightLabel)
    : ModeSwitch (state, parameterID,
                  juce::StringArray { std::move (leftLabel), std::move (rightLabel) })
{
}

ModeSwitch::ModeSwitch (juce::AudioProcessorValueTreeState& state,
                        const juce::String& parameterID,
                        juce::StringArray segmentLabels)
    : labels (std::move (segmentLabels))
{
    jassert (labels.size() >= 2);
    param = state.getParameter (parameterID);
    jassert (param != nullptr);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);

    if (param != nullptr)
    {
        attachment = std::make_unique<juce::ParameterAttachment> (
            *param, [this] (float v) { value = v; repaint(); }, state.undoManager);
        attachment->sendInitialUpdate();
    }
}

void ModeSwitch::setThemeColours (const Theme& t)
{
    theme = t;
    repaint();
}

int ModeSwitch::segmentAt (float x) const noexcept
{
    const int n = juce::jmax (1, labels.size());
    return juce::jlimit (0, n - 1, (int) (x / juce::jmax (1.0f, (float) getWidth() / (float) n)));
}

void ModeSwitch::mouseMove (const juce::MouseEvent& e)
{
    const int seg = segmentAt (e.position.x);
    if (seg != hovered)
    {
        hovered = seg;
        repaint();
    }
}

void ModeSwitch::mouseExit (const juce::MouseEvent&)
{
    if (hovered != -1)
    {
        hovered = -1;
        repaint();
    }
}

void ModeSwitch::mouseUp (const juce::MouseEvent& e)
{
    if (attachment == nullptr)
        return;

    attachment->setValueAsCompleteGesture ((float) segmentAt (e.position.x));
}

void ModeSwitch::paint (juce::Graphics& g)
{
    const auto full = getLocalBounds().toFloat().reduced (0.5f);
    const float r = full.getHeight() * 0.5f;

    // Recessed trough, so the lit segment reads as sitting proud of it.
    juce::ColourGradient trough (theme.bgBottom.darker (0.25f).withAlpha (0.88f),
                                 full.getCentreX(), full.getY(),
                                 theme.bgBottom.brighter (0.06f).withAlpha (0.82f),
                                 full.getCentreX(), full.getBottom(), false);
    g.setGradientFill (trough);
    g.fillRoundedRectangle (full, r);

    g.setColour (theme.panelEdge.withAlpha (0.75f));
    g.drawRoundedRectangle (full, r, 1.0f);

    const int n = juce::jmax (1, labels.size());
    const int selected = getSelectedIndex();
    const float segW = full.getWidth() / (float) n;

    const auto activeSeg = full.withWidth (segW).withX (full.getX() + segW * (float) selected)
                               .reduced (2.0f);

    BinauralBeatsLookAndFeel::drawActivePill (g, activeSeg, juce::jmax (1.0f, r - 2.0f),
                                               theme.accent, hovered == selected ? 1.0f : 0.94f);

    g.setFont (BinauralBeatsLookAndFeel::labelFont (n > 2 ? 8.5f : 9.6f, true, 0.075f));

    for (int i = 0; i < n; ++i)
    {
        const auto area = full.withWidth (segW).withX (full.getX() + segW * (float) i);

        g.setColour (i == selected ? BinauralBeatsLookAndFeel::pillTextColour (theme.accent)
                                   : theme.textDim.withAlpha (hovered == i ? 0.95f : 0.62f));

        g.drawText (labels[i].toUpperCase(), area, juce::Justification::centred);
    }
}

//==============================================================================
ValueField::ValueField (juce::AudioProcessorValueTreeState& state,
                        const juce::String& parameterID,
                        const juce::String& displayName,
                        juce::String suffixText,
                        int decimalPlaces)
    : name (displayName), suffix (std::move (suffixText)), decimals (decimalPlaces)
{
    param = state.getParameter (parameterID);
    jassert (param != nullptr);

    editor.setJustification (juce::Justification::centred);
    editor.setIndents (2, 0);
    editor.setBorder (juce::BorderSize<int> (0));
    editor.setPopupMenuEnabled (false);
    editor.setInputRestrictions (16, "0123456789.:-");
    editor.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    editor.setColour (juce::TextEditor::outlineColourId,     juce::Colours::transparentBlack);
    editor.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    editor.setColour (juce::TextEditor::highlightColourId, theme.accent.withAlpha (0.35f));
    editor.setSelectAllWhenFocused (true);
    editor.setFont (BinauralBeatsLookAndFeel::labelFont (13.0f, true, 0.01f));
    editor.addListener (this);
    addAndMakeVisible (editor);

    if (param != nullptr)
    {
        attachment = std::make_unique<juce::ParameterAttachment> (
            *param,
            [this] (float v)
            {
                if (! editor.hasKeyboardFocus (true) && ! dragging)
                    refreshText (v);
            },
            state.undoManager);
        attachment->sendInitialUpdate();
    }
}

ValueField::~ValueField()
{
    editor.removeListener (this);
}

void ValueField::setThemeColours (const Theme& t)
{
    theme = t;
    editor.setColour (juce::TextEditor::textColourId, t.text);
    editor.setColour (juce::TextEditor::highlightedTextColourId, t.text);
    editor.setColour (juce::TextEditor::highlightColourId, t.accent.withAlpha (0.35f));
    editor.applyFontToAllText (BinauralBeatsLookAndFeel::labelFont (13.0f, true, 0.01f));
    repaint();
}

void ValueField::setCaption (const juce::String& text)
{
    const bool layoutChanged = caption.isEmpty() != text.isEmpty();
    caption = text;
    if (layoutChanged)
        resized();
    repaint();
}

void ValueField::resized()
{
    auto b = getLocalBounds();
    b.removeFromTop (16);
    if (caption.isNotEmpty())
        b.removeFromBottom (14);
    editor.setBounds (b.reduced (6, 4));
}

void ValueField::paint (juce::Graphics& g)
{
    auto b = getLocalBounds();

    g.setFont (BinauralBeatsLookAndFeel::labelFont (10.0f, true, 0.085f));
    g.setColour (theme.textDim.withAlpha (isEnabled() ? 0.82f : 0.4f));
    g.drawFittedText (name.toUpperCase(), b.removeFromTop (14),
                      juce::Justification::centred, 1);

    if (caption.isNotEmpty())
    {
        auto cap = b.removeFromBottom (14);
        g.setFont (BinauralBeatsLookAndFeel::labelFont (9.0f, false, 0.04f));
        g.setColour (theme.accent.withAlpha (0.85f));
        g.drawFittedText (caption, cap, juce::Justification::centred, 1);
    }

    auto well = b.toFloat().reduced (2.0f);
    const float r = ui::controlCorner;
    const float dim = isEnabled() ? 1.0f : 0.45f;
    const bool focused = editor.hasKeyboardFocus (true);

    juce::ColourGradient trough (theme.bgBottom.darker (0.30f).withAlpha (0.88f * dim),
                                 well.getCentreX(), well.getY(),
                                 theme.bgBottom.brighter (0.05f).withAlpha (0.78f * dim),
                                 well.getCentreX(), well.getBottom(), false);
    g.setGradientFill (trough);
    g.fillRoundedRectangle (well, r);

    g.setColour ((focused ? theme.accent : theme.panelEdge.withAlpha (0.8f))
                     .withMultipliedAlpha (dim));
    g.drawRoundedRectangle (well, r, 1.0f);

    g.setColour (juce::Colours::white.withAlpha (0.07f * dim));
    g.drawLine (well.getX() + r, well.getY() + 1.0f,
                well.getRight() - r, well.getY() + 1.0f, 1.0f);
}

void ValueField::refreshText (float denormalised)
{
    auto text = juce::String (denormalised, decimals);
    if (suffix.isNotEmpty() && ! editor.hasKeyboardFocus (true))
        text += " " + suffix;
    editor.setText (text, juce::dontSendNotification);
}

float ValueField::parseTypedValue (const juce::String& text) const
{
    auto trimmed = text.trim();
    if (suffix.isNotEmpty() && trimmed.endsWithIgnoreCase (suffix))
        trimmed = trimmed.dropLastCharacters (suffix.length()).trim();

    // Duration fields accept mm:ss as well as a raw second count.
    if (trimmed.containsChar (':'))
    {
        const auto minutes = trimmed.upToFirstOccurrenceOf (":", false, false).getFloatValue();
        const auto seconds = trimmed.fromFirstOccurrenceOf (":", false, false).getFloatValue();
        return minutes * 60.0f + seconds;
    }

    return trimmed.getFloatValue();
}

float ValueField::nudgeAmount() const
{
    if (decimals >= 2) return 0.01f;
    if (name.containsIgnoreCase ("fade") || name.containsIgnoreCase ("duration"))
        return 0.5f;
    return 1.0f;
}

void ValueField::commitEditor()
{
    if (attachment == nullptr || param == nullptr)
        return;

    const float typed = parseTypedValue (editor.getText());
    const float clamped = param->convertFrom0to1 (
        param->convertTo0to1 (typed));
    attachment->setValueAsCompleteGesture (clamped);
    refreshText (clamped);
}

void ValueField::textEditorReturnKeyPressed (juce::TextEditor&)
{
    commitEditor();
    editor.giveAwayKeyboardFocus();
}

void ValueField::textEditorFocusLost (juce::TextEditor&)
{
    commitEditor();
}

void ValueField::textEditorEscapeKeyPressed (juce::TextEditor&)
{
    if (param != nullptr)
        refreshText (param->convertFrom0to1 (param->getValue()));
    editor.giveAwayKeyboardFocus();
}

void ValueField::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (attachment == nullptr || param == nullptr || ! isEnabled())
        return;

    const float delta = (wheel.deltaY > 0.0f ? 1.0f : -1.0f) * nudgeAmount()
                        * (wheel.isReversed ? -1.0f : 1.0f);
    const float current = param->convertFrom0to1 (param->getValue());
    attachment->setValueAsCompleteGesture (current + delta);
}

void ValueField::mouseDrag (const juce::MouseEvent& e)
{
    if (attachment == nullptr || param == nullptr || ! isEnabled())
        return;

    if (! dragging)
    {
        dragging = true;
        dragStartValue = param->convertFrom0to1 (param->getValue());
    }

    const float current = dragStartValue - (float) e.getDistanceFromDragStartY() * nudgeAmount() * 0.15f;
    attachment->setValueAsCompleteGesture (current);
}

void ValueField::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
}

} // namespace pt
