#include "BrowserList.h"

namespace pt
{

BrowserList::BrowserList()
{
    viewport.setViewedComponent (&body, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
}

void BrowserList::setTheme (const Theme& t)
{
    theme = t;
    body.repaint();
}

void BrowserList::setItems (std::vector<foundry::LibraryItem> next)
{
    items = std::move (next);
    body.setSize (juce::jmax (1, viewport.getWidth() - viewport.getScrollBarThickness()),
                  juce::jmax (viewport.getHeight(), (int) items.size() * kRowH));
    body.repaint();
}

void BrowserList::setSelectedPath (const juce::String& path)
{
    selectedPath = path;
    body.repaint();
}

const foundry::LibraryItem* BrowserList::getSelected() const
{
    for (const auto& it : items)
        if (it.path == selectedPath)
            return &it;
    return nullptr;
}

void BrowserList::resized()
{
    viewport.setBounds (getLocalBounds());
    body.setSize (juce::jmax (1, viewport.getWidth() - viewport.getScrollBarThickness()),
                  juce::jmax (getHeight(), (int) items.size() * kRowH));
}

int BrowserList::Body::rowAt (juce::Point<int> p) const noexcept
{
    if (p.y < 0)
        return -1;
    const int r = p.y / kRowH;
    return r >= 0 && r < (int) owner.items.size() ? r : -1;
}

void BrowserList::Body::paint (juce::Graphics& g)
{
    const auto& t = owner.theme;
    const int n = (int) owner.items.size();

    if (n == 0)
    {
        g.setColour (t.textDim);
        g.setFont (FoundryLookAndFeel::labelFont (13.0f, false));
        g.drawFittedText ("Nothing indexed in this category yet.\nMap folders in Settings, then Refresh Library.",
                          getLocalBounds().reduced (24), juce::Justification::centred, 4);
        return;
    }

    const auto view = owner.viewport.getViewArea();
    const int first = juce::jmax (0, view.getY() / kRowH - 1);
    const int last  = juce::jmin (n, (view.getBottom() / kRowH) + 2);

    for (int i = first; i < last; ++i)
    {
        auto row = juce::Rectangle<int> (0, i * kRowH, getWidth(), kRowH);
        const auto& it = owner.items[(size_t) i];
        const bool sel = it.path == owner.selectedPath;
        const bool hot = i == hover;

        if (sel)
            FoundryLookAndFeel::drawActivePill (g, row.toFloat().reduced (4.0f, 3.0f),
                                                ui::controlCorner, t.accent, 0.88f);
        else if (hot)
        {
            g.setColour (t.accent.withAlpha (0.10f));
            g.fillRoundedRectangle (row.toFloat().reduced (4.0f, 3.0f), ui::controlCorner);
        }

        if (i % 2 == 0 && ! sel)
        {
            g.setColour (juce::Colours::white.withAlpha (0.015f));
            g.fillRect (row);
        }

        auto star = row.removeFromLeft (28).toFloat();
        g.setFont (FoundryLookAndFeel::labelFont (12.0f, true));
        g.setColour (it.favorite ? t.accent : (sel ? FoundryLookAndFeel::pillTextColour (t.accent).withAlpha (0.55f)
                                                   : t.textDim.withAlpha (0.55f)));
        g.drawText (it.favorite ? juce::String::fromUTF8 ("\xE2\x98\x85") : juce::String::fromUTF8 ("\xE2\x98\x86"),
                    star.toNearestInt(), juce::Justification::centred);

        auto meta = row.removeFromRight (118);
        g.setFont (FoundryLookAndFeel::labelFont (10.0f, false));
        g.setColour (sel ? FoundryLookAndFeel::pillTextColour (t.accent).withAlpha (0.75f) : t.textDim);
        juce::String right;
        if (it.bpm > 1.0)
            right += juce::String (it.bpm, 0) + " BPM";
        if (it.bars > 0)
            right += (right.isEmpty() ? "" : "  ") + juce::String (it.bars) + " bar";
        g.drawText (right, meta.reduced (6, 0), juce::Justification::centredRight, true);

        g.setFont (FoundryLookAndFeel::labelFont (12.5f, sel, 0.01f));
        g.setColour (sel ? FoundryLookAndFeel::pillTextColour (t.accent) : t.text);
        g.drawText (it.filename, row.reduced (4, 0), juce::Justification::centredLeft, true);
    }
}

void BrowserList::Body::mouseMove (const juce::MouseEvent& e)
{
    const int r = rowAt (e.getPosition());
    if (r != hover)
    {
        hover = r;
        repaint();
    }
}

void BrowserList::Body::mouseExit (const juce::MouseEvent&)
{
    hover = -1;
    dragging = false;
    repaint();
}

void BrowserList::Body::mouseDown (const juce::MouseEvent& e)
{
    dragRow = rowAt (e.getPosition());
    dragStart = e.getPosition();
    dragging = false;
}

void BrowserList::Body::mouseDrag (const juce::MouseEvent& e)
{
    if (dragRow < 0 || dragRow >= (int) owner.items.size())
        return;

    if (e.getDistanceFromDragStart() < 6)
        return;

    if (dragging)
        return;

    dragging = true;
    const auto& it = owner.items[(size_t) dragRow];
    const juce::File f (it.path);
    if (! f.existsAsFile())
        return;

    if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
        c->performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
}

void BrowserList::Body::mouseUp (const juce::MouseEvent& e)
{
    if (dragging)
    {
        dragging = false;
        return;
    }

    const int r = rowAt (e.getPosition());
    if (r < 0 || r >= (int) owner.items.size())
        return;

    const auto& it = owner.items[(size_t) r];

    if (e.mods.isPopupMenu())
    {
        owner.showMenu (r, e.getScreenPosition());
        return;
    }

    if (e.x < 28)
    {
        if (owner.onToggleFavorite)
            owner.onToggleFavorite (it);
        return;
    }

    owner.selectedPath = it.path;
    if (owner.onSelectionChanged)
        owner.onSelectionChanged (it);
    if (owner.onPreview)
        owner.onPreview (it);
    repaint();
}

void BrowserList::showMenu (int row, juce::Point<int> screen)
{
    if (row < 0 || row >= (int) items.size())
        return;

    const auto it = items[(size_t) row];
    juce::PopupMenu m;
    m.addItem (1, it.favorite ? "Remove Favourite" : "Add to Favourites");
    m.addItem (2, "Reveal in Explorer / Finder");
    if (foundry::isGeneratedCategory (it.category))
        m.addItem (3, "Delete generated file");
    if (foundry::isBassLoopCategory (it.category))
        m.addItem (4, "Send to Riff Splicer");
    if (it.category == foundry::Category::OneShotKick
        || it.category == foundry::Category::OneShotClap
        || it.category == foundry::Category::OneShotSnare
        || it.category == foundry::Category::OneShotHat
        || it.category == foundry::Category::OneShotRide)
        m.addItem (5, "Lock this sample in Drums");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea ({ screen.x, screen.y, 1, 1 }),
                     [this, it] (int r)
                     {
                         if (r == 1 && onToggleFavorite) onToggleFavorite (it);
                         if (r == 2 && onReveal)         onReveal (it);
                         if (r == 3 && onDelete)         onDelete (it);
                         if (r == 4 && onSendToSplicer)  onSendToSplicer (it);
                         if (r == 5 && onLockFrom)       onLockFrom (it);
                     });
}

} // namespace pt
