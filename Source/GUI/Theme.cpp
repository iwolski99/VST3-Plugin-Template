#include "Theme.h"

namespace pt
{

namespace
{
    // Every colour in the plugin is derived from these eight per theme:
    // bgTop, bgBottom / panel, panelEdge / accent, accentAlt / text, textDim.
    const Theme kOrange
    {
        // Forged steel and molten metal: warm charcoal, ember gold, copper heat.
        juce::Colour (0xff161310), juce::Colour (0xff090807),
        juce::Colour (0xff221c16), juce::Colour (0xff4a3c2e),
        juce::Colour (0xffe2a045), juce::Colour (0xffe25a2a),
        juce::Colour (0xfff4eadc), juce::Colour (0xff9a8b78),
    };

    const Theme kPurple
    {
        juce::Colour (0xff15111b), juce::Colour (0xff09070d),
        juce::Colour (0xff1f1828), juce::Colour (0xff46395a),
        juce::Colour (0xffb98cff), juce::Colour (0xffe35ad6),
        juce::Colour (0xfff2ebfb), juce::Colour (0xff978aa8),
    };

    const Theme kGreen
    {
        juce::Colour (0xff101612), juce::Colour (0xff070a08),
        juce::Colour (0xff18221b), juce::Colour (0xff36493b),
        juce::Colour (0xff6fdc8c), juce::Colour (0xffc6df4f),
        juce::Colour (0xffe9f5ec), juce::Colour (0xff86a08d),
    };

    const Theme kBlue
    {
        juce::Colour (0xff0f1319), juce::Colour (0xff06090d),
        juce::Colour (0xff161d27), juce::Colour (0xff32465b),
        juce::Colour (0xff5cb8ff), juce::Colour (0xff7d7bff),
        juce::Colour (0xffe8f1fa), juce::Colour (0xff8399ae),
    };

    const Theme kRed
    {
        juce::Colour (0xff171010), juce::Colour (0xff0a0606),
        juce::Colour (0xff231616), juce::Colour (0xff4d3030),
        juce::Colour (0xffff5b5b), juce::Colour (0xffe8467f),
        juce::Colour (0xfff8ecec), juce::Colour (0xffa88a8a),
    };

    struct Current
    {
        juce::String name { "Orange" };
        Theme theme { kOrange };
    };

    Current& current()
    {
        static Current c;
        return c;
    }
}

const juce::StringArray& themeNames()
{
    static const juce::StringArray names { "Orange", "Purple", "Green", "Blue", "Red" };
    return names;
}

const Theme& themeByName (const juce::String& name) noexcept
{
    if (name.equalsIgnoreCase ("Purple")) return kPurple;
    if (name.equalsIgnoreCase ("Green"))  return kGreen;
    if (name.equalsIgnoreCase ("Blue"))   return kBlue;
    if (name.equalsIgnoreCase ("Red"))    return kRed;
    return kOrange;
}

void setCurrentTheme (const juce::String& name)
{
    auto& c = current();
    c.name = themeNames().contains (name, true) ? name : juce::String ("Orange");
    c.theme = themeByName (c.name);
}

juce::String currentThemeName()
{
    return current().name;
}

const Theme& getTheme() noexcept
{
    return current().theme;
}

} // namespace pt
