#pragma once

#include "LookAndFeel.h"

namespace pt
{

class SectionLabel : public juce::Component
{
public:
    explicit SectionLabel (juce::String t) : text (std::move (t)) {}
    void setTheme (const Theme& t) { theme = t; repaint(); }
    void paint (juce::Graphics& g) override
    {
        g.setFont (FoundryLookAndFeel::labelFont (9.0f, true, 0.16f));
        g.setColour (theme.textDim);
        g.drawText (text, getLocalBounds(), juce::Justification::centredLeft, true);
    }
private:
    juce::String text;
    Theme theme { getTheme() };
};

class Stepper : public juce::Component
{
public:
    std::function<void (int)> onChanged;

    Stepper (juce::String label, int minV, int maxV, int initial)
        : name (std::move (label)), minValue (minV), maxValue (maxV), value (initial)
    {
        minus.setButtonText ("-");
        plus.setButtonText ("+");
        minus.onClick = [this] { setValue (value - 1, true); };
        plus.onClick  = [this] { setValue (value + 1, true); };
        addAndMakeVisible (minus);
        addAndMakeVisible (plus);
    }

    void setValue (int v, bool notify)
    {
        v = juce::jlimit (minValue, maxValue, v);
        if (v == value)
            return;
        value = v;
        repaint();
        if (notify && onChanged)
            onChanged (value);
    }

    int getValue() const noexcept { return value; }

    void setTheme (const Theme& t)
    {
        theme = t;
        minus.setColour (juce::TextButton::buttonColourId, t.accent);
        plus.setColour (juce::TextButton::buttonColourId, t.accent);
        repaint();
    }

    void resized() override
    {
        auto b = getLocalBounds();
        b.removeFromTop (14);
        minus.setBounds (b.removeFromLeft (26));
        plus.setBounds (b.removeFromRight (26));
    }

    void paint (juce::Graphics& g) override
    {
        g.setFont (FoundryLookAndFeel::labelFont (9.0f, true, 0.12f));
        g.setColour (theme.textDim);
        g.drawText (name, 0, 0, getWidth(), 14, juce::Justification::centredLeft);

        auto mid = juce::Rectangle<int> (26, 14, getWidth() - 52, getHeight() - 14);
        g.setColour (theme.text);
        g.setFont (FoundryLookAndFeel::labelFont (13.0f, true));
        g.drawText (juce::String (value), mid, juce::Justification::centred);
    }

private:
    juce::String name;
    int minValue, maxValue, value;
    juce::TextButton minus, plus;
    Theme theme { getTheme() };
};

class StatusBanner : public juce::Component
{
public:
    std::function<void()> onOpenSettings;

    void setMessage (juce::String m, bool error, bool settings)
    {
        text = std::move (m);
        isError = error;
        showSettings = settings;
        settingsButton.setVisible (showSettings && text.isNotEmpty());
        setVisible (text.isNotEmpty());
        repaint();
    }

    StatusBanner()
    {
        settingsButton.setButtonText ("OPEN SETTINGS");
        settingsButton.onClick = [this] { if (onOpenSettings) onOpenSettings(); };
        addChildComponent (settingsButton);
        setVisible (false);
    }

    void setTheme (const Theme& t)
    {
        theme = t;
        settingsButton.setColour (juce::TextButton::buttonColourId, t.accent);
        repaint();
    }

    void resized() override
    {
        settingsButton.setBounds (getWidth() - 132, 6, 120, getHeight() - 12);
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        const auto fill = isError ? theme.accentAlt : theme.accent;
        g.setColour (fill.withAlpha (0.16f));
        g.fillRoundedRectangle (b, ui::controlCorner);
        g.setColour (fill.withAlpha (0.70f));
        g.drawRoundedRectangle (b.reduced (0.5f), ui::controlCorner, 1.0f);
        g.setColour (theme.text);
        g.setFont (FoundryLookAndFeel::labelFont (12.0f, false));
        g.drawFittedText (text, getLocalBounds().reduced (12, 6).withTrimmedRight (showSettings ? 130 : 0),
                          juce::Justification::centredLeft, 2);
    }

private:
    juce::String text;
    bool isError = false;
    bool showSettings = false;
    juce::TextButton settingsButton;
    Theme theme { getTheme() };
};

} // namespace pt
