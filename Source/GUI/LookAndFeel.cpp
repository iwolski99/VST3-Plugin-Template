#include "LookAndFeel.h"
#include "TemplateBinaryData.h"

namespace pt
{

/** The knob body is a single photographed/rendered PNG rather than a
    filmstrip: JUCE rotates the Image with an AffineTransform at draw time, so
    one frame covers every angle at full resolution instead of needing ~100
    pre-baked ones. Decoded once and shared by every knob on every page. */
struct KnobArt
{
    KnobArt()
        : body (juce::ImageFileFormat::loadFrom (templatedata::knob_body_master_png,
                                                 (size_t) templatedata::knob_body_master_pngSize))
    {
    }

    juce::Image body;
};

const juce::Image& BinauralBeatsLookAndFeel::knobBodyImage() const
{
    return knobArt->body;
}

BinauralBeatsLookAndFeel::BinauralBeatsLookAndFeel()
    : theme (getTheme())
{
    setTheme (theme);
}

void BinauralBeatsLookAndFeel::setTheme (const Theme& t)
{
    theme = t;

    setColour (juce::PopupMenu::backgroundColourId,          theme.panel.darker (0.45f));
    setColour (juce::PopupMenu::textColourId,                theme.text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, theme.accent.withAlpha (0.28f));
    setColour (juce::PopupMenu::highlightedTextColourId,     theme.text);
    setColour (juce::ComboBox::textColourId,                 theme.text);
    setColour (juce::ComboBox::backgroundColourId,           theme.panel.brighter (0.06f));
    setColour (juce::ComboBox::outlineColourId,              theme.panelEdge);
    setColour (juce::ComboBox::arrowColourId,                theme.accent);
    setColour (juce::Label::textColourId,                    theme.text);
    setColour (juce::TooltipWindow::backgroundColourId,      theme.panel.darker (0.5f));
    setColour (juce::TooltipWindow::textColourId,            theme.text);
    setColour (juce::TextEditor::backgroundColourId,         theme.panel.darker (0.3f));
    setColour (juce::TextEditor::textColourId,               theme.text);
    setColour (juce::TextEditor::highlightColourId,          theme.accent.withAlpha (0.4f));
    setColour (juce::TextEditor::outlineColourId,            theme.panelEdge);
    setColour (juce::TextEditor::focusedOutlineColourId,     theme.accent);
    setColour (juce::AlertWindow::backgroundColourId,        theme.bgBottom);
    setColour (juce::AlertWindow::textColourId,              theme.text);
    setColour (juce::TextButton::buttonColourId,             theme.accent);
    setColour (juce::TextButton::buttonOnColourId,           theme.accent);
    setColour (juce::Slider::rotarySliderFillColourId,       theme.accent);
}

//==============================================================================
// No embedded font files. An embedded Inter or SF Pro would mean carrying a
// licence and a binary blob for a difference most people would not name; what
// actually reads as "expensive" at this size is spacing and weight
// discipline, which the platform font does perfectly well once it is asked
// properly.
juce::Font BinauralBeatsLookAndFeel::titleFont (float height, float tracking)
{
    auto f = juce::Font (juce::FontOptions (height, juce::Font::bold));
    return tracking != 0.0f ? f.withExtraKerningFactor (tracking) : f;
}

juce::Font BinauralBeatsLookAndFeel::labelFont (float height, bool bold, float tracking)
{
    auto f = juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
    return tracking != 0.0f ? f.withExtraKerningFactor (tracking) : f;
}

void BinauralBeatsLookAndFeel::drawCentredTracked (juce::Graphics& g, const juce::String& text,
                                                    juce::Rectangle<float> area)
{
    const auto& f = g.getCurrentFont();

    // The trailing gap is one character's worth of extra kerning, and kerning
    // is expressed as a factor of the font height. Half of it is the amount
    // the visible glyphs are pushed left by centring the measured box.
    const float trailing = f.getHeight() * f.getExtraKerningFactor();

    g.drawText (text, area.translated (trailing * 0.5f, 0.0f), juce::Justification::centred);
}

//==============================================================================
void BinauralBeatsLookAndFeel::dropShadow (juce::Graphics& g, juce::Rectangle<float> b,
                                            float corner, float depth, float strength)
{
    constexpr int layers = 4;

    for (int i = layers; i >= 1; --i)
    {
        const float t = (float) i / (float) layers;
        const float spread = depth * t;

        g.setColour (juce::Colours::black.withAlpha (strength / (float) layers));
        g.fillRoundedRectangle (b.expanded (spread * 0.5f).translated (0.0f, spread * 0.7f),
                                corner + spread * 0.5f);
    }
}

void BinauralBeatsLookAndFeel::bloom (juce::Graphics& g, juce::Point<float> centre, float radius,
                                       juce::Colour colour, float strength)
{
    if (strength <= 0.005f || radius <= 0.0f)
        return;

    juce::ColourGradient halo (colour.withAlpha (juce::jlimit (0.0f, 1.0f, strength)),
                               centre.x, centre.y,
                               colour.withAlpha (0.0f), centre.x, centre.y + radius, true);
    halo.addColour (0.45, colour.withAlpha (juce::jlimit (0.0f, 1.0f, strength * 0.42f)));

    g.setGradientFill (halo);
    g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));
}

void BinauralBeatsLookAndFeel::drawActivePill (juce::Graphics& g, juce::Rectangle<float> b,
                                                float corner, juce::Colour accent, float intensity)
{
    const float k = juce::jlimit (0.0f, 1.0f, intensity);

    // Outer halo first, so the fill sits inside its own light.
    g.setColour (accent.withAlpha (0.20f * k));
    g.fillRoundedRectangle (b.expanded (2.6f), corner + 2.6f);
    g.setColour (accent.withAlpha (0.30f * k));
    g.fillRoundedRectangle (b.expanded (1.2f), corner + 1.2f);

    juce::ColourGradient fill (accent.brighter (0.26f).withAlpha (0.97f * k),
                               b.getCentreX(), b.getY(),
                               accent.darker (0.16f).withAlpha (0.97f * k),
                               b.getCentreX(), b.getBottom(), false);
    g.setGradientFill (fill);
    g.fillRoundedRectangle (b, corner);

    // Top-lit rim: one hairline of reflected light is the whole trick.
    juce::ColourGradient rim (juce::Colours::white.withAlpha (0.34f * k), b.getCentreX(), b.getY(),
                              juce::Colours::transparentWhite, b.getCentreX(),
                              b.getY() + b.getHeight() * 0.7f, false);
    g.setGradientFill (rim);
    g.drawRoundedRectangle (b.reduced (0.5f), corner, 1.1f);
}

juce::Colour BinauralBeatsLookAndFeel::pillTextColour (juce::Colour accent) noexcept
{
    return accent.getPerceivedBrightness() > 0.55f
             ? juce::Colour (0xff10131a).withAlpha (0.94f)
             : juce::Colours::white.withAlpha (0.97f);
}

//==============================================================================
void BinauralBeatsLookAndFeel::drawPanel (juce::Graphics& g, juce::Rectangle<float> b,
                                           const Theme& t, float corner)
{
    dropShadow (g, b, corner, 5.0f, 0.34f);

    // Slightly translucent smoked glass, with the gradient darkening toward
    // the bottom so the panel looks lit from above rather than uniformly
    // filled.
    juce::ColourGradient grad (t.panel.brighter (0.16f).withAlpha (0.92f), b.getCentreX(), b.getY(),
                               t.panel.darker (0.30f).withAlpha (0.96f),   b.getCentreX(), b.getBottom(),
                               false);
    grad.addColour (0.5, t.panel.darker (0.04f).withAlpha (0.94f));
    g.setGradientFill (grad);
    g.fillRoundedRectangle (b, corner);

    g.setColour (t.panelEdge.withAlpha (0.5f));
    g.drawRoundedRectangle (b.reduced (0.5f), corner, 1.0f);

    // Top-lit rim, drawn over the edge so it wins where they overlap.
    juce::ColourGradient rim (juce::Colours::white.withAlpha (0.11f), b.getCentreX(), b.getY(),
                              juce::Colours::transparentWhite, b.getCentreX(),
                              b.getY() + b.getHeight() * 0.42f, false);
    g.setGradientFill (rim);
    g.drawRoundedRectangle (b.reduced (0.5f), corner, 1.2f);
}

//==============================================================================
void BinauralBeatsLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                                  float sliderPos, float startAngle, float endAngle,
                                                  juce::Slider& slider)
{
    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (2.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre  = bounds.getCentre();
    const float track  = juce::jmax (2.5f, radius * 0.155f);
    const float arcR   = radius - track * 0.5f - 1.0f;

    const bool  enabled = slider.isEnabled();
    const bool  hot     = enabled && slider.isMouseOverOrDragging();
    const float dim     = enabled ? 1.0f : 0.4f;

    auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId, true)
                      .withMultipliedAlpha (dim);

    if (hot)
        accent = accent.brighter (0.18f);

    const float angle = startAngle + sliderPos * (endAngle - startAngle);

    // Sized to leave a thin recessed collar before the LED track starts,
    // rather than the knob's rim touching the track directly - the gap is
    // what reads as a socket the knob sits in instead of a ring painted
    // around it.
    const auto body = juce::Rectangle<float> (arcR * 1.62f, arcR * 1.62f).withCentre (centre);

    // --- recessed collar ------------------------------------------------------
    g.setColour (juce::Colours::black.withAlpha (0.30f * dim));
    g.fillEllipse (body.translated (0.0f, 1.6f).expanded (0.6f));

    juce::ColourGradient collar (theme.bgBottom.darker (0.30f).withAlpha (0.9f * dim),
                                 centre.x, centre.y - arcR,
                                 theme.bgBottom.darker (0.02f).withAlpha (0.85f * dim),
                                 centre.x, centre.y + arcR, false);
    g.setGradientFill (collar);
    g.fillEllipse (juce::Rectangle<float> (arcR * 2.0f, arcR * 2.0f).withCentre (centre));

    // --- machined body, a photographed knob rather than vector art ----------
    // JUCE rotates the cached bitmap with an AffineTransform, so this is one
    // frame at full resolution rather than a pre-baked filmstrip. Its own
    // alpha channel is already a clean circle, and a circle stays a circle
    // under rotation about its own centre, so no extra clip path is needed.
    const auto& knobImg = knobBodyImage();

    if (knobImg.isValid())
    {
        const float imgCx = (float) knobImg.getWidth()  * 0.5f;
        const float imgCy = (float) knobImg.getHeight() * 0.5f;
        const float destR = body.getWidth() * 0.5f;
        const float s     = destR / imgCx;

        const auto xform = juce::AffineTransform::translation (-imgCx, -imgCy)
                                .scaled (s)
                                .rotated (angle)
                                .translated (centre.x, centre.y);

        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.setOpacity (dim);
        g.drawImageTransformed (knobImg, xform, false);
        g.setOpacity (1.0f);
    }
    else
    {
        // Fallback so a build missing the asset still renders a usable, if
        // plainer, knob rather than a blank disc.
        juce::ColourGradient bodyFill (theme.panel.brighter (0.36f),
                                       centre.x - arcR * 0.42f, centre.y - arcR * 0.62f,
                                       theme.panel.darker (0.48f),
                                       centre.x + arcR * 0.34f, centre.y + arcR * 0.74f, false);
        bodyFill.addColour (0.52, theme.panel.darker (0.04f));
        g.setGradientFill (bodyFill);
        g.fillEllipse (body);
        g.setColour (theme.panelEdge.withAlpha (0.72f * dim));
        g.drawEllipse (body, 1.0f);
    }

    // Static specular sheen across the upper third, painted after the rotated
    // body rather than baked into it: a real key light does not turn with the
    // knob, so rotating a directional highlight along with the photo would
    // make it read as "spinning under a fixed lamp" instead of "turning under
    // one".
    juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.07f * dim),
                                centre.x, centre.y - arcR * 0.80f,
                                juce::Colours::transparentWhite, centre.x, centre.y + arcR * 0.15f,
                                false);
    g.setGradientFill (sheen);
    g.fillEllipse (body.reduced (1.0f));

    // --- track --------------------------------------------------------------
    // Nearly black rather than a visible grey-blue stroke: the unlit portion
    // of the arc and the collar underneath it should read as one continuous
    // recessed groove, not as a lit-ring-on-top-of-a-different-dark-ring.
    juce::Path bg;
    bg.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colours::black.withAlpha (0.55f * dim));
    g.strokePath (bg, juce::PathStrokeType (track, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));

    // --- filled value arc, from the centre for bipolar ranges ---------------
    const bool bipolar = slider.getMinimum() < -0.001 && slider.getMaximum() > 0.001;
    const float originPos = bipolar
        ? (float) ((0.0 - slider.getMinimum()) / (slider.getMaximum() - slider.getMinimum()))
        : 0.0f;
    const float originAngle = startAngle + originPos * (endAngle - startAngle);
    const float travel = std::abs (angle - originAngle) / juce::jmax (0.001f, endAngle - startAngle);

    if (std::abs (angle - originAngle) > 0.001f)
    {
        // Glow scales with how far the control is from its resting point, so
        // a page's visual hierarchy follows what is actually doing something
        // rather than being assigned by hand.
        const float lit = juce::jlimit (0.0f, 1.0f, travel * 1.35f) * dim * (hot ? 1.25f : 1.0f);

        bloom (g, centre, juce::jmin (arcR * 1.85f, radius * 0.92f), accent, 0.24f * lit);

        juce::Path fill;
        fill.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f,
                            juce::jmin (originAngle, angle), juce::jmax (originAngle, angle), true);

        // Halo around the value arc: several concentric strokes whose alpha
        // decays as they widen, approximating a falloff a single flat-alpha
        // stroke can't have (a stroke has a hard edge however wide it is).
        constexpr int kHaloLayers = 5;

        const float haloRoom  = (radius + 2.0f) - arcR - 0.5f;
        const float haloWidth = juce::jmin (track * 2.6f, haloRoom * 2.0f);
        const float haloLit   = 0.30f + 0.70f * lit;

        for (int i = kHaloLayers; i >= 1; --i)
        {
            const float t = (float) i / (float) kHaloLayers;     // 1 = widest, faintest

            g.setColour (accent.withAlpha (0.15f * haloLit * (1.0f - t * 0.70f)));
            g.strokePath (fill, juce::PathStrokeType (track + (haloWidth - track) * t,
                                                      juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        }

        // The core is a strip of discrete LED ticks rather than one
        // continuous stroke - decorative, not informative (the smooth halo
        // above it already says precisely where the value sits), but it's the
        // one detail that reads as "premium hardware" rather than "software
        // knob" at a glance.
        constexpr int   kArcTicks    = 28;
        constexpr float kTickSpanFrac = 0.70f;   // fraction of each tick's angular slot that is drawn

        const float sweep    = endAngle - startAngle;
        const float tickStep = sweep / (float) kArcTicks;
        const float tickSpan = tickStep * kTickSpanFrac;
        const float loArc    = juce::jmin (originAngle, angle);
        const float hiArc    = juce::jmax (originAngle, angle);
        const float tickWidth = track * 0.62f;

        g.setColour (accent.interpolatedWith (juce::Colours::white, 0.30f * lit));

        for (int i = 0; i < kArcTicks; ++i)
        {
            const float a0 = startAngle + tickStep * (float) i + (tickStep - tickSpan) * 0.5f;
            const float a1 = a0 + tickSpan;

            if (0.5f * (a0 + a1) < loArc || 0.5f * (a0 + a1) > hiArc)
                continue;

            juce::Path tick;
            tick.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, a0, a1, true);
            g.strokePath (tick, juce::PathStrokeType (tickWidth, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::butt));
        }

        // A small hot core at the moving end - the one point on the arc that
        // corresponds to an actual physical position.
        const juce::Point<float> tip (centre.x + arcR * std::sin (angle),
                                      centre.y - arcR * std::cos (angle));
        bloom (g, tip, track * 2.2f, accent, 0.45f * lit);
        g.setColour (juce::Colours::white.withAlpha (0.85f * lit * dim));
        g.fillEllipse (juce::Rectangle<float> (track * 0.62f, track * 0.62f).withCentre (tip));
    }

    // No separate pointer or cap layer: both are photographed into the body
    // image (the light bar at its top edge, the specular convergence at its
    // centre), and they rotate correctly for free as part of it. Hover/active
    // feedback comes from the arc and halo above brightening instead.
}

//==============================================================================
void BinauralBeatsLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                              int, int, int, int, juce::ComboBox& box)
{
    auto b = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    const float enabledAlpha = box.isEnabled() ? 1.0f : 0.4f;
    const bool over = box.isMouseOver() && box.isEnabled();

    juce::ColourGradient fill (theme.bgBottom.brighter (0.10f).withAlpha (0.80f * enabledAlpha),
                               b.getCentreX(), b.getY(),
                               theme.bgBottom.withAlpha (0.86f * enabledAlpha),
                               b.getCentreX(), b.getBottom(), false);
    g.setGradientFill (fill);
    g.fillRoundedRectangle (b, ui::controlCorner);

    g.setColour ((over ? theme.accent.withAlpha (0.75f) : theme.panelEdge.withAlpha (0.8f))
                     .withMultipliedAlpha (enabledAlpha));
    g.drawRoundedRectangle (b, ui::controlCorner, 1.0f);

    juce::ColourGradient rim (juce::Colours::white.withAlpha (0.10f * enabledAlpha),
                              b.getCentreX(), b.getY(), juce::Colours::transparentWhite,
                              b.getCentreX(), b.getCentreY(), false);
    g.setGradientFill (rim);
    g.drawRoundedRectangle (b, ui::controlCorner, 1.0f);

    juce::Path arrow;
    const float cx = b.getRight() - 12.0f;
    const float cy = b.getCentreY();
    arrow.startNewSubPath (cx - 4.0f, cy - 2.0f);
    arrow.lineTo (cx, cy + 2.5f);
    arrow.lineTo (cx + 4.0f, cy - 2.0f);

    g.setColour (theme.accent.withAlpha ((over ? 1.0f : 0.9f) * enabledAlpha));
    g.strokePath (arrow, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
}

void BinauralBeatsLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 0, box.getWidth() - 24, box.getHeight());
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

juce::Font BinauralBeatsLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return labelFont (12.0f, false, 0.012f);
}

juce::Font BinauralBeatsLookAndFeel::getPopupMenuFont()
{
    return labelFont (13.0f, false, 0.010f);
}

void BinauralBeatsLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    auto b = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);

    juce::ColourGradient fill (theme.bgBottom.brighter (0.11f), b.getCentreX(), b.getY(),
                               theme.bgBottom.brighter (0.03f), b.getCentreX(), b.getBottom(), false);
    g.setGradientFill (fill);
    g.fillRoundedRectangle (b, ui::controlCorner);

    g.setColour (theme.panelEdge.withAlpha (0.9f));
    g.drawRoundedRectangle (b.reduced (0.5f), ui::controlCorner, 1.0f);
}

//==============================================================================
void BinauralBeatsLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                      const juce::Colour& backgroundColour,
                                                      bool highlighted, bool down)
{
    auto b = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = button.getToggleState();

    const auto accent = backgroundColour.isTransparent() ? theme.accent : backgroundColour;

    if (on)
    {
        drawActivePill (g, b, ui::controlCorner, accent, highlighted ? 1.0f : 0.92f);
        return;
    }

    juce::ColourGradient fill (theme.bgBottom.brighter (0.09f).withAlpha (0.80f),
                               b.getCentreX(), b.getY(),
                               theme.bgBottom.withAlpha (0.86f), b.getCentreX(), b.getBottom(), false);
    g.setGradientFill (fill);
    g.fillRoundedRectangle (b, ui::controlCorner);

    if (down)
    {
        g.setColour (accent.withAlpha (0.20f));
        g.fillRoundedRectangle (b, ui::controlCorner);
    }

    g.setColour (highlighted ? accent.withAlpha (0.78f) : theme.panelEdge.withAlpha (0.85f));
    g.drawRoundedRectangle (b, ui::controlCorner, 1.0f);

    juce::ColourGradient rim (juce::Colours::white.withAlpha (0.10f), b.getCentreX(), b.getY(),
                              juce::Colours::transparentWhite, b.getCentreX(), b.getCentreY(), false);
    g.setGradientFill (rim);
    g.drawRoundedRectangle (b, ui::controlCorner, 1.0f);
}

void BinauralBeatsLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button,
                                                bool highlighted, bool)
{
    const bool on = button.getToggleState();

    g.setFont (labelFont (10.5f, true, 0.055f));

    if (on)
    {
        // High contrast against the lit fill, whichever way that falls: an
        // active state should be readable across the room, not merely
        // distinguishable up close.
        const auto accent = button.findColour (juce::TextButton::buttonOnColourId);
        g.setColour (pillTextColour (accent));
    }
    else
    {
        g.setColour (highlighted ? theme.text : theme.textDim.brighter (0.22f));
    }

    g.drawFittedText (button.getButtonText(), button.getLocalBounds(),
                      juce::Justification::centred, 1);
}

} // namespace pt
