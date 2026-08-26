#include "PresetBrowser.h"

namespace pt
{

namespace
{
    constexpr int kCardMargin  = 26;
    constexpr int kSidebarW    = 132;
    constexpr int kHeaderH     = 44;
    constexpr int kFooterH     = 40;
    constexpr int kRowH        = 26;
}

//==============================================================================
PresetBrowser::PresetBrowser (PresetManager& manager)
    : presets (manager), theme (getTheme())
{
    categories.add ("All");
    categories.addArray (presetCategories());

    addAndMakeVisible (closeButton);
    closeButton.onClick = [this] { hide(); };

    setWantsKeyboardFocus (true);
    setVisible (false);
    setComponentID ("presetBrowser");
}

void PresetBrowser::setTheme (const Theme& t)
{
    theme = t;

    closeButton.setColour (juce::TextButton::buttonColourId, theme.panel.brighter (0.10f));
    closeButton.setColour (juce::TextButton::textColourOffId, theme.text);
    repaint();
}

//==============================================================================
void PresetBrowser::rebuildEntries()
{
    allEntries.clear();

    const auto& factory = getFactoryPresets();

    for (int i = 0; i < (int) factory.size(); ++i)
        allEntries.push_back ({ factory[(size_t) i].name, factory[(size_t) i].category, false, i });

    // User presets carry whichever folder they were saved into, read back off
    // the file, so they file alongside the factory bank rather than all
    // landing in one pile.
    const auto& user = presets.getUserNames();
    const auto& cats = presets.getUserCategories();

    for (int i = 0; i < user.size(); ++i)
        allEntries.push_back ({ user[i], i < cats.size() ? cats[i] : juce::String ("User"), true, i });

    refreshFilter();
}

void PresetBrowser::refreshFilter()
{
    visible.clear();
    loadedTile = -1;

    const juce::String wanted = categories[selectedCategory];
    const bool all = (selectedCategory == 0);
    const juce::String current = presets.getCurrentName();

    for (int i = 0; i < (int) allEntries.size(); ++i)
    {
        if (all || allEntries[(size_t) i].category == wanted)
        {
            if (allEntries[(size_t) i].name == current)
                loadedTile = (int) visible.size();

            visible.push_back (i);
        }
    }

    resized();
    repaint();
}

void PresetBrowser::show()
{
    rebuildEntries();
    setVisible (true);
    toFront (true);
    grabKeyboardFocus();
}

void PresetBrowser::hide()
{
    setVisible (false);
}

//==============================================================================
juce::Rectangle<int> PresetBrowser::cardBounds() const
{
    return getLocalBounds().reduced (kCardMargin);
}

juce::Rectangle<int> PresetBrowser::sidebarBounds() const
{
    auto c = cardBounds();
    c.removeFromTop (kHeaderH);
    c.removeFromBottom (kFooterH);
    return c.removeFromLeft (kSidebarW).reduced (10, 4);
}

juce::Rectangle<int> PresetBrowser::gridBounds() const
{
    auto c = cardBounds();
    c.removeFromTop (kHeaderH);
    c.removeFromBottom (kFooterH);
    c.removeFromLeft (kSidebarW);
    return c.reduced (12, 4);
}

void PresetBrowser::resized()
{
    auto card = cardBounds();
    closeButton.setBounds (card.getRight() - 88, card.getY() + 10, 72, 24);

    // Fit as many columns as the grid is wide enough for, then size the rows
    // so the whole filtered category lands on one screen wherever it can.
    auto g = gridBounds();
    const int minTileW = 150;

    columns = juce::jlimit (1, 6, (g.getWidth() + gridGap) / (minTileW + gridGap));
    tileW = (g.getWidth() - gridGap * (columns - 1)) / juce::jmax (1, columns);

    const int rows = juce::jmax (1, ((int) visible.size() + columns - 1) / columns);
    const int fit  = (g.getHeight() - gridGap * (rows - 1)) / rows;

    tileH = juce::jlimit (24, 40, fit);
}

//==============================================================================
int PresetBrowser::tileIndexAt (juce::Point<int> p) const
{
    auto g = gridBounds();
    if (! g.contains (p))
        return -1;

    const int col = (p.x - g.getX()) / juce::jmax (1, tileW + gridGap);
    const int row = (p.y - g.getY()) / juce::jmax (1, tileH + gridGap);

    if (col < 0 || col >= columns)
        return -1;

    const int idx = row * columns + col;
    return idx >= 0 && idx < (int) visible.size() ? idx : -1;
}

void PresetBrowser::mouseMove (const juce::MouseEvent& e)
{
    const int t = tileIndexAt (e.getPosition());

    if (t != hoveredTile)
    {
        hoveredTile = t;
        repaint();
    }
}

void PresetBrowser::mouseExit (const juce::MouseEvent&)
{
    if (hoveredTile >= 0)
    {
        hoveredTile = -1;
        repaint();
    }
}

void PresetBrowser::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.getPosition();

    // Clicking the dimmed area outside the card closes, the way a modal sheet
    // does - the panel covers the whole window, so without this there would
    // be no way out but the button.
    if (! cardBounds().contains (p))
    {
        hide();
        return;
    }

    auto side = sidebarBounds();

    if (side.contains (p))
    {
        const int row = (p.y - side.getY()) / kRowH;

        if (row >= 0 && row < categories.size() && row != selectedCategory)
        {
            selectedCategory = row;
            refreshFilter();
        }

        return;
    }

    const int t = tileIndexAt (p);

    if (t >= 0)
    {
        loadEntry (allEntries[(size_t) visible[(size_t) t]]);
        loadedTile = t;
        repaint();
    }
}

void PresetBrowser::loadEntry (const Entry& e)
{
    // Single click loads straight away. Seeing the whole bank at once is only
    // useful if you can walk along it and hear each one.
    if (e.isUser)
        presets.loadUser (e.index);
    else
        presets.loadFactory (e.index);

    if (onPresetLoaded != nullptr)
        onPresetLoaded();
}

bool PresetBrowser::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey)
    {
        hide();
        return true;
    }

    return false;
}

//==============================================================================
void PresetBrowser::paint (juce::Graphics& g)
{
    // Scrim over the live UI: dark, and translucent enough that the plugin is
    // still faintly readable behind it without competing with the names,
    // which sit on their own opaque tiles.
    g.fillAll (juce::Colour (0xff05060a).withAlpha (0.88f));

    auto card = cardBounds().toFloat();

    PluginTemplateLookAndFeel::dropShadow (g, card, ui::panelCorner, 10.0f, 0.55f);

    g.setColour (juce::Colour (0xff0c0e14).withAlpha (0.97f));
    g.fillRoundedRectangle (card, ui::panelCorner);

    g.setColour (theme.accent.withAlpha (0.30f));
    g.drawRoundedRectangle (card.reduced (0.5f), ui::panelCorner, 1.0f);

    // --- header -----------------------------------------------------------
    g.setFont (PluginTemplateLookAndFeel::titleFont (15.0f, 0.22f));
    g.setColour (theme.text);
    g.drawText ("PRESETS", (int) card.getX() + 18, (int) card.getY() + 8, 220, 26,
                juce::Justification::centredLeft);

    g.setFont (PluginTemplateLookAndFeel::labelFont (9.5f, false, 0.06f));
    g.setColour (theme.textDim.withAlpha (0.75f));
    g.drawText (juce::String ((int) visible.size()) + " presets  \xc2\xb7  click to load",
                (int) card.getX() + 116, (int) card.getY() + 8, 300, 26,
                juce::Justification::centredLeft);

    g.setColour (theme.panelEdge.withAlpha (0.55f));
    g.fillRect ((int) card.getX() + 14, (int) card.getY() + kHeaderH - 2,
                (int) card.getWidth() - 28, 1);

    // --- sidebar -----------------------------------------------------------
    auto side = sidebarBounds();

    for (int i = 0; i < categories.size(); ++i)
    {
        juce::Rectangle<int> row (side.getX(), side.getY() + i * kRowH, side.getWidth(), kRowH - 3);

        if (i == selectedCategory)
        {
            g.setColour (theme.accent.withAlpha (0.22f));
            g.fillRoundedRectangle (row.toFloat(), ui::controlCorner);

            g.setColour (theme.accent);
            g.fillRoundedRectangle ((float) row.getX(), (float) row.getY() + 3.0f,
                                    2.5f, (float) row.getHeight() - 6.0f, 1.2f);
        }

        g.setFont (PluginTemplateLookAndFeel::labelFont (10.5f, i == selectedCategory, 0.05f));
        g.setColour (i == selectedCategory ? theme.text : theme.textDim.withAlpha (0.8f));
        g.drawText (categories[i], row.reduced (11, 0), juce::Justification::centredLeft);
    }

    g.setColour (theme.panelEdge.withAlpha (0.4f));
    g.fillRect (side.getRight() + 8, side.getY(), 1, side.getHeight());

    // --- tiles ---------------------------------------------------------------
    auto grid = gridBounds();

    for (int i = 0; i < (int) visible.size(); ++i)
    {
        const int col = i % columns;
        const int row = i / columns;

        juce::Rectangle<int> t (grid.getX() + col * (tileW + gridGap),
                                grid.getY() + row * (tileH + gridGap),
                                tileW, tileH);

        if (! grid.intersects (t))
            continue;

        const auto& entry = allEntries[(size_t) visible[(size_t) i]];
        const bool isLoaded = (i == loadedTile);
        const bool isHover  = (i == hoveredTile);

        if (isLoaded)
        {
            PluginTemplateLookAndFeel::drawActivePill (g, t.toFloat(), ui::controlCorner, theme.accent, 1.0f);
            g.setColour (PluginTemplateLookAndFeel::pillTextColour (theme.accent));
        }
        else
        {
            g.setColour (juce::Colour (0xff161a23).withAlpha (isHover ? 1.0f : 0.85f));
            g.fillRoundedRectangle (t.toFloat(), ui::controlCorner);

            g.setColour (isHover ? theme.accent.withAlpha (0.65f)
                                 : theme.panelEdge.withAlpha (0.5f));
            g.drawRoundedRectangle (t.toFloat().reduced (0.5f), ui::controlCorner, 1.0f);

            g.setColour (isHover ? theme.text : theme.text.withAlpha (0.86f));
        }

        g.setFont (PluginTemplateLookAndFeel::labelFont (10.5f, isLoaded, 0.02f));
        g.drawText (entry.name, t.reduced (9, 0), juce::Justification::centredLeft, true);

        // A dot marks a user preset, so the two banks are distinguishable
        // inside "All" without a second heading.
        if (entry.isUser && ! isLoaded)
        {
            g.setColour (theme.accentAlt.withAlpha (0.8f));
            g.fillEllipse ((float) t.getRight() - 11.0f, (float) t.getCentreY() - 2.5f, 5.0f, 5.0f);
        }
    }

    if (visible.empty())
    {
        g.setFont (PluginTemplateLookAndFeel::labelFont (11.0f, false, 0.05f));
        g.setColour (theme.textDim.withAlpha (0.7f));
        g.drawText ("Nothing saved in this folder yet", grid, juce::Justification::centred);
    }

    // --- footer --------------------------------------------------------------
    g.setFont (PluginTemplateLookAndFeel::labelFont (9.0f, false, 0.05f));
    g.setColour (theme.textDim.withAlpha (0.6f));
    g.drawText ("Esc or click outside to close", (int) card.getX() + 18,
                (int) card.getBottom() - kFooterH + 8, 420, 20,
                juce::Justification::centredLeft);
}

} // namespace pt
