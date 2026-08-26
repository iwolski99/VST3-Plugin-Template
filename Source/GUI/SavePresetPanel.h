#pragma once

#include "LookAndFeel.h"

namespace pt
{

//==============================================================================
/**
    Preset-naming prompt drawn inside the editor rather than as a separate OS
    window. Plugin hosts handle child components far more predictably than
    top-level windows, and this way the prompt inherits the plugin's zoom and
    colour theme instead of floating at desktop scale.
*/
class SavePresetPanel : public juce::Component
{
public:
    SavePresetPanel();

    std::function<void (const juce::String& name, const juce::String& category)> onSave;

    void setTheme (const Theme& t);
    void show (const juce::String& initialName);
    void dismiss();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    juce::Rectangle<int> panelBounds() const;
    void commit();

    juce::TextEditor nameEditor;
    juce::ComboBox   categoryBox;
    juce::TextButton saveButton   { "SAVE" };
    juce::TextButton cancelButton { "CANCEL" };

    Theme theme { getTheme() };
};

} // namespace pt
