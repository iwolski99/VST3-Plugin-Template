#include "Theme.h"

namespace pt
{

// Deep indigo / violet, chosen only as a starting point that already looks
// deliberate rather than as a "final" brand colour - see Theme.h for how to
// replace it.
static const Theme theme
{
    juce::Colour (0xff1f1a2e), juce::Colour (0xff0e0c17),   // bgTop, bgBottom
    juce::Colour (0xff272038), juce::Colour (0xff3d3358),   // panel, panelEdge
    juce::Colour (0xff9a7cf2), juce::Colour (0xff5ec4b8),   // accent, accentAlt
    juce::Colour (0xfff2eefc), juce::Colour (0xff938bb0),   // text, textDim
};

const Theme& getTheme() noexcept
{
    return theme;
}

} // namespace pt
