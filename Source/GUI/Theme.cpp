#include "Theme.h"

namespace pt
{

// Midnight teal: dark enough for a listening tool, with a calm cyan accent
// so frequency readouts and the PLAY pill read as "lit" rather than loud.
static const Theme theme
{
    juce::Colour (0xff12202a), juce::Colour (0xff070d12),   // bgTop, bgBottom
    juce::Colour (0xff17303c), juce::Colour (0xff2c5564),   // panel, panelEdge
    juce::Colour (0xff4ecdc4), juce::Colour (0xff7aa2f7),   // accent, accentAlt
    juce::Colour (0xffe7f4f4), juce::Colour (0xff7d9aa8),   // text, textDim
};

const Theme& getTheme() noexcept
{
    return theme;
}

} // namespace pt
