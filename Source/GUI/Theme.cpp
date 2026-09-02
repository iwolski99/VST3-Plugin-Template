#include "Theme.h"

namespace pt
{

// Forged steel and molten metal. Warm charcoal ground, ember gold as the
// working accent, copper as the secondary heat. Every other colour in the
// plugin is derived from these eight.
static const Theme theme
{
    juce::Colour (0xff161310), juce::Colour (0xff090807),   // bgTop, bgBottom
    juce::Colour (0xff221c16), juce::Colour (0xff4a3c2e),   // panel, panelEdge
    juce::Colour (0xffe2a045), juce::Colour (0xffe25a2a),   // accent, accentAlt
    juce::Colour (0xfff4eadc), juce::Colour (0xff9a8b78),   // text, textDim
};

const Theme& getTheme() noexcept
{
    return theme;
}

} // namespace pt
