#pragma once

#include "ConfigStore.h"
#include "LibraryDatabase.h"
#include "Patterns.h"

namespace foundry
{

/** A place in the sample browser. The top level holds curated folders
    (Favourites, Drums, Bass, My Riffs, Batch Generated, Patterns) mapped onto
    the library folders from Settings, plus any folders you add yourself;
    below those you browse the real sub-folders. */
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
    bool isRealFolder() const noexcept { return kind == Kind::Folder && dir.isNotEmpty(); }
};

struct BrowserEntry
{
    enum class Type { Folder, File, Pattern, AddFolder };
    enum class Icon { Folder, Favourites, Drums, Bass, Riffs, Generated, Patterns, Custom };

    Type type = Type::File;
    juce::String name;
    Icon icon = Icon::Folder;
    BrowserLocation target;   // folders: where clicking it goes
    LibraryItem item;         // files (and patterns: path + filename only)
    int count = 0;            // folders: items inside, any depth
    bool unmapped = false;    // a library folder that Settings has not set yet
    bool custom = false;      // a top-level folder the user added

    bool isFolder() const noexcept   { return type == Type::Folder; }
    bool isFile() const noexcept     { return type == Type::File; }
    bool isPattern() const noexcept  { return type == Type::Pattern; }
    bool hasPath() const noexcept    { return type == Type::File || type == Type::Pattern; }
};

/** Answers "what is in this folder" and "find X in here" from the library
    index - never by walking a sample library on disk - so a giant collection
    lists and searches instantly instead of freezing the host. (Patterns are
    small text files in their own folder and are listed from disk.) */
class BrowserModel
{
public:
    static constexpr int kSearchLimit = 500;
    static constexpr int kFlatLimit   = 3000;

    BrowserModel (ConfigStore& config, LibraryDatabase& db);

    /** `showFolders` false flattens the view: every file below `where`. */
    std::vector<BrowserEntry> list (const BrowserLocation& where, const juce::String& search,
                                    bool showFolders, bool& truncated) const;

    /** Every sample at or below a location - the dice draws from this. */
    std::vector<LibraryItem> filesUnder (const BrowserLocation& where) const;

    static BrowserLocation root();
    static BrowserLocation folder (const juce::File& dir, const juce::String& title = {});

    BrowserEntry roleFolder (FolderRole role, const juce::String& name, BrowserEntry::Icon icon) const;

    /** Breadcrumb for a location. */
    std::vector<BrowserLocation> pathTo (const BrowserLocation& where) const;

    bool isPatternLocation (const BrowserLocation& where) const;

private:
    juce::File dirForRole (FolderRole role) const;
    std::vector<juce::File> groupDirs (const juce::String& group) const;
    std::vector<BrowserEntry> rootEntries() const;
    std::vector<BrowserEntry> groupEntries (const juce::String& group) const;
    std::vector<BrowserEntry> folderEntries (const juce::String& dir) const;
    std::vector<BrowserEntry> patternEntries (const juce::File& dir, const juce::String& search, bool recursive) const;
    static BrowserEntry fileEntry (LibraryItem it);

    ConfigStore& config;
    LibraryDatabase& db;
};

} // namespace foundry
