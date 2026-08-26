#pragma once

#include "LookAndFeel.h"
#include "../Presets/PresetManager.h"

namespace pt
{

//==============================================================================
/**
    Full-window preset browser.

    Lays the whole bank out as a grid so a category fits on screen without
    scrolling, with a category sidebar down the left - the folders are
    pre-made (see FactoryPresets.h's presetCategories()), so the obvious ones
    are there without anyone having to build them.

    A single click loads immediately rather than needing a confirm, because
    the point of seeing them all at once is auditioning: click along a row and
    hear each one. The panel stays open while you do, and closes on Escape, on
    the close button, or on a click outside the card.
*/
class PresetBrowser : public juce::Component
{
public:
    explicit PresetBrowser (PresetManager& manager);

    void setTheme (const Theme& t);

    /** Rebuilds the tile list from the manager and shows the panel. */
    void show();
    void hide();

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    /** Fired after a preset is loaded, so the editor can refresh the footer. */
    std::function<void()> onPresetLoaded;

private:
    struct Entry
    {
        juce::String name;
        juce::String category;
        bool isUser = false;
        int  index = 0;          // index within its own bank
    };

    void rebuildEntries();
    void refreshFilter();
    void loadEntry (const Entry& e);

    juce::Rectangle<int> cardBounds() const;
    juce::Rectangle<int> gridBounds() const;
    juce::Rectangle<int> sidebarBounds() const;
    int tileIndexAt (juce::Point<int> p) const;

    PresetManager& presets;
    Theme theme;

    std::vector<Entry> allEntries;
    std::vector<int>   visible;          // indices into allEntries
    juce::StringArray  categories;       // "All" + presetCategories()

    int selectedCategory = 0;
    int hoveredTile = -1;
    int loadedTile = -1;

    juce::TextButton closeButton { "CLOSE" };

    // Grid geometry, recomputed on resize.
    int columns = 4, tileW = 150, tileH = 34, gridGap = 6;

    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBrowser)
};

} // namespace pt
