#pragma once

#include "Theme.h"

namespace pt
{

//==============================================================================
/** Shared geometry. Every container, pill and drop-down derives its corner
    from one of these two numbers rather than picking its own, because
    inconsistent radii are the thing that reads as "amateur plugin" from
    across the room even when nobody can say why. */
namespace ui
{
    inline constexpr float panelCorner   = 9.0f;
    inline constexpr float controlCorner = 5.0f;
}

//==============================================================================
/** Everything a page needs to look considered rather than default: panel
    chrome, a photographed rotary knob instead of a flat vector disc, glow and
    shadow primitives, and consistent type. Every GUI component in this
    template draws through here rather than rolling its own paint() from
    scratch - that's what keeps forty controls looking like one instrument
    instead of forty separate widgets.

    FoundryLookAndFeel is the one identifier in this file worth
    renaming to match your plugin (see scripts/rename_plugin.py) - everything
    it does is generic. */
class FoundryLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FoundryLookAndFeel();

    void setTheme (const Theme& t);
    const Theme& getCurrentTheme() const noexcept { return theme; }

    static juce::Font titleFont (float height, float tracking = 0.0f);
    static juce::Font labelFont (float height, bool bold = false, float tracking = 0.0f);

    /** drawText, but for letter-spaced text that has to look optically
        centred. withExtraKerningFactor() inserts its extra space after
        *every* character, the last one included, so a tracked string carries
        an invisible trailing gap that a naive centred drawText() measures as
        part of the text - the glyphs end up sitting visibly left of where the
        eye expects them. This corrects for it. */
    static void drawCentredTracked (juce::Graphics&, const juce::String& text,
                                    juce::Rectangle<float> area);

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;

    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;

    void drawButtonBackground (juce::Graphics&, juce::Button&,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override;

    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;

    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height,
                        bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                        bool isMouseOver, bool isMouseDown) override;

    int getDefaultScrollbarWidth() override { return 9; }

    void fillTextEditorBackground (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int width, int height, juce::TextEditor&) override;

    //== shared primitives =====================================================
    /** Smoked-glass container: a cast shadow, a vertical gradient body, a
        hairline edge and a top-lit rim. Every panel in the plugin should be
        drawn with this rather than a plain fillRoundedRectangle(). */
    static void drawPanel (juce::Graphics&, juce::Rectangle<float> bounds, const Theme&,
                           float cornerSize = ui::panelCorner);

    /** Soft drop shadow under a rounded rectangle. Concentric offset rounded
        rects rather than a real Gaussian blur: cheap enough to run on every
        repaint, and at these sizes and opacities the difference from a true
        blur isn't visible. */
    static void dropShadow (juce::Graphics&, juce::Rectangle<float> bounds,
                            float corner, float depth = 4.0f, float strength = 0.30f);

    /** Radial bloom centred on a point - what makes a lit control look like it
        is emitting onto the surface behind it rather than sitting on top of
        it. Cheap to overuse; use it sparingly, only where something is
        actually "lit". */
    static void bloom (juce::Graphics&, juce::Point<float> centre, float radius,
                       juce::Colour colour, float strength);

    /** Illuminated pill: the shared "this is active" fill for toggle buttons,
        mode switches and selected list rows. */
    static void drawActivePill (juce::Graphics&, juce::Rectangle<float> bounds,
                                float corner, juce::Colour accent, float intensity = 1.0f);

    /** Legible text colour for a label sitting on top of an illuminated pill.
        Not simply white: derived from the fill's own luminance so a light
        accent and a dark one both stay readable without hand-picking two text
        colours per theme. */
    static juce::Colour pillTextColour (juce::Colour accent) noexcept;

    static void drawInsetWell (juce::Graphics&, juce::Rectangle<float> bounds, const Theme&,
                               float corner = ui::controlCorner);

private:
    /** The shared knob bitmap. Held by SharedResourcePointer rather than a
        static or juce::ImageCache so its lifetime ends with the last
        look-and-feel instance, well before the host unloads the plugin.

        This matters more than it looks: a function-local static juce::Image
        is destroyed by the C runtime at DLL unload, which on Windows happens
        *after* JUCE has already run its own shutdown - freeing image pixel
        data after the framework that allocated it has been torn down is a
        classic crash-on-unload. juce::ImageCache is worse again, because it
        lazily creates a DeletedAtShutdown singleton with its own Timer.
        SharedResourcePointer sidesteps both: the bitmap is decoded when the
        first look-and-feel is built and freed when the last one goes away. */
    const juce::Image& knobBodyImage() const;

    juce::SharedResourcePointer<struct KnobArt> knobArt;
    Theme theme;
};

} // namespace pt
