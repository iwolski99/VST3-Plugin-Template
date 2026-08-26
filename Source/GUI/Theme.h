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

/** The default theme. Start here, then just change the hex codes - the rest
    of the GUI updates itself because nothing else hard-codes a colour. */
const Theme& getTheme() noexcept;

} // namespace pt
