#include "TileBrowser.h"
#include <algorithm>

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
            case Icon::Patterns:   return t.accent.interpolatedWith (t.panelEdge, 0.35f);
            case Icon::Custom:     return t.text.interpolatedWith (t.accent, 0.25f).darker (0.15f);
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

    void drawPatternGrid (juce::Graphics& g, juce::Rectangle<float> r, const foundry::PatternFile& p, juce::Colour c)
    {
        const int steps = juce::jmax (1, p.grid.numSteps());
        const float cw = r.getWidth() / (float) steps;
        const float ch = r.getHeight() / 4.0f;
        for (int row = 0; row < 4; ++row)
        {
            for (int s = 0; s < steps; ++s)
            {
                auto cell = juce::Rectangle<float> (r.getX() + cw * (float) s, r.getY() + ch * (float) row, cw, ch)
                                .reduced (juce::jmin (0.6f, cw * 0.15f), 0.8f);
                const bool on = p.grid.getStep (row, s);
                g.setColour (on ? c : c.withAlpha ((s % 4 == 0) ? 0.16f : 0.07f));
                g.fillRect (cell);
            }
        }
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
            count = juce::String ((int) owner.entries.size()) + (owner.truncated ? "+ items" : " items");
        if (owner.selection.size() > 1)
            count = juce::String ((int) owner.selection.size()) + " selected  ·  " + count;

        auto area = getLocalBounds();
        g.setFont (FoundryLookAndFeel::labelFont (10.0f, false, 0.04f));
        g.setColour (owner.selection.size() > 1 ? t.accent : t.textDim);
        g.drawText (count, area.removeFromRight (150), juce::Justification::centredRight);

        const auto font = FoundryLookAndFeel::labelFont (12.5f, true, 0.02f);
        const auto sep = juce::String::fromUTF8 ("  \xE2\x80\xBA  ");
        g.setFont (font);

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
            const auto ell = juce::String::fromUTF8 ("\xE2\x80\xA6") + sep;
            g.setColour (t.textDim);
            g.drawText (ell, juce::Rectangle<float> (x, 0.0f, 40.0f, (float) getHeight()), juce::Justification::centredLeft);
            x += juce::GlyphArrangement::getStringWidth (font, ell);
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
class TileBrowser::Body : public juce::Component,
                          public juce::DragAndDropTarget,
                          public juce::FileDragAndDropTarget
{
public:
    explicit Body (TileBrowser& o) : owner (o)
    {
        setWantsKeyboardFocus (true);
    }

    int columns() const noexcept
    {
        if (owner.listView)
            return 1;
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

    void scrollToShow (int i)
    {
        if (i < 0)
            return;
        const auto r = boundsFor (i).expanded (0, kGap);
        const auto view = owner.viewport.getViewArea();
        if (r.getY() < view.getY())
            owner.viewport.setViewPosition (0, r.getY());
        else if (r.getBottom() > view.getBottom())
            owner.viewport.setViewPosition (0, r.getBottom() - view.getHeight());
    }

    //== painting ==============================================================
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
                msg = "No favourites yet - select samples and press F, or right-click > Add to Favourites.";
            else if (owner.processor.getBrowser().isPatternLocation (owner.location))
                msg = "No patterns here yet. Save one from the Generator tab (SAVE PATTERN).";
            else
                msg = "This folder is empty. Drag audio files in from Explorer / Finder, or add some and press RESCAN.";
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

    void paintIcon (juce::Graphics& g, juce::Rectangle<float> icon, const foundry::BrowserEntry& e, bool small)
    {
        const auto& t = owner.theme;

        if (e.type == foundry::BrowserEntry::Type::AddFolder)
        {
            juce::Path p;
            p.addRoundedRectangle (icon.reduced (6.0f), 7.0f);
            const float dashes[] = { 5.0f, 4.0f };
            juce::PathStrokeType (1.2f).createDashedStroke (p, p, dashes, 2);
            g.setColour (t.textDim.withAlpha (0.7f));
            g.fillPath (p);
            g.setFont (FoundryLookAndFeel::labelFont (small ? 16.0f : 30.0f, false));
            g.drawText ("+", icon, juce::Justification::centred);
            return;
        }

        if (e.isFolder())
        {
            const auto c = tintFor (t, e.icon);
            drawFolder (g, icon, c, e.unmapped);
            if (small)
                return;

            auto label = icon.reduced (6.0f, 14.0f).translated (0.0f, 6.0f);
            g.setColour (FoundryLookAndFeel::pillTextColour (c).withAlpha (e.unmapped ? 0.5f : 0.9f));
            if (e.icon == foundry::BrowserEntry::Icon::Favourites)
                drawStar (g, label.getCentre(), 12.0f);
            else
            {
                g.setFont (FoundryLookAndFeel::labelFont (e.unmapped ? 9.5f : 13.0f, true));
                g.drawText (e.unmapped ? (e.custom ? "missing" : "not set") : juce::String (e.count), label, juce::Justification::centred);
            }
            return;
        }

        if (e.isPattern())
        {
            FoundryLookAndFeel::drawInsetWell (g, icon, t, 7.0f);
            if (auto p = owner.patternFor (e.item.path))
            {
                const auto c = p->kind == foundry::PatternKind::Bass ? t.accentAlt : t.accent;
                g.setColour (c.withAlpha (0.35f));
                g.drawRoundedRectangle (icon.reduced (0.5f), 7.0f, 1.0f);
                drawPatternGrid (g, icon.reduced (small ? 3.0f : 7.0f, small ? 3.0f : 18.0f), *p, c);
                if (! small)
                {
                    g.setFont (FoundryLookAndFeel::labelFont (8.5f, true, 0.05f));
                    g.setColour (c);
                    g.drawText (p->kind == foundry::PatternKind::Bass ? "BASS" : "DRUMS",
                                icon.reduced (6.0f, 3.0f).removeFromTop (12.0f), juce::Justification::topLeft);
                    g.setColour (t.textDim);
                    g.drawText (juce::String (p->grid.bars) + (p->grid.bars == 1 ? " bar" : " bars"),
                                icon.reduced (6.0f, 3.0f).removeFromBottom (12.0f), juce::Justification::bottomLeft);
                }
            }
            return;
        }

        const auto c = tintFor (t, e.item.category);
        FoundryLookAndFeel::drawInsetWell (g, icon, t, small ? 4.0f : 7.0f);
        if (small)
        {
            paintWave (g, icon, e, c);
            return;
        }

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

    void paintTile (juce::Graphics& g, int i, juce::Rectangle<int> rInt)
    {
        const auto& t = owner.theme;
        const auto& e = owner.entries[(size_t) i];
        const auto r = rInt.toFloat();
        const bool sel = owner.selection.count (i) != 0;
        const bool hot = i == hover;
        const bool drop = i == dropIndex;

        auto icon = juce::Rectangle<float> ((float) kIcon, (float) kIcon).withCentre ({ r.getCentreX(), r.getY() + 4.0f + kIcon / 2.0f });

        if (sel || hot || drop)
        {
            g.setColour (t.accent.withAlpha (drop ? 0.28f : sel ? 0.18f : 0.08f));
            g.fillRoundedRectangle (r, ui::controlCorner + 2.0f);
            if (sel || drop)
            {
                g.setColour (t.accent.withAlpha (drop ? 1.0f : 0.65f));
                g.drawRoundedRectangle (r.reduced (0.5f), ui::controlCorner + 2.0f, drop ? 2.0f : 1.0f);
            }
        }
        if (i == owner.focus && hasKeyboardFocus (false) && ! sel)
        {
            g.setColour (t.textDim.withAlpha (0.6f));
            g.drawRoundedRectangle (r.reduced (0.5f), ui::controlCorner + 2.0f, 1.0f);
        }

        paintIcon (g, icon, e, false);

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
        const bool sel = owner.selection.count (i) != 0;

        if (sel || i == dropIndex)
            FoundryLookAndFeel::drawActivePill (g, r.toFloat().reduced (0.0f, 2.0f), ui::controlCorner, t.accent, 0.85f);
        else if (i == hover)
        {
            g.setColour (t.accent.withAlpha (0.08f));
            g.fillRoundedRectangle (r.toFloat().reduced (0.0f, 2.0f), ui::controlCorner);
        }

        const auto textCol = sel ? FoundryLookAndFeel::pillTextColour (t.accent) : t.text;
        auto icon = r.removeFromLeft (64).reduced (4, 4).toFloat();

        if (e.isFolder() || e.type == foundry::BrowserEntry::Type::AddFolder)
            paintIcon (g, icon.withSizeKeepingCentre (26.0f, 26.0f).translated (0.0f, -2.0f), e, true);
        else
            paintIcon (g, icon, e, true);

        auto meta = r.removeFromRight (170);
        juce::String right;
        if (e.isFolder())
            right = e.unmapped ? "not set" : juce::String (e.count) + " items";
        else if (e.isPattern())
            right = "Pattern";
        else if (e.isFile())
        {
            right = foundry::categoryName (e.item.category);
            if (auto thumb = owner.processor.getThumbnails().request (e.item.path))
                right += "   " + formatSeconds (thumb->seconds);
        }
        g.setFont (FoundryLookAndFeel::labelFont (10.0f, false));
        g.setColour (sel ? textCol.withAlpha (0.7f) : t.textDim);
        g.drawText (right, meta.reduced (6, 0), juce::Justification::centredRight);

        if (e.isFile() && e.item.favorite)
        {
            g.setColour (sel ? textCol : t.accent);
            drawStar (g, { (float) r.getX() + 6.0f, (float) r.getCentreY() }, 5.0f);
        }

        g.setFont (FoundryLookAndFeel::labelFont (12.0f, sel, 0.01f));
        g.setColour (textCol);
        g.drawText (e.name, r.withTrimmedLeft (16), juce::Justification::centredLeft, true);
    }

    juce::Image dragImageFor (const foundry::BrowserEntry& e, int count)
    {
        juce::Image img (juce::Image::ARGB, kIcon, kIcon, true);
        juce::Graphics g (img);
        auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) kIcon, (float) kIcon);
        g.setColour (owner.theme.panel.withAlpha (0.92f));
        g.fillRoundedRectangle (r, 7.0f);
        paintIcon (g, r, e, false);
        g.setColour (owner.theme.text);
        g.setFont (FoundryLookAndFeel::labelFont (9.5f, true));
        g.drawFittedText (count > 1 ? juce::String (count) + " files" : e.name,
                          r.reduced (4.0f).removeFromBottom (14.0f).toNearestInt(), juce::Justification::centred, 1, 0.7f);
        return img;
    }

    //== mouse =================================================================
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
        grabKeyboardFocus();
        pressIndex = indexAt (e.getPosition());
        dragging = false;

        if (e.mods.isPopupMenu())
        {
            if (pressIndex < 0)
            {
                owner.showBackgroundMenu();
            }
            else
            {
                if (owner.selection.count (pressIndex) == 0 && owner.entries[(size_t) pressIndex].hasPath())
                {
                    owner.selection = { pressIndex };
                    owner.anchor = owner.focus = pressIndex;
                }
                repaint();
                owner.showMenu (pressIndex);
            }
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
        if (! entry.hasPath())
            return;

        // Drag the whole selection if the grabbed tile is part of it.
        juce::StringArray paths;
        if (owner.selection.count (pressIndex) != 0)
            paths = owner.selectedPaths();
        else
            paths.add (entry.item.path);

        dragging = true;
        if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            const juce::Point<int> offset (-kIcon / 2, -kIcon / 2);
            c->startDragging (dnd::samplePrefix + paths.joinIntoString ("\n"), this,
                              juce::ScaledImage (dragImageFor (entry, paths.size())), true, &offset);
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
        if (e.mods.isPopupMenu() || e.getNumberOfClicks() >= 2)
            return;

        if (i < 0)
        {
            if (! e.mods.isCommandDown() && ! e.mods.isShiftDown())
            {
                owner.selection.clear();
                repaint();
                owner.crumbs->repaint();
            }
            return;
        }

        if (i == pressIndex)
            owner.clickTile (i, e.mods);
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int i = indexAt (e.getPosition());
        if (i >= 0 && ! e.mods.isPopupMenu() && ! e.mods.isCommandDown() && ! e.mods.isShiftDown())
            owner.activate (i, true);
    }

    //== keyboard ==============================================================
    bool keyPressed (const juce::KeyPress& key) override
    {
        const int cols = columns();
        const bool shift = key.getModifiers().isShiftDown();

        if (key == juce::KeyPress::leftKey)  { owner.moveFocus (-1, shift);    return true; }
        if (key == juce::KeyPress::rightKey) { owner.moveFocus (+1, shift);    return true; }
        if (key == juce::KeyPress::upKey)    { owner.moveFocus (-cols, shift); return true; }
        if (key == juce::KeyPress::downKey)  { owner.moveFocus (+cols, shift); return true; }

        if (key == juce::KeyPress::returnKey)
        {
            owner.activate (owner.focus, true);
            return true;
        }
        if (key == juce::KeyPress::spaceKey && owner.focus >= 0)
        {
            owner.activate (owner.focus, false);
            return true;
        }
        if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
        {
            owner.deleteSelection();
            return true;
        }
        if (key.getTextCharacter() == 'f' || key.getTextCharacter() == 'F')
        {
            owner.toggleFavouriteOnSelection();
            return true;
        }
        if (key.getModifiers().isCommandDown() && key.getKeyCode() == 'A')
        {
            owner.selectAll();
            return true;
        }
        if (key == juce::KeyPress::escapeKey)
        {
            owner.selection.clear();
            repaint();
            owner.crumbs->repaint();
            return true;
        }
        return false;
    }

    //== drops =================================================================
    int dropFolderAt (juce::Point<int> p) const
    {
        const int i = indexAt (p);
        if (i < 0)
            return -1;
        const auto& e = owner.entries[(size_t) i];
        return e.isFolder() && ! e.unmapped && e.target.isRealFolder() ? i : -1;
    }

    bool isInterestedInDragSource (const SourceDetails& d) override
    {
        return d.description.toString().startsWith (dnd::samplePrefix);
    }

    void itemDragMove (const SourceDetails& d) override
    {
        const int i = dropFolderAt (d.localPosition.toInt());
        if (i != dropIndex) { dropIndex = i; repaint(); }
    }

    void itemDragExit (const SourceDetails&) override
    {
        if (dropIndex != -1) { dropIndex = -1; repaint(); }
    }

    void itemDropped (const SourceDetails& d) override
    {
        const int i = dropFolderAt (d.localPosition.toInt());
        dropIndex = -1;
        repaint();
        if (i >= 0)
            owner.dropFilesOnto (dnd::pathsFrom (d.description.toString()), juce::File (owner.entries[(size_t) i].target.dir));
    }

    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        for (const auto& f : files)
            if (foundry::isSupportedAudioFile (juce::File (f)))
                return true;
        return false;
    }

    void fileDragMove (const juce::StringArray&, int x, int y) override
    {
        const int i = dropFolderAt ({ x, y });
        if (i != dropIndex) { dropIndex = i; repaint(); }
    }

    void fileDragExit (const juce::StringArray&) override
    {
        if (dropIndex != -1) { dropIndex = -1; repaint(); }
    }

    void filesDropped (const juce::StringArray& files, int x, int y) override
    {
        // Import from Explorer / Finder: onto a folder tile, or into the open folder.
        const int i = dropFolderAt ({ x, y });
        dropIndex = -1;
        repaint();

        if (i >= 0)
            owner.dropFilesOnto (files, juce::File (owner.entries[(size_t) i].target.dir));
        else if (owner.location.isRealFolder() && ! owner.processor.getBrowser().isPatternLocation (owner.location))
            owner.dropFilesOnto (files, juce::File (owner.location.dir));
        else
            owner.message ("Open a folder (or drop onto a folder tile) to copy samples into it.", false);
    }

    int hover = -1;
    int dropIndex = -1;

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
    showFolders = processor.getConfig().get().browserShowFolders;
    location = foundry::BrowserModel::root();

    upButton.setTooltip ("Up one folder");
    upButton.onClick = [this] { goUp(); };

    foldersButton.setClickingTogglesState (false);
    foldersButton.setTooltip ("Show folders, or hide them and list every sample in here and below in one flat view");
    foldersButton.onClick = [this] { setShowFolders (! showFolders); };

    tilesButton.setClickingTogglesState (false);
    listButton.setClickingTogglesState (false);
    tilesButton.onClick = [this] { setListView (false); };
    listButton.onClick  = [this] { setListView (true); };
    tilesButton.setTooltip ("Show samples as tiles with waveforms");
    listButton.setTooltip ("Show samples as a compact list");

    addFolderButton.setTooltip ("Add one of your own folders to the browser, or create a new folder here");
    addFolderButton.onClick = [this] { showAddFolderMenu(); };

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

    for (auto* c : std::initializer_list<juce::Component*> { &upButton, crumbs.get(), &search, &foldersButton,
                                                             &tilesButton, &listButton, &addFolderButton,
                                                             &rescanButton, &viewport })
        addAndMakeVisible (c);

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
    for (auto* b : { &upButton, &rescanButton, &addFolderButton })
        b->setColour (juce::TextButton::buttonColourId, t.accent);
    search.setTextToShowWhenEmpty ("Search this folder...", t.textDim);
    setListView (listView);
    setShowFolders (showFolders);
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

void TileBrowser::setShowFolders (bool show)
{
    const bool changed = show != showFolders;
    showFolders = show;
    processor.getConfig().get().browserShowFolders = show;
    foldersButton.setColour (juce::TextButton::buttonColourId, show ? theme.accent : theme.panelEdge);
    foldersButton.setButtonText (show ? "FOLDERS" : "FLAT");
    if (changed)
        refresh();
}

std::shared_ptr<const foundry::PatternFile> TileBrowser::patternFor (const juce::String& path)
{
    if (auto it = patternCache.find (path); it != patternCache.end())
        return it->second;

    auto p = std::make_shared<foundry::PatternFile>();
    std::shared_ptr<const foundry::PatternFile> result;
    if (foundry::PatternFile::load (juce::File (path), *p))
        result = p;
    patternCache[path] = result;
    return result;
}

void TileBrowser::refresh()
{
    const auto keep = selectedPaths();

    entries = processor.getBrowser().list (location, search.getText(), showFolders, truncated);
    patternCache.clear();

    selection.clear();
    focus = anchor = -1;
    for (size_t i = 0; i < entries.size(); ++i)
        if (entries[i].hasPath() && keep.contains (entries[i].item.path))
        {
            selection.insert ((int) i);
            focus = anchor = (int) i;
        }

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
    selection.clear();
    refresh();
    viewport.setViewPosition (0, 0);
}

void TileBrowser::showFolder (const juce::File& dir)
{
    navigateTo (foundry::BrowserModel::folder (dir));
}

void TileBrowser::goUp()
{
    auto path = processor.getBrowser().pathTo (location);
    if (path.size() > 1)
        navigateTo (path[path.size() - 2]);
}

juce::StringArray TileBrowser::selectedPaths() const
{
    juce::StringArray out;
    for (int i : selection)
        if (i >= 0 && i < (int) entries.size() && entries[(size_t) i].hasPath())
            out.add (entries[(size_t) i].item.path);
    return out;
}

void TileBrowser::selectAll()
{
    selection.clear();
    for (size_t i = 0; i < entries.size(); ++i)
        if (entries[i].hasPath())
            selection.insert ((int) i);
    body->repaint();
    crumbs->repaint();
}

std::vector<foundry::LibraryItem> TileBrowser::dicePool() const
{
    std::vector<foundry::LibraryItem> out;

    if (selection.size() > 1)
    {
        for (int i : selection)
            if (entries[(size_t) i].isFile())
                out.push_back (entries[(size_t) i].item);
        return out;
    }

    if (search.getText().trim().isNotEmpty() || ! showFolders)
    {
        for (const auto& e : entries)
            if (e.isFile())
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

void TileBrowser::clickTile (int index, const juce::ModifierKeys& mods)
{
    if (index < 0 || index >= (int) entries.size())
        return;

    const auto& e = entries[(size_t) index];

    if (e.hasPath() && mods.isCommandDown())
    {
        if (selection.count (index) != 0) selection.erase (index);
        else                              selection.insert (index);
        focus = anchor = index;
        body->repaint();
        crumbs->repaint();
        return;
    }

    if (e.hasPath() && mods.isShiftDown() && anchor >= 0)
    {
        selection.clear();
        for (int i = juce::jmin (anchor, index); i <= juce::jmax (anchor, index); ++i)
            if (entries[(size_t) i].hasPath())
                selection.insert (i);
        focus = index;
        body->repaint();
        crumbs->repaint();
        return;
    }

    activate (index, false);
}

void TileBrowser::moveFocus (int delta, bool extend)
{
    if (entries.empty())
        return;

    const int next = juce::jlimit (0, (int) entries.size() - 1, focus < 0 ? 0 : focus + delta);
    focus = next;

    if (extend && anchor >= 0)
    {
        selection.clear();
        for (int i = juce::jmin (anchor, next); i <= juce::jmax (anchor, next); ++i)
            if (entries[(size_t) i].hasPath())
                selection.insert (i);
    }
    else
    {
        selection.clear();
        if (entries[(size_t) next].hasPath())
            selection.insert (next);
        anchor = next;

        // Stepping through a folder auditions as it goes - the fastest way
        // to cull a batch: arrow, listen, F to keep, Delete to bin.
        if (entries[(size_t) next].isFile())
        {
            juce::String err;
            if (! processor.previewItem (entries[(size_t) next].item, err))
                message (err, true);
        }
    }

    body->scrollToShow (next);
    body->repaint();
    crumbs->repaint();
}

void TileBrowser::activate (int index, bool doubleClick)
{
    if (index < 0 || index >= (int) entries.size())
        return;

    const auto e = entries[(size_t) index];

    if (e.type == foundry::BrowserEntry::Type::AddFolder)
    {
        addFolderFromDisk();
        return;
    }

    if (e.isFolder())
    {
        if (e.unmapped)
        {
            message (e.custom ? "That folder can't be found any more - right-click it to remove it from the browser."
                              : "The " + e.name + " folder isn't set yet - choose it in Settings (or create the recommended structure).",
                     true);
            return;
        }
        navigateTo (e.target);
        return;
    }

    selection = { index };
    focus = anchor = index;
    body->repaint();
    crumbs->repaint();

    const juce::File file (e.item.path);

    if (e.isPattern())
    {
        if (doubleClick && onOpenPattern)
            onOpenPattern (file);
        return;
    }

    if (doubleClick)
    {
        if (FoundryAudioProcessor::isEditableRiff (file) && onOpenRiff)
            onOpenRiff (file);
        else if (FoundryAudioProcessor::hasPattern (file) && onOpenPattern)
            onOpenPattern (file);
        else if (onLoadIntoSlot)
            onLoadIntoSlot (-1, file);
        return;
    }

    juce::String err;
    if (! processor.previewItem (e.item, err))
        message (err, true);
}

void TileBrowser::toggleFavouriteOnSelection()
{
    std::vector<foundry::LibraryItem> items;
    for (int i : selection)
        if (entries[(size_t) i].isFile())
            items.push_back (entries[(size_t) i].item);
    if (items.empty())
        return;

    // If any of them isn't a favourite yet, star them all; otherwise unstar.
    const bool makeFav = std::any_of (items.begin(), items.end(), [] (const auto& it) { return ! it.favorite; });
    juce::String err;
    for (const auto& it : items)
        processor.getLibrary().setFavorite (it.id, makeFav, err);
    refresh();
}

void TileBrowser::deleteSelection()
{
    const auto paths = selectedPaths();
    if (paths.isEmpty())
        return;

    juce::StringArray managed;
    int own = 0;
    for (const auto& p : paths)
    {
        if (processor.isManagedFile (juce::File (p)))
            managed.add (p);
        else
            ++own;
    }

    if (managed.isEmpty())
    {
        message ("Foundry only deletes files it made - Batch Generated loops, My Riffs and Patterns. "
                 "Your own samples are never deleted from here.", false);
        return;
    }

    juce::String text = "This removes " + juce::String (managed.size()) + (managed.size() == 1 ? " file" : " files")
                      + " from disk. It can't be undone.";
    if (own > 0)
        text << "\n\n" << own << " of your own samples in the selection will be left alone.";

    auto options = juce::MessageBoxOptions()
                       .withIconType (juce::MessageBoxIconType::WarningIcon)
                       .withTitle ("Delete " + (managed.size() == 1 ? juce::File (managed[0]).getFileName()
                                                                    : juce::String (managed.size()) + " files") + "?")
                       .withMessage (text)
                       .withButton ("Delete")
                       .withButton ("Cancel")
                       .withAssociatedComponent (this);

    juce::Component::SafePointer<TileBrowser> safe (this);
    juce::AlertWindow::showAsync (options, [safe, managed] (int result)
    {
        if (safe == nullptr || result != 1)
            return;
        int skipped = 0;
        const int n = safe->processor.deleteFiles (managed, skipped);
        safe->selection.clear();
        safe->refresh();
        safe->message ("Deleted " + juce::String (n) + (n == 1 ? " file." : " files."), false);
        if (safe->onFilesChanged)
            safe->onFilesChanged();
    });
}

void TileBrowser::cullUnfavourited()
{
    // Everything in here (and below) that Foundry made and you didn't star.
    juce::StringArray doomed;
    int kept = 0;
    for (const auto& it : processor.getBrowser().filesUnder (location))
    {
        if (! processor.isManagedFile (juce::File (it.path)))
            continue;
        if (it.favorite)
            ++kept;
        else
            doomed.add (it.path);
    }

    if (doomed.isEmpty())
    {
        message (kept > 0 ? "Nothing to cull - every generated file in here is a favourite."
                          : "Nothing to cull here. This works in Batch Generated and My Riffs.", false);
        return;
    }

    auto options = juce::MessageBoxOptions()
                       .withIconType (juce::MessageBoxIconType::WarningIcon)
                       .withTitle ("Keep favourites, delete the rest?")
                       .withMessage ("Deletes " + juce::String (doomed.size()) + " generated files in "
                                     + location.title + " that aren't favourites, and keeps "
                                     + juce::String (kept) + " favourites. It can't be undone.")
                       .withButton ("Delete " + juce::String (doomed.size()))
                       .withButton ("Cancel")
                       .withAssociatedComponent (this);

    juce::Component::SafePointer<TileBrowser> safe (this);
    juce::AlertWindow::showAsync (options, [safe, doomed] (int result)
    {
        if (safe == nullptr || result != 1)
            return;
        int skipped = 0;
        const int n = safe->processor.deleteFiles (doomed, skipped);
        safe->refresh();
        safe->message ("Deleted " + juce::String (n) + " files - your favourites are still here.", false);
        if (safe->onFilesChanged)
            safe->onFilesChanged();
    });
}

void TileBrowser::dropFilesOnto (const juce::StringArray& paths, const juce::File& dir)
{
    int copied = 0;
    juce::String err;
    const int n = processor.moveOrCopyFiles (paths, dir, copied, err);
    refresh();

    if (n == 0)
    {
        message (err.isNotEmpty() ? err : "Those files are already in " + dir.getFileName() + ".", err.isNotEmpty());
        return;
    }

    const int moved = n - copied;
    juce::StringArray parts;
    if (moved > 0)  parts.add ("moved " + juce::String (moved));
    if (copied > 0) parts.add ("copied " + juce::String (copied));
    message (parts.joinIntoString (", ").substring (0, 1).toUpperCase() + parts.joinIntoString (", ").substring (1)
                 + (n == 1 ? " file" : " files") + " to " + dir.getFileName() + "."
                 + (copied > 0 && moved == 0 ? " (Your own samples are copied, never moved.)" : ""),
             err.isNotEmpty());
    if (onFilesChanged)
        onFilesChanged();
}

void TileBrowser::addFolderFromDisk()
{
    chooser = std::make_unique<juce::FileChooser> ("Add a folder to the browser",
                                                   processor.getConfig().root(), "*");
    juce::Component::SafePointer<TileBrowser> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [safe] (const juce::FileChooser& fc)
    {
        if (safe == nullptr)
            return;
        const auto dir = fc.getResult();
        if (! dir.isDirectory())
            return;
        juce::String err;
        safe->processor.addUserFolder (dir, err);
        safe->navigateTo (foundry::BrowserModel::root());
        safe->message (err.isNotEmpty() ? err : "Added " + dir.getFileName() + ". Indexing its samples...", false);
    });
}

void TileBrowser::createFolderHere()
{
    juce::Component::SafePointer<TileBrowser> safe (this);
    askForName ("New folder", location.isRealFolder() ? "Create a folder inside " + location.title + ":"
                                                      : "Create a new top-level folder in your library:",
                "New Folder", [safe] (const juce::String& name)
    {
        if (safe == nullptr)
            return;

        auto& p = safe->processor;
        const auto stem = foundry::legalStem (name);

        if (safe->location.isRealFolder())
        {
            auto dir = juce::File (safe->location.dir).getChildFile (stem);
            if (! dir.createDirectory())
            {
                safe->message ("Could not create " + dir.getFullPathName(), true);
                return;
            }
            safe->refresh();
            return;
        }

        // At the top level a new folder becomes one of your own browser folders.
        auto base = p.getConfig().root().isDirectory() ? p.getConfig().root() : foundry::appDataDir().getChildFile ("Folders");
        auto dir = base.getChildFile (stem);
        if (! dir.createDirectory())
        {
            safe->message ("Could not create " + dir.getFullPathName(), true);
            return;
        }
        juce::String err;
        p.addUserFolder (dir, err);
        safe->navigateTo (foundry::BrowserModel::root());
    });
}

void TileBrowser::showAddFolderMenu()
{
    juce::PopupMenu m;
    m.addItem (1, "Add a folder from disk...");
    const bool canCreate = location.kind == foundry::BrowserLocation::Kind::Root || location.isRealFolder();
    m.addItem (2, location.isRealFolder() ? "New folder inside " + location.title + "..." : "New folder...", canCreate);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addFolderButton), [this] (int r)
    {
        if (r == 1) addFolderFromDisk();
        if (r == 2) createFolderHere();
    });
}

void TileBrowser::showBackgroundMenu()
{
    juce::PopupMenu m;
    m.addItem (1, "Select all");
    m.addItem (2, showFolders ? "Hide folders (flat view)" : "Show folders");
    m.addSeparator();
    m.addItem (3, "Keep favourites, delete the rest...", location.kind != foundry::BrowserLocation::Kind::Root);
    m.addSeparator();
    m.addItem (4, "New folder...", location.kind == foundry::BrowserLocation::Kind::Root || location.isRealFolder());
    m.addItem (5, "Add a folder from disk...");
    m.addItem (6, "Show this folder in Explorer / Finder", location.isRealFolder());
    m.addItem (7, "Rescan library");
    m.showMenuAsync (juce::PopupMenu::Options(), [this] (int r)
    {
        if (r == 1) selectAll();
        if (r == 2) setShowFolders (! showFolders);
        if (r == 3) cullUnfavourited();
        if (r == 4) createFolderHere();
        if (r == 5) addFolderFromDisk();
        if (r == 6) juce::File (location.dir).revealToUser();
        if (r == 7) rescanButton.onClick();
    });
}

void TileBrowser::showMenu (int index)
{
    if (index < 0 || index >= (int) entries.size())
        return;

    const auto e = entries[(size_t) index];
    juce::PopupMenu m;

    if (e.type == foundry::BrowserEntry::Type::AddFolder)
    {
        showAddFolderMenu();
        return;
    }

    if (e.isFolder())
    {
        m.addItem (1, "Open", ! e.unmapped);
        m.addItem (2, "Show in Explorer / Finder", ! e.unmapped && e.target.dir.isNotEmpty());
        if (e.custom)
        {
            m.addSeparator();
            m.addItem (3, "Remove from browser (keeps the files)");
        }
        m.showMenuAsync (juce::PopupMenu::Options(), [this, e] (int r)
        {
            if (r == 1) navigateTo (e.target);
            if (r == 2) juce::File (e.target.dir).revealToUser();
            if (r == 3)
            {
                processor.removeUserFolder (juce::File (e.target.dir));
                refresh();
                message ("Removed " + e.name + " from the browser. The folder and its files are untouched.", false);
            }
        });
        return;
    }

    const juce::File file (e.item.path);
    const auto paths = selectedPaths();
    const int n = paths.size();
    int managed = 0;
    for (const auto& p : paths)
        if (processor.isManagedFile (juce::File (p)))
            ++managed;

    if (e.isPattern())
    {
        m.addItem (20, "Open in Generator");
    }
    else
    {
        const bool riffFile = FoundryAudioProcessor::isEditableRiff (file);
        const bool hasPat = FoundryAudioProcessor::hasPattern (file);

        m.addItem (1, "Preview");
        if (riffFile)
            m.addItem (2, "Open in Riff Maker (edit)");
        if (hasPat)
        {
            m.addItem (20, "Edit pattern in Generator (keeps these sounds)");
            m.addItem (21, "Save its pattern to Patterns...");
        }
        m.addItem (3, n > 1 ? "Load " + juce::String (n) + " into the Riff Maker" : juce::String ("Load into next empty slot"));

        juce::PopupMenu slots;
        for (int s = 0; s < foundry::kRiffSlots; ++s)
            slots.addItem (100 + s, "Slot " + juce::String (s + 1));
        m.addSubMenu ("Load into slot", slots);

        m.addSeparator();
        m.addItem (4, e.item.favorite ? "Remove from Favourites" : (n > 1 ? "Add " + juce::String (n) + " to Favourites"
                                                                           : juce::String ("Add to Favourites")));
        if (foundry::isBassLoopCategory (e.item.category))
            m.addItem (5, "Send to Splicer");
        if (isDrumOneShot (e.item.category))
            m.addItem (6, "Lock in Drum Generator");
        if (! foundry::isGeneratedCategory (e.item.category) && ! isDrumOneShot (e.item.category))
        {
            juce::PopupMenu src;
            for (int r = 0; r < 4; ++r)
                src.addItem (200 + r, "Source " + juce::String::charToString ((juce::juce_wchar) ('A' + r)));
            m.addSubMenu ("Lock as bass source", src);
        }
        m.addItem (7, "Show in Explorer / Finder");
    }

    m.addSeparator();
    m.addItem (8, n > 1 ? "Delete " + juce::String (managed) + " selected..." : juce::String ("Delete file..."), managed > 0);
    if (managed == 0)
        m.addItem (9, "(Only files Foundry made can be deleted here)", false);

    m.showMenuAsync (juce::PopupMenu::Options(), [this, e, file, paths] (int r)
    {
        if (r == 1)
        {
            juce::String err;
            if (! processor.previewItem (e.item, err))
                message (err, true);
        }
        else if (r == 2 && onOpenRiff)       onOpenRiff (file);
        else if (r == 20 && onOpenPattern)   onOpenPattern (file);
        else if (r == 21)
        {
            foundry::PatternFile pf;
            juce::String err;
            if (! processor.loadPattern (file, pf, err))
            {
                message (err, true);
                return;
            }
            pf.samples = {};   // a library pattern is just the groove
            juce::Component::SafePointer<TileBrowser> safe (this);
            askForName ("Save pattern", "Name this pattern:", file.getFileNameWithoutExtension(),
                        [safe, pf] (const juce::String& name)
            {
                if (safe == nullptr)
                    return;
                juce::String e2;
                const auto f = safe->processor.savePattern (pf, name, e2);
                safe->message (f.existsAsFile() ? "Saved " + f.getFileNameWithoutExtension() + " to Patterns > "
                                                      + foundry::patternKindName (pf.kind) + "." : e2,
                               ! f.existsAsFile());
            });
        }
        else if (r == 3 && onLoadIntoSlot)
        {
            for (const auto& p : paths)
                onLoadIntoSlot (-1, juce::File (p));
        }
        else if (r >= 100 && r < 100 + foundry::kRiffSlots && onLoadIntoSlot)
            onLoadIntoSlot (r - 100, file);
        else if (r == 4)
        {
            if (e.item.favorite)
            {
                juce::String err;
                processor.getLibrary().setFavorite (e.item.id, false, err);
                refresh();
            }
            else
                toggleFavouriteOnSelection();
        }
        else if (r == 5 && onSendToSplicer)  onSendToSplicer (e.item);
        else if (r == 6 && onLockInDrums)    onLockInDrums (e.item);
        else if (r >= 200 && r < 204 && onLockBassSource) onLockBassSource (r - 200, e.item);
        else if (r == 7)                     file.revealToUser();
        else if (r == 8)                     deleteSelection();
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
    rescanButton.setBounds (top.removeFromRight (70));
    top.removeFromRight (6);
    addFolderButton.setBounds (top.removeFromRight (78));
    top.removeFromRight (10);
    listButton.setBounds (top.removeFromRight (44));
    top.removeFromRight (2);
    tilesButton.setBounds (top.removeFromRight (50));
    top.removeFromRight (6);
    foldersButton.setBounds (top.removeFromRight (70));
    top.removeFromRight (10);
    search.setBounds (top.removeFromRight (juce::jlimit (140, 260, top.getWidth() / 3)));
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
