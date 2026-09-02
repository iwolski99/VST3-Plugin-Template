#include "LibraryPanel.h"

namespace pt
{

namespace
{
    struct NavItem { juce::String label; bool header; foundry::Category cat; bool fav; };

    const NavItem kNav[] = {
        { "FAVOURITES",          true,  foundry::Category::Unknown,          true  },
        { "Favourites",          false, foundry::Category::Unknown,          true  },
        { "DRUM LOOPS",          true,  foundry::Category::Unknown,          false },
        { "Riddim",              false, foundry::Category::DrumLoopsRiddim,  false },
        { "Dubstep",             false, foundry::Category::DrumLoopsDubstep, false },
        { "BASS LOOPS",          true,  foundry::Category::Unknown,          false },
        { "Base Riffs",          false, foundry::Category::BassLoopsBase,    false },
        { "Spliced Riffs",       false, foundry::Category::BassLoopsSpliced, false },
        { "ONE-SHOTS",           true,  foundry::Category::Unknown,          false },
        { "Kicks",               false, foundry::Category::OneShotKick,      false },
        { "Snares",              false, foundry::Category::OneShotSnare,     false },
        { "Claps",               false, foundry::Category::OneShotClap,      false },
        { "Closed Hats",         false, foundry::Category::OneShotHat,       false },
        { "Rides",               false, foundry::Category::OneShotRide,      false },
        { "Bass",                false, foundry::Category::OneShotBass,      false },
    };

    constexpr int kNavN = (int) (sizeof (kNav) / sizeof (kNav[0]));
}

LibraryPanel::LibraryPanel (FoundryAudioProcessor& p)
    : processor (p)
{
    search.setTextToShowWhenEmpty ("Search filename…", getTheme().textDim);
    search.onTextChange = [this] { rebuild(); };
    addAndMakeVisible (search);

    sortBox.addItem ("Name", 1);
    sortBox.addItem ("Newest", 2);
    sortBox.addItem ("Oldest", 3);
    sortBox.addItem ("BPM", 4);
    sortBox.addItem ("Genre", 5);
    sortBox.addItem ("Favourites", 6);
    sortBox.setSelectedId (2, juce::dontSendNotification);
    sortBox.onChange = [this] { rebuild(); };
    addAndMakeVisible (sortBox);

    refreshButton.onClick = [this]
    {
        auto err = processor.startLibraryScan();
        if (err.isNotEmpty())
            banner.setMessage (err, true, true);
    };
    addAndMakeVisible (refreshButton);
    addAndMakeVisible (list);
    addChildComponent (banner);

    banner.onOpenSettings = [this] { if (onOpenSettings) onOpenSettings(); };

    list.onPreview = [this] (const foundry::LibraryItem& it)
    {
        juce::String err;
        if (! processor.previewItem (it, err))
            banner.setMessage (err, true, false);
        else
            banner.setMessage ({}, false, false);
    };
    list.onToggleFavorite = [this] (const foundry::LibraryItem& it)
    {
        juce::String err;
        processor.getLibrary().setFavorite (it.id, ! it.favorite, err);
        rebuild();
    };
    list.onReveal = [] (const foundry::LibraryItem& it)
    {
        juce::File (it.path).revealToUser();
    };
    list.onDelete = [this] (const foundry::LibraryItem& it)
    {
        juce::File f (it.path);
        if (f.existsAsFile())
            f.deleteFile();
        juce::String err;
        processor.getLibrary().removeById (it.id, err);
        rebuild();
    };
    list.onSendToSplicer = [this] (const foundry::LibraryItem& it)
    {
        if (onSendToSplicer)
            onSendToSplicer (it);
    };
    list.onLockFrom = [this] (const foundry::LibraryItem& it)
    {
        if (onLockFrom)
            onLockFrom (it);
    };

    rebuild();
}

void LibraryPanel::setTheme (const Theme& t)
{
    theme = t;
    list.setTheme (t);
    banner.setTheme (t);
    refreshButton.setColour (juce::TextButton::buttonColourId, t.accent);
    sortBox.setColour (juce::ComboBox::textColourId, t.text);
    repaint();
}

void LibraryPanel::setSearch (const juce::String& s)
{
    search.setText (s, juce::dontSendNotification);
    rebuild();
}

void LibraryPanel::refresh()
{
    rebuild();
}

void LibraryPanel::rebuild()
{
    const auto& nav = kNav[juce::jlimit (0, kNavN - 1, navIndex)];
    const bool fav = nav.fav && ! nav.header;
    const auto cat = nav.header ? foundry::Category::Unknown : nav.cat;
    const auto sort = (foundry::SortMode) (sortBox.getSelectedId() - 1);
    auto items = processor.getLibrary().query (fav ? foundry::Category::Unknown : cat,
                                               search.getText(), sort, fav);
    list.setItems (std::move (items));
    repaint();
}

int LibraryPanel::navHit (juce::Point<int> p) const
{
    if (p.x > 188 || p.y < 54)
        return -1;
    const int row = (p.y - 54) / 22;
    if (row < 0 || row >= kNavN)
        return -1;
    if (kNav[row].header)
        return -1;
    return row;
}

void LibraryPanel::resized()
{
    auto b = getLocalBounds().reduced (12);
    banner.setBounds (b.removeFromTop (banner.isVisible() ? 36 : 0));
    if (banner.isVisible())
        b.removeFromTop (8);

    auto top = b.removeFromTop (30);
    top.removeFromLeft (188);
    search.setBounds (top.removeFromLeft (juce::jmax (160, top.getWidth() - 250)));
    top.removeFromLeft (8);
    sortBox.setBounds (top.removeFromLeft (120));
    top.removeFromLeft (8);
    refreshButton.setBounds (top.removeFromLeft (90));

    b.removeFromTop (8);
    auto nav = b.removeFromLeft (180);
    juce::ignoreUnused (nav);
    b.removeFromLeft (10);
    list.setBounds (b);
}

void LibraryPanel::paint (juce::Graphics& g)
{
    FoundryLookAndFeel::drawPanel (g, getLocalBounds().toFloat().reduced (6.0f), theme);
    paintNav (g);
}

void LibraryPanel::paintNav (juce::Graphics& g)
{
    auto nav = juce::Rectangle<float> (18.0f, 54.0f, 176.0f, (float) getHeight() - 72.0f);

    for (int i = 0; i < kNavN; ++i)
    {
        auto row = juce::Rectangle<float> (nav.getX(), nav.getY() + 22.0f * (float) i, nav.getWidth(), 22.0f);

        if (kNav[i].header)
        {
            g.setFont (FoundryLookAndFeel::labelFont (8.5f, true, 0.16f));
            g.setColour (theme.textDim);
            g.drawText (kNav[i].label, row.toNearestInt().translated (4, 4), juce::Justification::centredLeft);
            continue;
        }

        const bool on = i == navIndex;
        if (on)
            FoundryLookAndFeel::drawActivePill (g, row.reduced (2.0f, 2.0f), ui::controlCorner, theme.accent, 0.9f);

        g.setFont (FoundryLookAndFeel::labelFont (12.0f, on, 0.02f));
        g.setColour (on ? FoundryLookAndFeel::pillTextColour (theme.accent) : theme.text);
        g.drawText (kNav[i].label, row.reduced (10.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft);
    }
}

void LibraryPanel::mouseUp (const juce::MouseEvent& e)
{
    const int hit = navHit (e.getPosition());
    if (hit >= 0)
    {
        navIndex = hit;
        rebuild();
    }
}

} // namespace pt
