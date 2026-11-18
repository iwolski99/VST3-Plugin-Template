#include "TileBrowser.h"

namespace pt
{

namespace
{
    constexpr int kTileW = 96;
    constexpr int kTileH = 120;
    constexpr int kIcon  = 80;
    constexpr int kGap   = 10;
    constexpr int kPad   = 10;
    constexpr int kRowH  = 26;

    juce::Colour tintFor (const Theme& t, foundry::Category c)
    {
        using foundry::Category;
        switch (c)
        {
            case Category::OneShotKick: case Category::OneShotSnare: case Category::OneShotClap:
            case Category::OneShotHat:  case Category::OneShotRide:
            case Category::DrumLoopsRiddim: case Category::DrumLoopsDubstep:
                return t.accent;
            case Category::OneShotBass: case Category::BassLoopsBase: case Category::BassLoopsSpliced:
                return t.accentAlt;
            case Category::Riff:
                return t.accent.interpolatedWith (t.text, 0.45f);
            case Category::Unknown:
            default:
                return t.textDim.brighter (0.3f);
        }
    }

    juce::Colour tintFor (const Theme& t, foundry::BrowserEntry::Icon i)
    {
        using Icon = foundry::BrowserEntry::Icon;
        switch (i)
        {
            case Icon::Favourites: return t.accent.brighter (0.25f);
            case Icon::Drums:      return t.accent;
            case Icon::Bass:       return t.accentAlt;
            case Icon::Riffs:      return t.accent.interpolatedWith (t.text, 0.45f);
            case Icon::Generated:  return t.accentAlt.interpolatedWith (t.accent, 0.5f);
            case Icon::Folder:
            default:               return t.textDim.brighter (0.25f);
        }
    }

    juce::String formatSeconds (double s)
    {
        if (s <= 0.0)  return {};
        if (s < 10.0)  return juce::String (s, 2) + "s";
        if (s < 60.0)  return juce::String (s, 1) + "s";
        return juce::String ((int) s / 60) + ":" + juce::String ((int) s % 60).paddedLeft ('0', 2);
    }

    bool isDrumOneShot (foundry::Category c)
    {
        using foundry::Category;
        return c == Category::OneShotKick || c == Category::OneShotClap || c == Category::OneShotSnare
            || c == Category::OneShotHat  || c == Category::OneShotRide;
    }

    void drawStar (juce::Graphics& g, juce::Point<float> c, float r)
    {
        juce::Path star;
        star.addStar (c, 5, r * 0.45f, r);
        g.fillPath (star);
    }

    void drawFolder (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, bool dim)
    {
        const float alpha = dim ? 0.35f : 1.0f;
        auto body = r.reduced (6.0f, 14.0f).translated (0.0f, 4.0f);
        auto tab = juce::Rectangle<float> (body.getX(), body.getY() - 7.0f, body.getWidth() * 0.42f, 10.0f);

        g.setColour (c.darker (0.45f).withAlpha (alpha));
        g.fillRoundedRectangle (tab, 3.0f);

        juce::ColourGradient grad (c.withAlpha (0.92f * alpha), body.getX(), body.getY(),
                                   c.darker (0.55f).withAlpha (alpha), body.getX(), body.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (body, 5.0f);

        g.setColour (juce::Colours::white.withAlpha (0.18f * alpha));
        g.drawLine (body.getX() + 5.0f, body.getY() + 1.5f, body.getRight() - 5.0f, body.getY() + 1.5f, 1.0f);
    }
}

//==============================================================================
class TileBrowser::Crumbs : public juce::Component
{
public:
    explicit Crumbs (TileBrowser& o) : owner (o) {}

    void paint (juce::Graphics& g) override
    {
        hits.clear();
        const auto& t = owner.theme;
        auto path = owner.processor.getBrowser().pathTo (owner.location);

        juce::String count;
        if (owner.search.getText().trim().isNotEmpty())
            count = owner.truncated ? "first " + juce::String ((int) owner.entries.size()) + " matches"
                                    : juce::String ((int) owner.entries.size()) + " matches";
        else
            count = juce::String ((int) owner.entries.size()) + " items";

        auto area = getLocalBounds();
        g.setFont (FoundryLookAndFeel::labelFont (10.0f, false, 0.04f));
        g.setColour (t.textDim);
        g.drawText (count, area.removeFromRight (110), juce::Justification::centredRight);

        const auto font = FoundryLookAndFeel::labelFont (12.5f, true, 0.02f);
        const auto sep = juce::String::fromUTF8 ("  \xE2\x80\xBA  ");
        g.setFont (font);

        // Keep the deepest folders visible; elide from the left if needed.
        size_t first = 0;
        auto widthFrom = [&] (size_t from)
        {
            float w = 0.0f;
            for (size_t i = from; i < path.size(); ++i)
                w += juce::GlyphArrangement::getStringWidth (font, path[i].title + (i + 1 < path.size() ? sep : juce::String()));
            return w;
        };
        while (first + 1 < path.size() && widthFrom (first) > (float) area.getWidth() - 20.0f)
            ++first;

        float x = 4.0f;
        if (first > 0)
        {
            g.setColour (t.textDim);
            g.drawText (juce::String::fromUTF8 ("\xE2\x80\xA6") + sep, juce::Rectangle<float> (x, 0.0f, 40.0f, (float) getHeight()),
                        juce::Justification::centredLeft);
            x += juce::GlyphArrangement::getStringWidth (font, juce::String::fromUTF8 ("\xE2\x80\xA6") + sep);
        }

        for (size_t i = first; i < path.size(); ++i)
        {
            const bool last = i + 1 == path.size();
            const float w = juce::GlyphArrangement::getStringWidth (font, path[i].title);
            auto r = juce::Rectangle<float> (x, 0.0f, w, (float) getHeight());

            g.setColour (last ? t.text : (hover == (int) i ? t.accent : t.textDim.brighter (0.2f)));
            g.drawText (path[i].title, r, juce::Justification::centredLeft, false);
            hits.push_back ({ r, path[i] });
            x += w;

            if (! last)
            {
                const float sw = juce::GlyphArrangement::getStringWidth (font, sep);
                g.setColour (t.textDim.withAlpha (0.6f));
                g.drawText (sep, juce::Rectangle<float> (x, 0.0f, sw, (float) getHeight()), juce::Justification::centredLeft, false);
                x += sw;
            }
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        int h = -1;
        for (size_t i = 0; i < hits.size(); ++i)
            if (hits[i].first.contains (e.position))
                h = (int) i;
        if (h != hover) { hover = h; repaint(); }
    }

    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }

    void mouseUp (const juce::MouseEvent& e) override
    {
        for (auto& h : hits)
            if (h.first.contains (e.position) && ! (h.second == owner.location))
            {
                owner.navigateTo (h.second);
                return;
            }
    }

private:
    TileBrowser& owner;
    std::vector<std::pair<juce::Rectangle<float>, foundry::BrowserLocation>> hits;
    int hover = -1;
};

//==============================================================================
class TileBrowser::Body : public juce::Component
{
public:
    explicit Body (TileBrowser& o) : owner (o) {}

    int columns() const noexcept
    {
        return juce::jmax (1, (getWidth() - 2 * kPad + kGap) / (kTileW + kGap));
    }

    void updateSize (int width)
    {
        const int n = (int) owner.entries.size();
        int h;
        if (owner.listView)
            h = kPad + n * kRowH + kPad;
        else
        {
            const int cols = juce::jmax (1, (width - 2 * kPad + kGap) / (kTileW + kGap));
            const int rows = (n + cols - 1) / cols;
            h = kPad + rows * (kTileH + kGap) + kPad;
        }
        setSize (width, juce::jmax (h, owner.viewport.getHeight()));
    }

    juce::Rectangle<int> boundsFor (int i) const
    {
        if (owner.listView)
            return { kPad, kPad + i * kRowH, getWidth() - 2 * kPad, kRowH };

        const int cols = columns();
        // Spread spare width evenly so the grid never looks ragged on the right.
        const int used = cols * kTileW + (cols - 1) * kGap;
        const int extra = juce::jmax (0, getWidth() - 2 * kPad - used) / juce::jmax (1, cols);
        const int col = i % cols, row = i / cols;
        return { kPad + col * (kTileW + kGap + extra) + extra / 2, kPad + row * (kTileH + kGap), kTileW, kTileH };
    }

    int indexAt (juce::Point<int> p) const
    {
        for (int i = firstVisible(); i < lastVisible(); ++i)
            if (boundsFor (i).contains (p))
                return i;
        return -1;
    }

    int firstVisible() const
    {
        const auto view = owner.viewport.getViewArea();
        if (owner.listView)
            return juce::jmax (0, (view.getY() - kPad) / kRowH - 1);
        return juce::jmax (0, ((view.getY() - kPad) / (kTileH + kGap) - 1) * columns());
    }

    int lastVisible() const
    {
        const auto view = owner.viewport.getViewArea();
        const int n = (int) owner.entries.size();
        if (owner.listView)
            return juce::jmin (n, (view.getBottom() - kPad) / kRowH + 2);
        return juce::jmin (n, ((view.getBottom() - kPad) / (kTileH + kGap) + 2) * columns());
    }

    void paint (juce::Graphics& g) override
    {
        const auto& t = owner.theme;

        if (owner.entries.empty())
        {
            g.setColour (t.textDim);
            g.setFont (FoundryLookAndFeel::labelFont (13.0f, false));
            juce::String msg;
            if (owner.search.getText().trim().isNotEmpty())
                msg = "No samples here match \"" + owner.search.getText().trim() + "\".";
            else if (owner.location.kind == foundry::BrowserLocation::Kind::Favourites)
                msg = "No favourites yet - right-click any sample and choose Add to Favourites.";
            else
                msg = "This folder is empty. Add audio files to it, then press RESCAN.";
            g.drawFittedText (msg, owner.viewport.getViewArea().reduced (24), juce::Justification::centred, 3);
            return;
        }

        for (int i = firstVisible(); i < lastVisible(); ++i)
        {
            if (owner.listView)
                paintRow (g, i, boundsFor (i));
            else
                paintTile (g, i, boundsFor (i));
        }
    }

    void paintWave (juce::Graphics& g, juce::Rectangle<float> r, const foundry::BrowserEntry& e, juce::Colour c)
    {
        auto thumb = owner.processor.getThumbnails().request (e.item.path);
        if (thumb == nullptr)
        {
            g.setColour (c.withAlpha (0.18f));
            g.fillRect (r.withSizeKeepingCentre (r.getWidth(), 1.0f));
            return;
        }

        if (thumb->failed || thumb->peaks.empty())
        {
            g.setColour (owner.theme.textDim.withAlpha (0.6f));
            g.setFont (FoundryLookAndFeel::labelFont (r.getHeight() > 30.0f ? 18.0f : 11.0f, true));
            g.drawText ("?", r, juce::Justification::centred);
            return;
        }

        const auto n = thumb->peaks.size();
        const float binW = r.getWidth() / (float) n;
        const float mid = r.getCentreY();
        for (size_t b = 0; b < n; ++b)
        {
            const float mag = (float) thumb->peaks[b] / 255.0f;
            const float h = juce::jmax (0.6f, mag * r.getHeight() * 0.5f);
            g.setColour (c.withAlpha (0.45f + 0.5f * mag));
            g.fillRect (r.getX() + binW * (float) b + 0.25f, mid - h, juce::jmax (0.8f, binW - 0.6f), h * 2.0f);
        }
    }

    void paintTile (juce::Graphics& g, int i, juce::Rectangle<int> rInt)
    {
        const auto& t = owner.theme;
        const auto& e = owner.entries[(size_t) i];
        const auto r = rInt.toFloat();
        const bool sel = i == owner.selected;
        const bool hot = i == hover;

        auto icon = juce::Rectangle<float> ((float) kIcon, (float) kIcon).withCentre ({ r.getCentreX(), r.getY() + 4.0f + kIcon / 2.0f });

        if (sel || hot)
        {
            g.setColour (t.accent.withAlpha (sel ? 0.18f : 0.08f));
            g.fillRoundedRectangle (r, ui::controlCorner + 2.0f);
            if (sel)
            {
                g.setColour (t.accent.withAlpha (0.65f));
                g.drawRoundedRectangle (r.reduced (0.5f), ui::controlCorner + 2.0f, 1.0f);
            }
        }

        if (e.isFolder())
        {
            const auto c = tintFor (t, e.icon);
            drawFolder (g, icon, c, e.unmapped);

            auto label = icon.reduced (6.0f, 14.0f).translated (0.0f, 6.0f);
            g.setColour (FoundryLookAndFeel::pillTextColour (c).withAlpha (e.unmapped ? 0.5f : 0.9f));
            if (e.icon == foundry::BrowserEntry::Icon::Favourites)
                drawStar (g, label.getCentre(), 12.0f);
            else
            {
                g.setFont (FoundryLookAndFeel::labelFont (e.unmapped ? 9.5f : 13.0f, true));
                g.drawText (e.unmapped ? "not set" : juce::String (e.count), label, juce::Justification::centred);
            }
        }
        else
        {
            const auto c = tintFor (t, e.item.category);
            FoundryLookAndFeel::drawInsetWell (g, icon, t, 7.0f);
            g.setColour (c.withAlpha (0.35f));
            g.drawRoundedRectangle (icon.reduced (0.5f), 7.0f, 1.0f);

            paintWave (g, icon.reduced (7.0f, 16.0f), e, c);

            g.setFont (FoundryLookAndFeel::labelFont (8.5f, true, 0.05f));
            if (auto thumb = owner.processor.getThumbnails().request (e.item.path))
            {
                g.setColour (t.textDim);
                g.drawText (formatSeconds (thumb->seconds), icon.reduced (6.0f, 3.0f).removeFromBottom (12.0f),
                            juce::Justification::bottomLeft);
            }

            if (e.item.category == foundry::Category::Riff)
            {
                g.setColour (c);
                g.drawText ("RIFF", icon.reduced (6.0f, 3.0f).removeFromTop (12.0f), juce::Justification::topLeft);
            }
            else if (e.item.fileType == "loop")
            {
                g.setColour (t.textDim);
                g.drawText ("LOOP", icon.reduced (6.0f, 3.0f).removeFromTop (12.0f), juce::Justification::topLeft);
            }

            if (e.item.favorite)
            {
                g.setColour (t.accent);
                drawStar (g, { icon.getRight() - 10.0f, icon.getY() + 9.0f }, 5.5f);
            }
        }

        // The name is always there: thumbnails help you scan, names let you find.
        auto nameArea = juce::Rectangle<float> (r.getX() - 2.0f, icon.getBottom() + 4.0f, r.getWidth() + 4.0f, r.getBottom() - icon.getBottom() - 4.0f);
        g.setColour (sel ? t.text : t.text.withAlpha (0.86f));
        g.setFont (FoundryLookAndFeel::labelFont (11.0f, sel, 0.0f));
        g.drawFittedText (e.name, nameArea.toNearestInt(), juce::Justification::centredTop, 2, 0.85f);
    }

    void paintRow (juce::Graphics& g, int i, juce::Rectangle<int> r)
    {
        const auto& t = owner.theme;
        const auto& e = owner.entries[(size_t) i];
        const bool sel = i == owner.selected;

        if (sel)
            FoundryLookAndFeel::drawActivePill (g, r.toFloat().reduced (0.0f, 2.0f), ui::controlCorner, t.accent, 0.85f);
        else if (i == hover)
        {
            g.setColour (t.accent.withAlpha (0.08f));
            g.fillRoundedRectangle (r.toFloat().reduced (0.0f, 2.0f), ui::controlCorner);
        }

        const auto textCol = sel ? FoundryLookAndFeel::pillTextColour (t.accent) : t.text;
        auto icon = r.removeFromLeft (64).reduced (4, 4).toFloat();

        if (e.isFolder())
        {
            drawFolder (g, icon.withSizeKeepingCentre (26.0f, 26.0f).translated (0.0f, -2.0f), tintFor (t, e.icon), e.unmapped);
            auto meta = r.removeFromRight (150);
            g.setFont (FoundryLookAndFeel::labelFont (10.0f, false));
            g.setColour (sel ? textCol.withAlpha (0.7f) : t.textDim);
            g.drawText (e.unmapped ? "not set" : juce::String (e.count) + " samples", meta.reduced (6, 0), juce::Justification::centredRight);
        }
        else
        {
            paintWave (g, icon, e, tintFor (t, e.item.category));
            auto meta = r.removeFromRight (150);
            juce::String right = foundry::categoryName (e.item.category);
            if (auto thumb = owner.processor.getThumbnails().request (e.item.path))
                right += "   " + formatSeconds (thumb->seconds);
            g.setFont (FoundryLookAndFeel::labelFont (10.0f, false));
            g.setColour (sel ? textCol.withAlpha (0.7f) : t.textDim);
            g.drawText (right, meta.reduced (6, 0), juce::Justification::centredRight);

            if (e.item.favorite)
            {
                g.setColour (sel ? textCol : t.accent);
                drawStar (g, { (float) r.getX() + 6.0f, (float) r.getCentreY() }, 5.0f);
            }
        }

        g.setFont (FoundryLookAndFeel::labelFont (12.0f, sel, 0.01f));
        g.setColour (textCol);
        g.drawText (e.name, r.withTrimmedLeft (16), juce::Justification::centredLeft, true);
    }

    juce::Image dragImageFor (const foundry::BrowserEntry& e)
    {
        juce::Image img (juce::Image::ARGB, kIcon, kIcon, true);
        juce::Graphics g (img);
        auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) kIcon, (float) kIcon);
        const auto c = tintFor (owner.theme, e.item.category);
        g.setColour (owner.theme.panel.withAlpha (0.92f));
        g.fillRoundedRectangle (r, 7.0f);
        g.setColour (c);
        g.drawRoundedRectangle (r.reduced (1.0f), 7.0f, 1.5f);
        paintWave (g, r.reduced (7.0f, 18.0f), e, c);
        g.setColour (owner.theme.text);
        g.setFont (FoundryLookAndFeel::labelFont (9.5f, true));
        g.drawFittedText (e.name, r.reduced (4.0f).removeFromBottom (14.0f).toNearestInt(), juce::Justification::centred, 1, 0.7f);
        return img;
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int h = indexAt (e.getPosition());
        if (h != hover) { hover = h; repaint(); }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (hover != -1) { hover = -1; repaint(); }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        pressIndex = indexAt (e.getPosition());
        dragging = false;

        if (e.mods.isPopupMenu() && pressIndex >= 0)
        {
            owner.selected = pressIndex;
            repaint();
            owner.showMenu (pressIndex);
            pressIndex = -1;
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging || pressIndex < 0 || pressIndex >= (int) owner.entries.size())
            return;
        if (e.getDistanceFromDragStart() < 6)
            return;

        const auto& entry = owner.entries[(size_t) pressIndex];
        if (entry.isFolder())
            return;

        dragging = true;
        if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            const juce::Point<int> offset (-kIcon / 2, -kIcon / 2);
            c->startDragging (dnd::samplePrefix + entry.item.path, this,
                              juce::ScaledImage (dragImageFor (entry)), true, &offset);
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (dragging)
        {
            dragging = false;
            return;
        }

        const int i = indexAt (e.getPosition());
        if (i >= 0 && i == pressIndex && ! e.mods.isPopupMenu() && e.getNumberOfClicks() < 2)
            owner.activate (i, false);
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int i = indexAt (e.getPosition());
        if (i >= 0 && ! e.mods.isPopupMenu())
            owner.activate (i, true);
    }

    int hover = -1;

private:
    TileBrowser& owner;
    int pressIndex = -1;
    bool dragging = false;
};

//==============================================================================
TileBrowser::TileBrowser (FoundryAudioProcessor& p)
    : processor (p)
{
    crumbs = std::make_unique<Crumbs> (*this);
    body = std::make_unique<Body> (*this);

    listView = processor.getConfig().get().browserListView;
    location = foundry::BrowserModel::root();

    upButton.setTooltip ("Up one folder");
    upButton.onClick = [this] { goUp(); };

    tilesButton.setClickingTogglesState (false);
    listButton.setClickingTogglesState (false);
    tilesButton.onClick = [this] { setListView (false); };
    listButton.onClick  = [this] { setListView (true); };
    tilesButton.setTooltip ("Show samples as tiles with waveforms");
    listButton.setTooltip ("Show samples as a compact list");

    rescanButton.setTooltip ("Re-index your library folders (new and removed files)");
    rescanButton.onClick = [this]
    {
        auto err = processor.startLibraryScan();
        if (err.isNotEmpty())
            message (err, true);
    };

    search.setTextToShowWhenEmpty ("Search this folder...", getTheme().textDim);
    search.onTextChange = [this] { searchDebounce = 3; };
    search.onEscapeKey = [this] { search.clear(); refresh(); };
    search.onReturnKey = [this] { searchDebounce = 0; refresh(); };

    viewport.setViewedComponent (body.get(), false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::never);

    addAndMakeVisible (upButton);
    addAndMakeVisible (*crumbs);
    addAndMakeVisible (search);
    addAndMakeVisible (tilesButton);
    addAndMakeVisible (listButton);
    addAndMakeVisible (rescanButton);
    addAndMakeVisible (viewport);

    refresh();
    startTimerHz (15);
}

TileBrowser::~TileBrowser()
{
    stopTimer();
}

void TileBrowser::setTheme (const Theme& t)
{
    theme = t;
    for (auto* b : { &upButton, &rescanButton })
        b->setColour (juce::TextButton::buttonColourId, t.accent);
    setListView (listView);
    repaint();
}

void TileBrowser::setListView (bool list)
{
    listView = list;
    processor.getConfig().get().browserListView = list;
    tilesButton.setColour (juce::TextButton::buttonColourId, list ? theme.panelEdge : theme.accent);
    listButton.setColour (juce::TextButton::buttonColourId, list ? theme.accent : theme.panelEdge);
    body->updateSize (viewport.getMaximumVisibleWidth());
    body->repaint();
}

void TileBrowser::refresh()
{
    const auto selectedPath = selected >= 0 && selected < (int) entries.size() && ! entries[(size_t) selected].isFolder()
                                  ? entries[(size_t) selected].item.path : juce::String();

    entries = processor.getBrowser().list (location, search.getText(), truncated);

    selected = -1;
    for (size_t i = 0; i < entries.size(); ++i)
        if (selectedPath.isNotEmpty() && ! entries[i].isFolder() && entries[i].item.path == selectedPath)
            selected = (int) i;

    upButton.setEnabled (location.kind != foundry::BrowserLocation::Kind::Root);
    body->hover = -1;
    body->updateSize (viewport.getMaximumVisibleWidth());
    body->repaint();
    crumbs->repaint();
}

void TileBrowser::navigateTo (const foundry::BrowserLocation& where)
{
    location = where;
    search.clear();
    searchDebounce = 0;
    selected = -1;
    refresh();
    viewport.setViewPosition (0, 0);
}

void TileBrowser::showFolder (const juce::File& dir)
{
    foundry::BrowserLocation l;
    l.kind = foundry::BrowserLocation::Kind::Folder;
    l.dir = dir.getFullPathName();
    l.title = dir.getFileName();
    navigateTo (l);
}

void TileBrowser::goUp()
{
    auto path = processor.getBrowser().pathTo (location);
    if (path.size() > 1)
        navigateTo (path[path.size() - 2]);
}

std::vector<foundry::LibraryItem> TileBrowser::dicePool() const
{
    if (search.getText().trim().isNotEmpty())
    {
        std::vector<foundry::LibraryItem> out;
        for (const auto& e : entries)
            if (! e.isFolder())
                out.push_back (e.item);
        return out;
    }
    return processor.getBrowser().filesUnder (location);
}

void TileBrowser::message (const juce::String& m, bool error)
{
    if (onMessage)
        onMessage (m, error);
}

void TileBrowser::activate (int index, bool doubleClick)
{
    if (index < 0 || index >= (int) entries.size())
        return;

    const auto e = entries[(size_t) index];

    if (e.isFolder())
    {
        if (e.unmapped)
        {
            message ("The " + e.name + " folder isn't set yet - choose it in Settings (or create the recommended structure).", true);
            return;
        }
        navigateTo (e.target);
        return;
    }

    selected = index;
    body->repaint();

    const juce::File file (e.item.path);

    if (doubleClick)
    {
        if (FoundryAudioProcessor::isEditableRiff (file) && onOpenRiff)
            onOpenRiff (file);
        else if (onLoadIntoSlot)
            onLoadIntoSlot (-1, file);
        return;
    }

    juce::String err;
    if (! processor.previewItem (e.item, err))
        message (err, true);
}

void TileBrowser::showMenu (int index)
{
    if (index < 0 || index >= (int) entries.size())
        return;

    const auto e = entries[(size_t) index];
    juce::PopupMenu m;

    if (e.isFolder())
    {
        m.addItem (1, "Open", ! e.unmapped);
        m.addItem (2, "Show in Explorer / Finder", ! e.unmapped && e.target.dir.isNotEmpty());
        m.showMenuAsync (juce::PopupMenu::Options(), [this, e] (int r)
        {
            if (r == 1) navigateTo (e.target);
            if (r == 2) juce::File (e.target.dir).revealToUser();
        });
        return;
    }

    const juce::File file (e.item.path);
    const bool riffFile = FoundryAudioProcessor::isEditableRiff (file);

    m.addItem (1, "Preview");
    if (riffFile)
        m.addItem (2, "Open in Riff Maker (edit)");
    m.addItem (3, "Load into next empty slot");

    juce::PopupMenu slots;
    for (int s = 0; s < foundry::kRiffSlots; ++s)
        slots.addItem (100 + s, "Slot " + juce::String (s + 1));
    m.addSubMenu ("Load into slot", slots);

    m.addSeparator();
    m.addItem (4, e.item.favorite ? "Remove from Favourites" : "Add to Favourites");
    if (foundry::isBassLoopCategory (e.item.category))
        m.addItem (5, "Send to Splicer");
    if (isDrumOneShot (e.item.category))
        m.addItem (6, "Lock in Drum Generator");
    m.addItem (7, "Show in Explorer / Finder");
    if (foundry::isGeneratedCategory (e.item.category))
    {
        m.addSeparator();
        m.addItem (8, "Delete file...");
    }

    m.showMenuAsync (juce::PopupMenu::Options(), [this, e, file] (int r)
    {
        if (r == 1)
        {
            juce::String err;
            if (! processor.previewItem (e.item, err))
                message (err, true);
        }
        else if (r == 2 && onOpenRiff)       onOpenRiff (file);
        else if (r == 3 && onLoadIntoSlot)   onLoadIntoSlot (-1, file);
        else if (r >= 100 && r < 100 + foundry::kRiffSlots && onLoadIntoSlot)
            onLoadIntoSlot (r - 100, file);
        else if (r == 4)
        {
            juce::String err;
            processor.getLibrary().setFavorite (e.item.id, ! e.item.favorite, err);
            refresh();
        }
        else if (r == 5 && onSendToSplicer)  onSendToSplicer (e.item);
        else if (r == 6 && onLockInDrums)    onLockInDrums (e.item);
        else if (r == 7)                     file.revealToUser();
        else if (r == 8)                     confirmDelete (e.item);
    });
}

void TileBrowser::confirmDelete (const foundry::LibraryItem& item)
{
    const juce::File file (item.path);
    auto options = juce::MessageBoxOptions()
                       .withIconType (juce::MessageBoxIconType::WarningIcon)
                       .withTitle ("Delete " + file.getFileName() + "?")
                       .withMessage ("This removes the file from disk. It can't be undone.")
                       .withButton ("Delete")
                       .withButton ("Cancel")
                       .withAssociatedComponent (this);

    juce::Component::SafePointer<TileBrowser> safe (this);
    juce::AlertWindow::showAsync (options, [safe, item] (int result)
    {
        if (safe == nullptr || result != 1)
            return;

        const juce::File f (item.path);
        f.deleteFile();
        foundry::riffSidecarFor (f).deleteFile();

        juce::String err;
        safe->processor.getLibrary().removeById (item.id, err);
        safe->processor.getThumbnails().forget (item.path);
        safe->refresh();
    });
}

void TileBrowser::timerCallback()
{
    if (searchDebounce > 0 && --searchDebounce == 0)
        refresh();

    const int gen = processor.getThumbnails().generation();
    if (gen != lastThumbGeneration)
    {
        lastThumbGeneration = gen;
        body->repaint();
    }
}

void TileBrowser::resized()
{
    auto b = getLocalBounds().reduced (8, 6);
    auto top = b.removeFromTop (kTopBarH - 6).withTrimmedBottom (4);

    upButton.setBounds (top.removeFromLeft (30));
    top.removeFromLeft (8);
    rescanButton.setBounds (top.removeFromRight (74));
    top.removeFromRight (8);
    listButton.setBounds (top.removeFromRight (46));
    top.removeFromRight (2);
    tilesButton.setBounds (top.removeFromRight (52));
    top.removeFromRight (8);
    search.setBounds (top.removeFromRight (230));
    top.removeFromRight (10);
    crumbs->setBounds (top);

    b.removeFromTop (4);
    viewport.setBounds (b);
    body->updateSize (viewport.getMaximumVisibleWidth());
}

void TileBrowser::paint (juce::Graphics& g)
{
    FoundryLookAndFeel::drawPanel (g, getLocalBounds().toFloat().reduced (2.0f), theme);
    g.setColour (theme.panelEdge.withAlpha (0.45f));
    g.fillRect (10.0f, (float) kTopBarH + 1.0f, (float) getWidth() - 20.0f, 1.0f);
}

} // namespace pt
