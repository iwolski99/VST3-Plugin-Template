#include "RiffMakerPanel.h"
#include <iterator>

namespace pt
{

namespace
{
    // Lengths the ◂ ▸ arrows step through, in 16th notes.
    const int kLengthSteps[] = { 1, 2, 3, 4, 6, 8, 12, 16 };

    int nextLength (int current, int dir)
    {
        if (dir > 0)
        {
            for (int v : kLengthSteps)
                if (v > current) return v;
            return kLengthSteps[std::size (kLengthSteps) - 1];
        }
        for (int i = (int) std::size (kLengthSteps) - 1; i >= 0; --i)
            if (kLengthSteps[i] < current) return kLengthSteps[i];
        return kLengthSteps[0];
    }

    float gainToDb (float g)   { return juce::Decibels::gainToDecibels (g, -60.0f); }
    float dbToGain (float db)  { return juce::Decibels::decibelsToGain (db, -60.0f); }

    juce::String gainText (float g)
    {
        const auto db = gainToDb (g);
        if (db <= -59.0f) return "-inf dB";
        return (db > 0.05f ? "+" : "") + juce::String (db, 1) + " dB";
    }

    juce::String offsetText (double seconds)
    {
        if (seconds < 0.0005) return "start 0 ms";
        if (seconds < 1.0)    return "start +" + juce::String (juce::roundToInt (seconds * 1000.0)) + " ms";
        return "start +" + juce::String (seconds, 2) + " s";
    }
}

//==============================================================================
class RiffMakerPanel::SlotView : public juce::Component,
                                 public juce::DragAndDropTarget,
                                 public juce::FileDragAndDropTarget
{
public:
    SlotView (RiffMakerPanel& o, FoundryAudioProcessor& p, int i)
        : owner (o), processor (p), index (i)
    {
        setRepaintsOnMouseActivity (true);
    }

    void setTheme (const Theme& t) { theme = t; repaint(); }
    void setPlaying (bool p)       { if (p != playing) { playing = p; repaint(); } }

    const foundry::RiffSlot& slot() const { return processor.getSketch().slots[(size_t) index]; }

    std::shared_ptr<const foundry::SourceSample> sample()
    {
        const auto& s = slot();
        if (s.path != cachedPath)
        {
            cachedPath = s.path;
            cached.reset();
            if (s.path.isNotEmpty())
            {
                juce::String err;
                cached = processor.getSamples().get (juce::File (s.path), err);
            }
        }
        return cached;
    }

    //== layout ================================================================
    void resized() override
    {
        auto b = getLocalBounds().reduced (6);
        topBar = b.removeFromTop (22);
        bottomBar = b.removeFromBottom (24);
        frame = b.reduced (0, 3);

        auto t = topBar;
        clearRect = t.removeFromRight (20);
        t.removeFromRight (2);
        lockRect = t.removeFromRight (22);

        auto bb = bottomBar;
        lenDownRect = bb.removeFromLeft (20);
        lenTextRect = bb.removeFromLeft (46);
        lenUpRect   = bb.removeFromLeft (20);
        gainRect    = bb.removeFromRight (64);
        offsetRect  = bb;
    }

    //== painting ==============================================================
    void paint (juce::Graphics& g) override
    {
        const auto& t = theme;
        const auto& s = slot();
        const auto bounds = getLocalBounds().toFloat().reduced (1.0f);

        FoundryLookAndFeel::drawPanel (g, bounds, t, ui::panelCorner);

        if (playing && ! s.isEmpty())
        {
            FoundryLookAndFeel::bloom (g, frame.getCentre().toFloat(), (float) frame.getWidth() * 0.6f, t.accent, 0.16f);
            g.setColour (t.accent);
            g.drawRoundedRectangle (bounds.reduced (0.5f), ui::panelCorner, 2.0f);
        }
        else if (dropHover)
        {
            g.setColour (t.accent.withAlpha (0.9f));
            g.drawRoundedRectangle (bounds.reduced (0.5f), ui::panelCorner, 2.0f);
        }
        else if (owner.getSelectedSlot() == index)
        {
            g.setColour (t.text.withAlpha (0.55f));
            g.drawRoundedRectangle (bounds.reduced (0.5f), ui::panelCorner, 1.5f);
        }

        paintTopBar (g, s);
        paintFrame (g, s);
        paintBottomBar (g, s);
    }

    void paintTopBar (juce::Graphics& g, const foundry::RiffSlot& s)
    {
        const auto& t = theme;
        auto bar = topBar;

        auto badge = bar.removeFromLeft (22).toFloat().reduced (1.0f);
        if (s.isEmpty())
        {
            g.setColour (t.panelEdge);
            g.drawEllipse (badge.reduced (0.5f), 1.2f);
            g.setColour (t.textDim);
        }
        else
        {
            g.setColour (t.accent);
            g.fillEllipse (badge);
            g.setColour (FoundryLookAndFeel::pillTextColour (t.accent));
        }
        g.setFont (FoundryLookAndFeel::labelFont (11.0f, true));
        g.drawText (juce::String (index + 1), badge, juce::Justification::centred);

        bar.removeFromLeft (6);
        bar.removeFromRight (46);
        g.setFont (FoundryLookAndFeel::labelFont (12.0f, ! s.isEmpty(), 0.01f));
        const bool missing = ! s.isEmpty() && ! juce::File (s.path).existsAsFile();
        g.setColour (s.isEmpty() ? t.textDim : (missing ? t.accentAlt : t.text));
        const int holder = processor.getSketch().heldBy (index);
        g.drawText (s.isEmpty() ? (holder >= 0 ? "Held from slot " + juce::String (holder + 1) : juce::String ("Silent"))
                                : s.displayName() + (missing ? "  (missing)" : ""),
                    bar, juce::Justification::centredLeft, true);

        // lock
        {
            auto r = lockRect.toFloat().reduced (3.0f, 2.0f);
            auto body = r.removeFromBottom (r.getHeight() * 0.55f);
            juce::Path shackle;
            shackle.addCentredArc (body.getCentreX(), body.getY(), body.getWidth() * 0.32f, r.getHeight() * 0.9f,
                                   0.0f, -juce::MathConstants<float>::halfPi, s.locked ? juce::MathConstants<float>::halfPi
                                                                                         : juce::MathConstants<float>::halfPi * 0.2f, true);
            const auto c = s.locked ? t.accent : t.textDim.withAlpha (isMouseOver() ? 0.9f : 0.45f);
            g.setColour (c);
            g.strokePath (shackle, juce::PathStrokeType (1.6f));
            if (s.locked)
                g.fillRoundedRectangle (body, 2.0f);
            else
                g.drawRoundedRectangle (body, 2.0f, 1.4f);
        }

        // clear (makes the slot silent)
        if (! s.isEmpty() || processor.getSketch().heldBy (index) >= 0)
        {
            g.setColour (t.textDim.withAlpha (isMouseOver() ? 0.9f : 0.4f));
            auto r = clearRect.toFloat().withSizeKeepingCentre (8.0f, 8.0f);
            g.drawLine ({ r.getTopLeft(), r.getBottomRight() }, 1.6f);
            g.drawLine ({ r.getTopRight(), r.getBottomLeft() }, 1.6f);
        }
    }

    void paintFrame (juce::Graphics& g, const foundry::RiffSlot& s)
    {
        const auto& t = theme;
        const auto f = frame.toFloat();
        FoundryLookAndFeel::drawInsetWell (g, f, t, 5.0f);

        if (s.isEmpty())
        {
            g.setColour (t.panelEdge.withAlpha (0.8f));
            const float dashes[] = { 5.0f, 4.0f };
            juce::Path p;
            p.addRoundedRectangle (f.reduced (4.0f), 5.0f);
            juce::PathStrokeType (1.0f).createDashedStroke (p, p, dashes, 2);
            g.fillPath (p);
            const int holder = processor.getSketch().heldBy (index);
            if (holder >= 0)
            {
                g.setColour (t.accent.withAlpha (0.10f));
                g.fillRoundedRectangle (f.reduced (4.0f), 5.0f);
                g.setColour (t.text.withAlpha (0.8f));
                g.setFont (FoundryLookAndFeel::labelFont (11.5f, false));
                g.drawFittedText ("Slot " + juce::String (holder + 1) + " rings on through here\nPress Delete to cut it to silence",
                                  frame.reduced (8), juce::Justification::centred, 3);
                return;
            }

            g.setColour (t.textDim);
            g.setFont (FoundryLookAndFeel::labelFont (11.5f, false));
            g.drawFittedText ("Silence\nDrop a sample here", frame.reduced (8), juce::Justification::centred, 2);
            return;
        }

        auto smp = sample();
        if (smp == nullptr || smp->audio.getNumSamples() == 0)
        {
            g.setColour (t.accentAlt);
            g.setFont (FoundryLookAndFeel::labelFont (11.0f, false));
            g.drawFittedText ("Sample not found.\nDrop a new one, or rescan the library.", frame.reduced (8),
                              juce::Justification::centred, 3);
            return;
        }

        const auto& sketch = processor.getSketch();
        const double bpm = processor.getRiffBpm();
        const double gridSec  = foundry::stepsToSeconds (sketch.gridSteps, bpm);
        const double sliceSec = foundry::stepsToSeconds (s.lengthSteps, bpm);
        const double pxPerSec = f.getWidth() / juce::jmax (1.0e-3, gridSec);
        const float sliceEndX = f.getX() + (float) juce::jmin ((double) f.getWidth(), sliceSec * pxPerSec);

        const auto& audio = smp->audio;
        const double sr = smp->sampleRate;
        const int total = audio.getNumSamples();
        const float mid = f.getCentreY();
        const float halfH = f.getHeight() * 0.44f;
        const float gainVis = juce::jlimit (0.25f, 1.6f, s.gain);

        g.saveState();
        g.reduceClipRegion (frame.reduced (1));

        for (int x = 0; x < frame.getWidth(); ++x)
        {
            const double t0 = s.offsetSeconds + (double) x / pxPerSec;
            const int s0 = (int) (t0 * sr);
            const int s1 = juce::jmin (total, (int) ((t0 + 1.0 / pxPerSec) * sr) + 1);
            if (s0 >= total)
                break;

            float mag = 0.0f;
            for (int ch = 0; ch < audio.getNumChannels(); ++ch)
                mag = juce::jmax (mag, audio.getMagnitude (ch, s0, juce::jmax (1, s1 - s0)));

            const float px = f.getX() + (float) x;
            const float h = juce::jmax (0.5f, juce::jmin (1.0f, mag * gainVis) * halfH);
            const bool inSlice = px < sliceEndX;
            g.setColour (inSlice ? t.accent.withAlpha (0.55f + 0.4f * juce::jmin (1.0f, mag))
                                 : t.textDim.withAlpha (0.22f));
            g.fillRect (px, mid - h, 1.0f, h * 2.0f);
        }

        g.restoreState();

        // Where the slice ends inside the grid cell.
        if (sliceEndX < f.getRight() - 1.0f)
        {
            g.setColour (t.accentAlt.withAlpha (0.85f));
            g.fillRect (sliceEndX, f.getY() + 3.0f, 1.5f, f.getHeight() - 6.0f);
        }
        else if (s.lengthSteps > sketch.gridSteps)
        {
            g.setColour (t.text.withAlpha (0.75f));
            g.setFont (FoundryLookAndFeel::labelFont (9.0f, true));
            g.drawText (juce::String::fromUTF8 ("rings on \xE2\x86\x92"), f.reduced (6.0f, 4.0f), juce::Justification::topRight);
        }

        // Overview: where in the whole file this slot is playing from.
        {
            const double fileSec = juce::jmax (1.0e-3, smp->seconds());
            auto strip = f.reduced (6.0f, 0.0f).removeFromBottom (5.0f).translated (0.0f, -3.0f);
            g.setColour (t.panelEdge.withAlpha (0.55f));
            g.fillRoundedRectangle (strip, 2.0f);
            const float x0 = strip.getX() + strip.getWidth() * (float) juce::jlimit (0.0, 1.0, s.offsetSeconds / fileSec);
            const float x1 = strip.getX() + strip.getWidth() * (float) juce::jlimit (0.0, 1.0, (s.offsetSeconds + sliceSec) / fileSec);
            g.setColour (t.accent.withAlpha (0.9f));
            g.fillRoundedRectangle (juce::Rectangle<float> (x0, strip.getY(), juce::jmax (2.0f, x1 - x0), strip.getHeight()), 2.0f);
        }

        if (slipping)
        {
            g.setColour (t.text);
            g.setFont (FoundryLookAndFeel::labelFont (10.0f, true));
            g.drawText (offsetText (s.offsetSeconds), f.reduced (6.0f, 4.0f), juce::Justification::topLeft);
        }
    }

    void paintBottomBar (juce::Graphics& g, const foundry::RiffSlot& s)
    {
        const auto& t = theme;
        const bool hasSample = ! s.isEmpty();

        g.setFont (FoundryLookAndFeel::labelFont (13.0f, true));
        g.setColour (t.textDim.brighter (0.2f));
        g.drawText (juce::String::fromUTF8 ("\xE2\x97\x82"), lenDownRect, juce::Justification::centred);
        g.drawText (juce::String::fromUTF8 ("\xE2\x96\xB8"), lenUpRect, juce::Justification::centred);

        auto pill = lenTextRect.toFloat().reduced (1.0f, 3.0f);
        g.setColour (t.accent.withAlpha (0.14f));
        g.fillRoundedRectangle (pill, ui::controlCorner);
        g.setColour (t.text);
        g.setFont (FoundryLookAndFeel::labelFont (11.5f, true));
        g.drawText (foundry::stepsName (s.lengthSteps), pill, juce::Justification::centred);

        g.setFont (FoundryLookAndFeel::labelFont (10.0f, false));
        g.setColour (hasSample ? t.textDim.brighter (0.15f) : t.textDim.withAlpha (0.5f));
        g.drawText (offsetText (s.offsetSeconds), offsetRect.reduced (6, 0), juce::Justification::centred);
        g.drawText (gainText (s.gain), gainRect, juce::Justification::centredRight);
    }

    //== editing ===============================================================
    void edit (const std::function<void (foundry::RiffSlot&)>& fn, bool recordUndo)
    {
        auto next = processor.getSketch();
        fn (next.slots[(size_t) index]);
        processor.setSketch (next, recordUndo);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto p = e.getPosition();
        gesture = Gesture::None;
        before = processor.getSketch();
        owner.selectSlot (index);

        if (e.mods.isPopupMenu())
        {
            showMenu();
            return;
        }

        const auto& s = slot();

        if (lockRect.contains (p))      { edit ([] (foundry::RiffSlot& x) { x.locked = ! x.locked; }, true); return; }
        if (clearRect.contains (p) && (! s.isEmpty() || processor.getSketch().heldBy (index) >= 0))
        {
            processor.silenceSlot (index);
            return;
        }
        if (lenDownRect.contains (p))   { edit ([] (foundry::RiffSlot& x) { x.lengthSteps = nextLength (x.lengthSteps, -1); }, true); return; }
        if (lenUpRect.contains (p) || lenTextRect.contains (p))
        {
            edit ([] (foundry::RiffSlot& x) { x.lengthSteps = nextLength (x.lengthSteps, +1); }, true);
            return;
        }

        if (s.isEmpty())
            return;

        if (gainRect.contains (p))
        {
            gesture = Gesture::Gain;
            startGainDb = gainToDb (s.gain);
        }
        else if (frame.contains (p))
        {
            gesture = Gesture::Slip;
            startOffset = s.offsetSeconds;
        }
        else if (topBar.contains (p))
        {
            gesture = Gesture::Move;
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto& s = slot();

        if (gesture == Gesture::Slip)
        {
            if (! slipping && std::abs (e.getDistanceFromDragStartX()) < 3)
                return;
            slipping = true;

            auto smp = sample();
            if (smp == nullptr)
                return;

            // Slip-edit, like sliding a clip's content in FL Studio's playlist:
            // the cell stays on the grid, the audio moves underneath it.
            // Drag left to start later in the sample. Shift = fine.
            const double gridSec = foundry::stepsToSeconds (processor.getSketch().gridSteps, processor.getRiffBpm());
            const double pxPerSec = (double) frame.getWidth() / juce::jmax (1.0e-3, gridSec);
            const double scale = e.mods.isShiftDown() ? 0.1 : 1.0;
            const double maxOffset = juce::jmax (0.0, smp->seconds() - 0.005);
            const double next = juce::jlimit (0.0, maxOffset, startOffset - e.getDistanceFromDragStartX() / pxPerSec * scale);

            if (! juce::approximatelyEqual (next, s.offsetSeconds))
                edit ([next] (foundry::RiffSlot& x) { x.offsetSeconds = next; }, false);
        }
        else if (gesture == Gesture::Gain)
        {
            const float db = juce::jlimit (-36.0f, 6.0f, startGainDb - (float) e.getDistanceFromDragStartY() * 0.2f);
            edit ([db] (foundry::RiffSlot& x) { x.gain = dbToGain (db); }, false);
        }
        else if (gesture == Gesture::Move && e.getDistanceFromDragStart() > 6)
        {
            gesture = Gesture::None;
            if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
                c->startDragging (dnd::slotPrefix + juce::String (index), this,
                                  juce::ScaledImage (createComponentSnapshot (getLocalBounds()).rescaled (getWidth() / 2, getHeight() / 2)),
                                  false);
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (gesture == Gesture::Slip && ! slipping && frame.contains (e.getPosition()) && e.getNumberOfClicks() < 2)
            processor.getRiff().auditionSlot (index);     // a click plays just this slot

        if (gesture == Gesture::Slip || gesture == Gesture::Gain)
            processor.recordUndoFrom (before);

        slipping = false;
        gesture = Gesture::None;
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (slot().isEmpty())
            return;
        if (frame.contains (e.getPosition()))
            edit ([] (foundry::RiffSlot& x) { x.offsetSeconds = 0.0; }, true);
        else if (gainRect.contains (e.getPosition()))
            edit ([] (foundry::RiffSlot& x) { x.gain = 1.0f; }, true);
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto p = e.getPosition();
        if (! slot().isEmpty() && frame.contains (p))
            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
        else if (! slot().isEmpty() && gainRect.contains (p))
            setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        else if (! slot().isEmpty() && topBar.contains (p) && ! lockRect.contains (p) && ! clearRect.contains (p))
            setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        else
            setMouseCursor (juce::MouseCursor::NormalCursor);
    }

    bool self_heldBy() const { return processor.getSketch().heldBy (index) >= 0; }

    void showMenu()
    {
        const auto s = slot();
        juce::PopupMenu m;
        m.addItem (1, "Play this slot", ! s.isEmpty());
        m.addItem (2, "Reset start position", ! s.isEmpty() && s.offsetSeconds > 0.0);
        m.addItem (3, "Reset volume", ! s.isEmpty() && ! juce::approximatelyEqual (s.gain, 1.0f));

        juce::PopupMenu lengths;
        for (int v : kLengthSteps)
            lengths.addItem (100 + v, foundry::stepsName (v), true, v == s.lengthSteps);
        m.addSubMenu ("Length", lengths);

        juce::PopupMenu copyTo;
        for (int k = 0; k < foundry::kRiffSlots; ++k)
            if (k != index)
                copyTo.addItem (200 + k, "Slot " + juce::String (k + 1));
        m.addSubMenu ("Copy to slot", copyTo, ! s.isEmpty());

        m.addSeparator();
        m.addItem (4, s.locked ? "Unlock" : "Lock (dice keeps it)");
        m.addItem (5, "Make silent (Delete)", ! s.isEmpty() || self_heldBy());
        m.addItem (6, "Show file in Explorer / Finder", ! s.isEmpty() && juce::File (s.path).existsAsFile());

        juce::Component::SafePointer<SlotView> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
        {
            if (safe == nullptr || r == 0)
                return;
            auto& self = *safe;
            if (r == 1) self.processor.getRiff().auditionSlot (self.index);
            if (r == 2) self.edit ([] (foundry::RiffSlot& x) { x.offsetSeconds = 0.0; }, true);
            if (r == 3) self.edit ([] (foundry::RiffSlot& x) { x.gain = 1.0f; }, true);
            if (r == 4) self.edit ([] (foundry::RiffSlot& x) { x.locked = ! x.locked; }, true);
            if (r == 5) self.processor.silenceSlot (self.index);
            if (r == 6) juce::File (self.slot().path).revealToUser();
            if (r >= 100 && r < 200)
            {
                const int v = r - 100;
                self.edit ([v] (foundry::RiffSlot& x) { x.lengthSteps = v; }, true);
            }
            if (r >= 200 && r < 200 + foundry::kRiffSlots)
            {
                auto next = self.processor.getSketch();
                const bool keepLock = next.slots[(size_t) (r - 200)].locked;
                next.slots[(size_t) (r - 200)] = next.slots[(size_t) self.index];
                next.slots[(size_t) (r - 200)].locked = keepLock;
                self.processor.setSketch (next, true);
            }
        });
    }

    //== drag and drop =========================================================
    bool isInterestedInDragSource (const SourceDetails& d) override
    {
        const auto desc = d.description.toString();
        return desc.startsWith (dnd::samplePrefix)
            || (desc.startsWith (dnd::slotPrefix) && desc.fromFirstOccurrenceOf (dnd::slotPrefix, false, false).getIntValue() != index);
    }

    void itemDragEnter (const SourceDetails&) override { dropHover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override  { dropHover = false; repaint(); }

    void itemDropped (const SourceDetails& d) override
    {
        dropHover = false;
        const auto desc = d.description.toString();

        if (desc.startsWith (dnd::samplePrefix))
        {
            // One sample replaces this slot; several fill this slot and the ones after it.
            owner.loadFilesFrom (index, dnd::pathsFrom (desc));
        }
        else if (desc.startsWith (dnd::slotPrefix))
        {
            // Dragging one slot onto another swaps them (locks stay with the position).
            const int from = desc.fromFirstOccurrenceOf (dnd::slotPrefix, false, false).getIntValue();
            if (from >= 0 && from < foundry::kRiffSlots && from != index)
            {
                auto next = processor.getSketch();
                auto a = next.slots[(size_t) from], b = next.slots[(size_t) index];
                std::swap (a.locked, b.locked);
                next.slots[(size_t) from] = b;
                next.slots[(size_t) index] = a;
                processor.setSketch (next, true);
            }
        }
        repaint();
    }

    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        for (const auto& f : files)
            if (foundry::isSupportedAudioFile (juce::File (f)))
                return true;
        return false;
    }

    void fileDragEnter (const juce::StringArray&, int, int) override { dropHover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override            { dropHover = false; repaint(); }

    void filesDropped (const juce::StringArray& files, int, int) override
    {
        dropHover = false;
        owner.loadFilesFrom (index, files);   // several files fill this slot and the ones after it
        repaint();
    }

private:
    enum class Gesture { None, Slip, Gain, Move };

    RiffMakerPanel& owner;
    FoundryAudioProcessor& processor;
    const int index;

    juce::Rectangle<int> topBar, bottomBar, frame, lockRect, clearRect,
                         lenDownRect, lenTextRect, lenUpRect, gainRect, offsetRect;

    juce::String cachedPath;
    std::shared_ptr<const foundry::SourceSample> cached;

    Gesture gesture = Gesture::None;
    foundry::RiffSketch before;
    double startOffset = 0.0;
    float startGainDb = 0.0f;
    bool slipping = false;
    bool dropHover = false;
    bool playing = false;
    Theme theme { getTheme() };
};

//==============================================================================
/** Grab this and drop it on the DAW's playlist: the riff is rendered to a WAV
    in My Riffs the moment the drag leaves the plugin window. */
class RiffMakerPanel::DragHandle : public juce::Component,
                                   public juce::SettableTooltipClient
{
public:
    DragHandle()
    {
        setTooltip ("Drag the riff into your DAW's playlist. It's rendered to a WAV in My Riffs as it leaves the plugin.");
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        setRepaintsOnMouseActivity (true);
    }

    std::function<void()> onClickedWithoutDrag;

    void setTheme (const Theme& t) { theme = t; repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (theme.accent.withAlpha (isMouseOver() ? 0.26f : 0.14f));
        g.fillRoundedRectangle (r, ui::controlCorner);
        g.setColour (theme.accent.withAlpha (0.8f));
        g.drawRoundedRectangle (r.reduced (0.5f), ui::controlCorner, 1.0f);

        // grip dots
        auto grip = r.removeFromLeft (18.0f).reduced (6.0f, 8.0f);
        g.setColour (theme.accent);
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 2; ++col)
                g.fillEllipse (grip.getX() + (float) col * 5.0f, grip.getY() + (float) row * 5.0f, 2.4f, 2.4f);

        g.setColour (theme.text);
        g.setFont (FoundryLookAndFeel::labelFont (11.0f, true, 0.08f));
        g.drawText ("DRAG TO DAW", r, juce::Justification::centred);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (started || e.getDistanceFromDragStart() < 5)
            return;
        started = true;

        juce::Image img (juce::Image::ARGB, 120, 34, true);
        {
            juce::Graphics g (img);
            g.setColour (theme.panel.withAlpha (0.92f));
            g.fillRoundedRectangle (img.getBounds().toFloat(), 6.0f);
            g.setColour (theme.accent);
            g.drawRoundedRectangle (img.getBounds().toFloat().reduced (1.0f), 6.0f, 1.5f);
            g.setColour (theme.text);
            g.setFont (FoundryLookAndFeel::labelFont (11.0f, true));
            g.drawText ("RIFF", img.getBounds(), juce::Justification::centred);
        }

        if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
            c->startDragging (dnd::riff, this, juce::ScaledImage (img), true);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (! started && onClickedWithoutDrag)
            onClickedWithoutDrag();
        started = false;
    }

private:
    bool started = false;
    Theme theme { getTheme() };
};

//==============================================================================
RiffMakerPanel::RiffMakerPanel (FoundryAudioProcessor& p)
    : processor (p)
{
    dragHandle = std::make_unique<DragHandle>();
    dragHandle->onClickedWithoutDrag = [this]
    {
        showMessage ("Click and drag this handle onto your DAW's playlist to drop the riff in as audio.", false);
    };

    for (int i = 0; i < foundry::kRiffSlots; ++i)
    {
        slots[(size_t) i] = std::make_unique<SlotView> (*this, processor, i);
        addAndMakeVisible (*slots[(size_t) i]);
    }

    playButton.setTooltip ("Loop the riff on its own (any MIDI note does the same)");
    stopButton.setTooltip ("Stop the riff and any preview");
    syncButton.setTooltip ("Play the riff along with the DAW's transport, locked to the bar");
    diceButton.setTooltip ("Fill every unlocked slot with a random sample from the folder open in the browser");
    undoButton.setTooltip ("Undo (Ctrl/Cmd+Z)");
    redoButton.setTooltip ("Redo (Ctrl/Cmd+Shift+Z)");
    clearButton.setTooltip ("Make every unlocked slot silent");
    setWantsKeyboardFocus (true);
    renderButton.setTooltip ("Save the riff as a WAV in My Riffs (with an editable copy you can reopen later)");

    playButton.onClick = [this] { processor.getRiff().startInternal(); };
    stopButton.onClick = [this]
    {
        processor.getRiff().stopAll();
        processor.getPreview().stop();
    };
    syncButton.setClickingTogglesState (true);
    syncButton.onClick = [this]
    {
        processor.setFollowTransport (syncButton.getToggleState());
        juce::String err;
        processor.persistConfig (err, true);
    };

    for (int g : foundry::gridChoices())
        gridBox.addItem (foundry::stepsName (g) + " grid", g);
    gridBox.setTooltip ("Spacing between slots. Eight slots of 1/4 make a 2-bar riff.");
    gridBox.onChange = [this] { setGrid (gridBox.getSelectedId()); };

    bpmEditor.setInputRestrictions (6, "0123456789.");
    bpmEditor.setJustification (juce::Justification::centred);
    bpmEditor.setTooltip ("Tempo used when the host doesn't provide one (standalone)");
    bpmEditor.onReturnKey = bpmEditor.onFocusLost = [this]
    {
        const double v = bpmEditor.getText().getDoubleValue();
        if (v >= 40.0 && v <= 300.0)
        {
            processor.setStandaloneBpm (v);
            processor.setSketch (processor.getSketch(), false);   // redraw slices at the new tempo
        }
        lastBpmShown = 0.0;
    };

    diceButton.onClick = [this] { rollDice(); };
    undoButton.onClick = [this] { processor.undo(); };
    redoButton.onClick = [this] { processor.redo(); };
    clearButton.onClick = [this]
    {
        auto next = processor.getSketch();
        for (auto& s : next.slots)
            if (! s.locked)
            {
                s.path.clear();
                s.offsetSeconds = 0.0;
            }
        processor.setSketch (next, true);
    };
    renderButton.onClick = [this] { render(); };

    for (auto* c : std::initializer_list<juce::Component*> { &playButton, &stopButton, &syncButton, &gridBox, &bpmEditor,
                                                             &diceButton, &undoButton, &redoButton, &clearButton,
                                                             &renderButton, dragHandle.get() })
        addAndMakeVisible (c);
    addChildComponent (banner);

    processor.sketchChanged.addChangeListener (this);
    syncFromProcessor();
    startTimerHz (30);
}

RiffMakerPanel::~RiffMakerPanel()
{
    stopTimer();
    processor.sketchChanged.removeChangeListener (this);
}

void RiffMakerPanel::setTheme (const Theme& t)
{
    theme = t;
    for (auto* b : { &playButton, &renderButton, &diceButton })
    {
        b->setColour (juce::TextButton::buttonColourId, t.accent);
        b->setColour (juce::TextButton::buttonOnColourId, t.accent);
    }
    for (auto* b : { &stopButton, &undoButton, &redoButton, &clearButton })
        b->setColour (juce::TextButton::buttonColourId, t.accentAlt);
    syncButton.setColour (juce::TextButton::buttonColourId, t.accentAlt);
    syncButton.setColour (juce::TextButton::buttonOnColourId, t.accent);
    gridBox.setColour (juce::ComboBox::textColourId, t.text);
    gridBox.setColour (juce::ComboBox::arrowColourId, t.accent);
    gridBox.setColour (juce::ComboBox::outlineColourId, t.panelEdge);
    banner.setTheme (t);
    dragHandle->setTheme (t);
    for (auto& s : slots)
        s->setTheme (t);
    repaint();
}

void RiffMakerPanel::syncFromProcessor()
{
    gridBox.setSelectedId (processor.getSketch().gridSteps, juce::dontSendNotification);
    syncButton.setToggleState (processor.getFollowTransport(), juce::dontSendNotification);
    undoButton.setEnabled (processor.canUndo());
    redoButton.setEnabled (processor.canRedo());
    for (auto& s : slots)
        s->repaint();

    const auto problems = processor.getSketchProblems();
    if (! problems.isEmpty())
        showMessage ("Can't find: " + problems.joinIntoString (", ")
                     + ". Those slots stay silent until you drop in a replacement.", true);
}

void RiffMakerPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    syncFromProcessor();
}

void RiffMakerPanel::selectSlot (int slot)
{
    selectedSlot = juce::jlimit (-1, foundry::kRiffSlots - 1, slot);
    grabKeyboardFocus();
    for (auto& s : slots)
        s->repaint();
}

bool RiffMakerPanel::keyPressed (const juce::KeyPress& key)
{
    if (selectedSlot < 0)
        return false;

    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        processor.silenceSlot (selectedSlot);
        return true;
    }
    if (key == juce::KeyPress::leftKey)  { selectSlot (juce::jmax (0, selectedSlot - 1)); return true; }
    if (key == juce::KeyPress::rightKey) { selectSlot (juce::jmin (foundry::kRiffSlots - 1, selectedSlot + 1)); return true; }
    if (key == juce::KeyPress::upKey)    { selectSlot (selectedSlot >= 4 ? selectedSlot - 4 : selectedSlot); return true; }
    if (key == juce::KeyPress::downKey)  { selectSlot (selectedSlot < 4 ? selectedSlot + 4 : selectedSlot); return true; }
    if (key == juce::KeyPress::returnKey)
    {
        processor.getRiff().auditionSlot (selectedSlot);
        return true;
    }
    if (key == juce::KeyPress::escapeKey)
    {
        selectSlot (-1);
        return true;
    }
    return false;
}

void RiffMakerPanel::setGrid (int steps)
{
    if (steps <= 0)
        return;

    auto next = processor.getSketch();
    // Slices that simply filled their cell keep filling it at the new spacing.
    for (auto& s : next.slots)
        if (s.lengthSteps == next.gridSteps)
            s.lengthSteps = steps;
    next.gridSteps = steps;
    processor.setSketch (next, true);
}

void RiffMakerPanel::loadIntoSlot (int slot, const juce::File& file)
{
    if (slot < 0)
    {
        const auto& sk = processor.getSketch();
        for (int i = 0; i < foundry::kRiffSlots && slot < 0; ++i)
            if (sk.slots[(size_t) i].isEmpty() && ! sk.slots[(size_t) i].locked)
                slot = i;

        if (slot < 0)
        {
            showMessage ("Every slot is full - drop the sample onto the slot you want to replace.", false);
            return;
        }
    }

    juce::String err;
    if (! processor.assignSample (slot, file, err))
        showMessage (err, true);
    else
        banner.setMessage ({}, false, false);
}

void RiffMakerPanel::loadFilesFrom (int firstSlot, const juce::StringArray& files)
{
    int slot = firstSlot;
    for (const auto& f : files)
    {
        if (slot >= foundry::kRiffSlots)
            break;
        if (! foundry::isSupportedAudioFile (juce::File (f)))
            continue;
        loadIntoSlot (slot++, juce::File (f));
    }
}

void RiffMakerPanel::showMessage (const juce::String& text, bool isError)
{
    banner.setMessage (text, isError, false);
}

void RiffMakerPanel::rollDice()
{
    std::vector<foundry::LibraryItem> pool;
    if (getDicePool)
        pool = getDicePool();

    juce::String msg;
    const bool ok = processor.rollDice (pool, msg);
    showMessage (msg, ! ok);
}

void RiffMakerPanel::render()
{
    juce::String err;
    bool reused = false;
    const auto file = processor.renderRiff (err, reused);

    if (file == juce::File())
    {
        showMessage (err, true);
        return;
    }

    if (reused)
        showMessage ("Nothing changed since the last render - it's already saved as " + file.getFileName() + " in My Riffs.", false);
    else if (err.isNotEmpty())
        showMessage (err, true);
    else
        showMessage ("Saved " + file.getFileName() + " to My Riffs. Drag it from the browser, or use DRAG TO DAW.", false);

    if (onRendered)
        onRendered (file);
}

void RiffMakerPanel::timerCallback()
{
    auto& riff = processor.getRiff();
    const int current = riff.isRunning() ? riff.getCurrentSlot() : -1;
    if (current != lastSlotShown)
    {
        lastSlotShown = current;
        for (int i = 0; i < foundry::kRiffSlots; ++i)
            slots[(size_t) i]->setPlaying (i == current);
    }

    playButton.setToggleState (riff.isInternalRunning(), juce::dontSendNotification);

    // Tempo: follow the host when it reports one, otherwise let the user type it.
    const bool hostTempo = processor.hostProvidesTempo();
    const double bpm = processor.getRiffBpm();
    if (hostTempo != lastHostTempo || std::abs (bpm - lastBpmShown) > 0.01)
    {
        lastHostTempo = hostTempo;
        lastBpmShown = bpm;
        if (! bpmEditor.hasKeyboardFocus (true))
            bpmEditor.setText (juce::String (bpm, std::abs (bpm - std::round (bpm)) < 0.05 ? 0 : 1) + " BPM", juce::dontSendNotification);
        bpmEditor.setReadOnly (hostTempo);
        bpmEditor.setTooltip (hostTempo ? "Tempo comes from the host" : "Tempo for the standalone app - type a value and press Enter");
        for (auto& s : slots)
            s->repaint();     // slice widths depend on tempo
    }
}

void RiffMakerPanel::resized()
{
    auto b = getLocalBounds().reduced (14, 10);

    auto bar = b.removeFromTop (30);
    playButton.setBounds (bar.removeFromLeft (62));
    bar.removeFromLeft (4);
    stopButton.setBounds (bar.removeFromLeft (62));
    bar.removeFromLeft (4);
    syncButton.setBounds (bar.removeFromLeft (58));
    bar.removeFromLeft (14);
    gridBox.setBounds (bar.removeFromLeft (100));
    bar.removeFromLeft (6);
    bpmEditor.setBounds (bar.removeFromLeft (84));
    bar.removeFromLeft (14);
    diceButton.setBounds (bar.removeFromLeft (64));
    bar.removeFromLeft (4);
    undoButton.setBounds (bar.removeFromLeft (58));
    bar.removeFromLeft (4);
    redoButton.setBounds (bar.removeFromLeft (58));
    bar.removeFromLeft (4);
    clearButton.setBounds (bar.removeFromLeft (62));

    dragHandle->setBounds (bar.removeFromRight (128));
    bar.removeFromRight (6);
    renderButton.setBounds (bar.removeFromRight (82));

    b.removeFromTop (8);
    if (banner.isVisible())
    {
        banner.setBounds (b.removeFromTop (30));
        b.removeFromTop (6);
    }

    const int gap = 10;
    const int rowH = (b.getHeight() - gap) / 2;
    const int colW = (b.getWidth() - 3 * gap) / 4;

    for (int i = 0; i < foundry::kRiffSlots; ++i)
    {
        const int row = i / 4, col = i % 4;
        slots[(size_t) i]->setBounds (b.getX() + col * (colW + gap), b.getY() + row * (rowH + gap), colW, rowH);
    }
}

void RiffMakerPanel::paint (juce::Graphics&)
{
}

} // namespace pt
