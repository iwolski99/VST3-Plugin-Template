#pragma once

#include "../Params/ParamIDs.h"

namespace pt
{

//==============================================================================
/** The plugin's whole colour language in one place. Every panel, knob and
    button derives its colour from one of these eight, rather than each
    component picking its own - which is the difference between a UI that
    reads as one designed thing and one that reads as a pile of controls.

    Swap the hex values below to reskin the whole plugin. Nothing else needs
    to change: the look-and-feel and every GUI component only ever reads
    through a Theme, never a literal colour. */
struct Theme
{
    juce::Colour bgTop, bgBottom;
    juce::Colour panel, panelEdge;
    juce::Colour accent, accentAlt;
    juce::Colour text, textDim;
};

/** The theme currently in use. New components start from this; open
    editors are re-skinned through their setTheme() chain. */
const Theme& getTheme() noexcept;

/** The built-in colour schemes, in menu order: Orange (the original forged
    steel and molten metal), Purple, Green, Blue and Red. */
const juce::StringArray& themeNames();
const Theme& themeByName (const juce::String& name) noexcept;

/** Makes `name` the current theme (unknown names fall back to Orange). */
void setCurrentTheme (const juce::String& name);
juce::String currentThemeName();

} // namespace pt
