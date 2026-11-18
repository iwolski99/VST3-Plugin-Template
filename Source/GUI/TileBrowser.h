#pragma once

#include "Widgets.h"
#include "../PluginProcessor.h"
#include <set>

namespace pt
{

/** Drag-and-drop descriptions shared by the browser, the slots and the editor. */
namespace dnd
{
    /** Followed by one or more absolute paths separated by newlines. */
    inline const juce::String samplePrefix = "foundry-sample:";
    inline const juce::String slotPrefix   = "foundry-slot:";
    inline const juce::String riff         = "foundry-riff";

    inline juce::StringArray pathsFrom (const juce::String& description)
    {
        return juce::StringArray::fromLines (description.fromFirstOccurrenceOf (samplePrefix, false, false));
    }
}

//==============================================================================
/** The asset dock along the bottom of Foundry: square tiles with a waveform
    and the sample's name under it, folders you click into (including your
    own), a breadcrumb, instant indexed search and a list view.

    Click to hear a sample; arrow keys step through a folder auditioning as
    they go; F stars, Delete deletes (only files Foundry made). Ctrl/Shift
    select several; drag them onto a Riff Maker slot, onto a folder tile, or
    straight out of the plugin into the DAW. */
class TileBrowser : public juce::Component,
                    private juce::Timer
{
public:
    std::function<void (int slot, const juce::File&)> onLoadIntoSlot;   // slot -1 = next empty
    std::function<void (const juce::File&)> onOpenRiff;
    std::function<void (const juce::File&)> onOpenPattern;              // pattern file, or a loop with one
    std::function<void (const foundry::LibraryItem&)> onSendToSplicer;
    std::function<void (const foundry::LibraryItem&)> onLockInDrums;
    std::function<void (int row, const foundry::LibraryItem&)> onLockBassSource;
    std::function<void (const juce::String&, bool isError)> onMessage;
    std::function<void()> onOpenSettings;
    std::function<void()> onFilesChanged;

    explicit TileBrowser (FoundryAudioProcessor& p);
    ~TileBrowser() override;

    void setTheme (const Theme& t);

    /** Re-reads the current folder from the library index. */
    void refresh();
    void navigateTo (const foundry::BrowserLocation& where);
    void showFolder (const juce::File& dir);
    const foundry::BrowserLocation& getLocation() const noexcept { return location; }

    /** What the dice may draw from: the selection if several samples are
        selected, the search results when searching, otherwise every sample
        at or below the open folder. */
    std::vector<foundry::LibraryItem> dicePool() const;

    juce::StringArray selectedPaths() const;
    void selectAll();
    void deleteSelection();
    void cullUnfavourited();

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int kTopBarH = 34;

private:
    class Body;
    class Crumbs;

    void timerCallback() override;
    void goUp();
    void setListView (bool list);
    void setShowFolders (bool show);
    void activate (int index, bool doubleClick);
    void clickTile (int index, const juce::ModifierKeys& mods);
    void moveFocus (int delta, bool extend);
    void toggleFavouriteOnSelection();
    void showMenu (int index);
    void showBackgroundMenu();
    void showAddFolderMenu();
    void addFolderFromDisk();
    void createFolderHere();
    void dropFilesOnto (const juce::StringArray& paths, const juce::File& dir);
    void message (const juce::String& m, bool error);
    std::shared_ptr<const foundry::PatternFile> patternFor (const juce::String& path);

    FoundryAudioProcessor& processor;

    juce::TextButton upButton { "<" };
    juce::TextButton foldersButton { "FOLDERS" };
    juce::TextButton tilesButton { "TILES" }, listButton { "LIST" };
    juce::TextButton addFolderButton { "+ FOLDER" };
    juce::TextButton rescanButton { "RESCAN" };
    juce::TextEditor search;
    std::unique_ptr<Crumbs> crumbs;
    juce::Viewport viewport;
    std::unique_ptr<Body> body;
    std::unique_ptr<juce::FileChooser> chooser;

    foundry::BrowserLocation location;
    std::vector<foundry::BrowserEntry> entries;
    std::map<juce::String, std::shared_ptr<const foundry::PatternFile>> patternCache;
    bool truncated = false;
    bool listView = false;
    bool showFolders = true;

    std::set<int> selection;
    int focus = -1, anchor = -1;

    int searchDebounce = 0;
    int lastThumbGeneration = -1;

    Theme theme { getTheme() };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TileBrowser)
};

} // namespace pt
