#pragma once

#include "ConfigStore.h"
#include "LibraryDatabase.h"

namespace foundry
{

/** A place in the sample browser. The top levels are fixed, curated folders
    (Favourites, Drums, Bass, My Riffs, Generated) mapped onto the library
    folders from Settings; below those you browse the real sub-folders. */
struct BrowserLocation
{
    enum class Kind { Root, Favourites, Group, Folder };

    Kind kind = Kind::Root;
    juce::String group;       // Group: "drums" or "generated"
    juce::String dir;         // Folder: absolute path
    juce::String title;

    bool operator== (const BrowserLocation& o) const noexcept
    {
        return kind == o.kind && group == o.group && dir == o.dir;
    }
};

struct BrowserEntry
{
    enum class Type { Folder, File };
    enum class Icon { Folder, Favourites, Drums, Bass, Riffs, Generated };

    Type type = Type::File;
    juce::String name;
    Icon icon = Icon::Folder;
    BrowserLocation target;   // folders: where clicking it goes
    LibraryItem item;         // files
    int count = 0;            // folders: number of samples inside, any depth
    bool unmapped = false;    // a library folder that Settings has not set yet

    bool isFolder() const noexcept { return type == Type::Folder; }
};

/** Answers "what is in this folder" and "find X in here" straight from the
    library index - never by walking the disk - so a giant sample collection
    lists and searches instantly instead of freezing the host. */
class BrowserModel
{
public:
    static constexpr int kSearchLimit = 500;

    BrowserModel (ConfigStore& config, LibraryDatabase& db);

    std::vector<BrowserEntry> list (const BrowserLocation& where, const juce::String& search,
                                    bool& truncated) const;

    /** Every sample at or below a location - the dice draws from this. */
    std::vector<LibraryItem> filesUnder (const BrowserLocation& where) const;

    static BrowserLocation root();

    /** The folder location for a library role, or an "unmapped" placeholder. */
    BrowserEntry roleFolder (FolderRole role, const juce::String& name, BrowserEntry::Icon icon) const;

    /** Breadcrumb for a folder: the curated parents plus the sub-folders
        between the library folder and `dir`. */
    std::vector<BrowserLocation> pathTo (const BrowserLocation& where) const;

private:
    juce::File dirForRole (FolderRole role) const;
    std::vector<juce::File> groupDirs (const juce::String& group) const;
    std::vector<BrowserEntry> rootEntries() const;
    std::vector<BrowserEntry> groupEntries (const juce::String& group) const;
    std::vector<BrowserEntry> folderEntries (const juce::String& dir) const;

    ConfigStore& config;
    LibraryDatabase& db;
};

} // namespace foundry
