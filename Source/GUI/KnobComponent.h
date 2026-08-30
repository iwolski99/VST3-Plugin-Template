#pragma once

#include "LookAndFeel.h"

namespace pt
{

//==============================================================================
/** Rotary control with its name above and the formatted value below.

    The label and readout rest at reduced opacity and come up to full
    brightness under the pointer, so a page reads as a calm field of controls
    until you reach for one - rather than as a dozen equally-loud numbers
    competing for attention. */
class Knob : public juce::Component,
             private juce::Slider::Listener
{
public:
    Knob (juce::AudioProcessorValueTreeState& state,
          const juce::String& parameterID,
          const juce::String& displayName,
          float knobScale = 1.0f);

    ~Knob() override;

    void setAccent (juce::Colour c);
    void setThemeColours (const Theme& t);

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseEnter (const juce::MouseEvent&) override { setHot (true); }
    void mouseExit  (const juce::MouseEvent&) override { setHot (false); }

    juce::Slider& getSlider() noexcept { return slider; }

private:
    void sliderValueChanged (juce::Slider*) override { repaint(); }
    void setHot (bool shouldBeHot);

    struct HoverWatcher : juce::MouseListener
    {
        explicit HoverWatcher (Knob& o) : owner (o) {}
        void mouseEnter (const juce::MouseEvent&) override { owner.setHot (true); }
        void mouseExit  (const juce::MouseEvent&) override { owner.setHot (false); }
        Knob& owner;
    };

    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    HoverWatcher hoverWatcher { *this };

    juce::String name;
    juce::Colour accent, textCol, dimCol, wellCol;
    float scale;
    bool  hot = false;
};

//==============================================================================
/** Labelled drop-down bound to a choice parameter. */
class Combo : public juce::Component
{
public:
    Combo (juce::AudioProcessorValueTreeState& state,
           const juce::String& parameterID,
           const juce::String& displayName);

    void setThemeColours (const Theme& t);
    void paint (juce::Graphics&) override;
    void resized() override;

    juce::ComboBox& getBox() noexcept { return box; }

private:
    juce::ComboBox box;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    juce::String name;
    juce::Colour dimCol;
};

//==============================================================================
/** Latching button bound to a bool parameter. */
class Toggle : public juce::Component
{
public:
    Toggle (juce::AudioProcessorValueTreeState& state,
            const juce::String& parameterID,
            const juce::String& displayName);

    void setAccent (juce::Colour c);
    void resized() override;

    juce::TextButton& getButton() noexcept { return button; }

private:
    juce::TextButton button;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

//==============================================================================
/** Segmented pill switch bound to a choice parameter. */
class ModeSwitch : public juce::Component,
                   public juce::SettableTooltipClient
{
public:
    ModeSwitch (juce::AudioProcessorValueTreeState& state,
                const juce::String& parameterID,
                juce::StringArray segmentLabels);

    /** Two-segment convenience overload. */
    ModeSwitch (juce::AudioProcessorValueTreeState& state,
                const juce::String& parameterID,
                juce::String leftLabel, juce::String rightLabel);

    void setThemeColours (const Theme& t);

    int getSelectedIndex() const noexcept
    {
        return juce::jlimit (0, labels.size() - 1, juce::roundToInt (value));
    }

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    int segmentAt (float x) const noexcept;

    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    juce::StringArray labels;
    float value = 0.0f;
    int hovered = -1;
    Theme theme { getTheme() };
};

//==============================================================================
/** Labelled numeric field bound to a float parameter. Type a value, scroll
    the wheel, or drag the digits - built for frequencies and durations where
    a knob is the wrong resolution. */
class ValueField : public juce::Component,
                   public juce::SettableTooltipClient,
                   private juce::TextEditor::Listener
{
public:
    ValueField (juce::AudioProcessorValueTreeState& state,
                const juce::String& parameterID,
                const juce::String& displayName,
                juce::String suffix = {},
                int decimalPlaces = 1);

    ~ValueField() override;

    void setThemeColours (const Theme& t);
    void setCaption (const juce::String& text);   // optional line under the box

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    juce::TextEditor& getEditor() noexcept { return editor; }

private:
    void textEditorReturnKeyPressed (juce::TextEditor&) override;
    void textEditorFocusLost (juce::TextEditor&) override;
    void textEditorEscapeKeyPressed (juce::TextEditor&) override;

    void commitEditor();
    void refreshText (float denormalised);
    float parseTypedValue (const juce::String& text) const;
    float nudgeAmount() const;

    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    juce::TextEditor editor;
    juce::String name, suffix, caption;
    int decimals = 1;
    Theme theme { getTheme() };
    bool dragging = false;
    float dragStartValue = 0.0f;
};

} // namespace pt
