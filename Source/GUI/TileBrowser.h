#pragma once

#include "Widgets.h"
#include "../PluginProcessor.h"

namespace pt
{

/** Drag-and-drop descriptions shared by the browser, the slots and the editor. */
namespace dnd
{
    inline const juce::String samplePrefix = "foundry-sample:";
    inline const juce::String slotPrefix   = "foundry-slot:";
    inline const juce::String riff         = "foundry-riff";
}

//==============================================================================
/** The asset dock along the bottom of Foundry: square tiles with a waveform
    and the sample's name under it, folders you click into, a breadcrumb, an
    instant (indexed) search and a list view for when you want density.

    Click a sample to hear it, drag it onto a Riff Maker slot - or drag it
    straight out of the plugin into the DAW. */
class TileBrowser : public juce::Component,
                    private juce::Timer
{
public:
    std::function<void (int slot, const juce::File&)> onLoadIntoSlot;   // slot -1 = next empty
    std::function<void (const juce::File&)> onOpenRiff;
    std::function<void (const foundry::LibraryItem&)> onSendToSplicer;
    std::function<void (const foundry::LibraryItem&)> onLockInDrums;
    std::function<void (const juce::String&, bool isError)> onMessage;
    std::function<void()> onOpenSettings;

    explicit TileBrowser (FoundryAudioProcessor& p);
    ~TileBrowser() override;

    void setTheme (const Theme& t);

    /** Re-reads the current folder from the library index. */
    void refresh();
    void navigateTo (const foundry::BrowserLocation& where);
    void showFolder (const juce::File& dir);
    const foundry::BrowserLocation& getLocation() const noexcept { return location; }

    /** What the dice may draw from: the search results when searching,
        otherwise every sample at or below the open folder. */
    std::vector<foundry::LibraryItem> dicePool() const;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int kTopBarH = 34;

private:
    class Body;
    class Crumbs;

    void timerCallback() override;
    void goUp();
    void setListView (bool list);
    void activate (int index, bool doubleClick);
    void showMenu (int index);
    void confirmDelete (const foundry::LibraryItem& item);
    void message (const juce::String& m, bool error);

    FoundryAudioProcessor& processor;

    juce::TextButton upButton { "<" };
    juce::TextButton tilesButton { "TILES" }, listButton { "LIST" };
    juce::TextButton rescanButton { "RESCAN" };
    juce::TextEditor search;
    std::unique_ptr<Crumbs> crumbs;
    juce::Viewport viewport;
    std::unique_ptr<Body> body;

    foundry::BrowserLocation location;
    std::vector<foundry::BrowserEntry> entries;
    bool truncated = false;
    bool listView = false;
    int selected = -1;

    int searchDebounce = 0;
    int lastThumbGeneration = -1;

    Theme theme { getTheme() };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TileBrowser)
};

} // namespace pt
