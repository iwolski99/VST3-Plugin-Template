#include "BrowserModel.h"
#include <algorithm>

namespace foundry
{

namespace
{
    struct CuratedFolder
    {
        FolderRole role;
        const char* group;          // "" = top level
        const char* name;
        BrowserEntry::Icon icon;
    };

    const CuratedFolder kCurated[] = {
        { FolderRole::Kick,           "drums",     "Kicks",      BrowserEntry::Icon::Drums },
        { FolderRole::Snare,          "drums",     "Snares",     BrowserEntry::Icon::Drums },
        { FolderRole::Clap,           "drums",     "Claps",      BrowserEntry::Icon::Drums },
        { FolderRole::Hat,            "drums",     "Hats",       BrowserEntry::Icon::Drums },
        { FolderRole::Ride,           "drums",     "Rides",      BrowserEntry::Icon::Drums },
        { FolderRole::Bass,           "",          "Bass",       BrowserEntry::Icon::Bass },
        { FolderRole::Riffs,          "",          "My Riffs",   BrowserEntry::Icon::Riffs },
        { FolderRole::GeneratedDrums, "generated", "Drum Loops", BrowserEntry::Icon::Generated },
        { FolderRole::GeneratedBass,  "generated", "Bass Loops", BrowserEntry::Icon::Generated },
    };

    juce::String groupTitle (const juce::String& g)
    {
        return g == "drums" ? "Drums" : g == "generated" ? "Batch Generated" : g;
    }

    bool isInside (const juce::String& path, const juce::String& dir)
    {
        if (dir.isEmpty())
            return false;
        auto prefix = dir;
        if (! prefix.endsWithChar (juce::File::getSeparatorChar()))
            prefix += juce::File::getSeparatorString();
        return path == dir || path.startsWith (prefix);
    }

    bool byNaturalName (const BrowserEntry& a, const BrowserEntry& b)
    {
        return a.name.compareNatural (b.name) < 0;
    }
}

BrowserModel::BrowserModel (ConfigStore& c, LibraryDatabase& d) : config (c), db (d) {}

BrowserLocation BrowserModel::root()
{
    BrowserLocation l;
    l.kind = BrowserLocation::Kind::Root;
    l.title = "Library";
    return l;
}

BrowserLocation BrowserModel::folder (const juce::File& dir, const juce::String& title)
{
    BrowserLocation l;
    l.kind = BrowserLocation::Kind::Folder;
    l.dir = dir.getFullPathName();
    l.title = title.isNotEmpty() ? title : dir.getFileName();
    return l;
}

BrowserEntry BrowserModel::fileEntry (LibraryItem it)
{
    BrowserEntry e;
    e.type = BrowserEntry::Type::File;
    e.name = juce::File (it.path).getFileNameWithoutExtension();
    e.item = std::move (it);
    return e;
}

bool BrowserModel::isPatternLocation (const BrowserLocation& where) const
{
    return where.isRealFolder() && isInside (where.dir, config.patternsRoot().getFullPathName());
}

juce::File BrowserModel::dirForRole (FolderRole role) const
{
    auto mapped = config.folder (role);
    if (mapped.isDirectory())
        return mapped;

    if (role == FolderRole::Riffs)
        return config.riffDir();

    auto r = config.root();
    if (r.isDirectory())
    {
        if (role == FolderRole::GeneratedDrums) return r.getChildFile ("Generated").getChildFile ("Drum_Loops");
        if (role == FolderRole::GeneratedBass)  return r.getChildFile ("Generated").getChildFile ("Bass_Loops");
    }
    return {};
}

BrowserEntry BrowserModel::roleFolder (FolderRole role, const juce::String& name, BrowserEntry::Icon icon) const
{
    BrowserEntry e;
    e.type = BrowserEntry::Type::Folder;
    e.name = name;
    e.icon = icon;

    const auto dir = dirForRole (role);
    if (dir.getFullPathName().isEmpty())
    {
        e.unmapped = true;
        return e;
    }

    e.target = folder (dir, name);
    e.count = db.countUnderFolder (e.target.dir);
    return e;
}

std::vector<juce::File> BrowserModel::groupDirs (const juce::String& group) const
{
    std::vector<juce::File> out;
    for (const auto& c : kCurated)
        if (group == c.group)
            if (auto d = dirForRole (c.role); d.getFullPathName().isNotEmpty())
                out.push_back (d);
    return out;
}

std::vector<BrowserEntry> BrowserModel::rootEntries() const
{
    std::vector<BrowserEntry> out;

    auto groupEntry = [this] (const juce::String& g, BrowserEntry::Icon icon)
    {
        BrowserEntry e;
        e.type = BrowserEntry::Type::Folder;
        e.name = groupTitle (g);
        e.icon = icon;
        e.target.kind = BrowserLocation::Kind::Group;
        e.target.group = g;
        e.target.title = e.name;
        for (const auto& d : groupDirs (g))
            e.count += db.countUnderFolder (d.getFullPathName());
        return e;
    };

    BrowserEntry fav;
    fav.type = BrowserEntry::Type::Folder;
    fav.name = "Favourites";
    fav.icon = BrowserEntry::Icon::Favourites;
    fav.target.kind = BrowserLocation::Kind::Favourites;
    fav.target.title = fav.name;
    fav.count = db.countFavourites();
    out.push_back (fav);

    out.push_back (groupEntry ("drums", BrowserEntry::Icon::Drums));
    out.push_back (roleFolder (FolderRole::Bass, "Bass", BrowserEntry::Icon::Bass));
    out.push_back (roleFolder (FolderRole::Riffs, "My Riffs", BrowserEntry::Icon::Riffs));
    out.push_back (groupEntry ("generated", BrowserEntry::Icon::Generated));

    BrowserEntry pat;
    pat.type = BrowserEntry::Type::Folder;
    pat.name = "Patterns";
    pat.icon = BrowserEntry::Icon::Patterns;
    pat.target = folder (config.patternsRoot(), "Patterns");
    pat.count = config.patternsRoot().getNumberOfChildFiles (juce::File::findFiles | juce::File::ignoreHiddenFiles,
                                                              juce::String ("*") + kPatternExtension);
    pat.count += config.patternDir (false).getNumberOfChildFiles (juce::File::findFiles, juce::String ("*") + kPatternExtension);
    pat.count += config.patternDir (true).getNumberOfChildFiles (juce::File::findFiles, juce::String ("*") + kPatternExtension);
    out.push_back (pat);

    for (const auto& path : config.get().userFolders)
    {
        const juce::File dir (path);
        BrowserEntry e;
        e.type = BrowserEntry::Type::Folder;
        e.name = dir.getFileName();
        e.icon = BrowserEntry::Icon::Custom;
        e.custom = true;
        e.unmapped = ! dir.isDirectory();
        e.target = folder (dir);
        e.count = db.countUnderFolder (path);
        out.push_back (e);
    }

    BrowserEntry add;
    add.type = BrowserEntry::Type::AddFolder;
    add.name = "Add folder";
    out.push_back (add);

    return out;
}

std::vector<BrowserEntry> BrowserModel::groupEntries (const juce::String& group) const
{
    std::vector<BrowserEntry> out;
    for (const auto& c : kCurated)
        if (group == c.group)
            out.push_back (roleFolder (c.role, c.name, c.icon));
    return out;
}

std::vector<BrowserEntry> BrowserModel::patternEntries (const juce::File& dir, const juce::String& search,
                                                         bool recursive) const
{
    std::vector<BrowserEntry> folders, files;
    const auto wildcard = juce::String ("*") + kPatternExtension;

    if (! recursive)
    {
        for (const auto& sub : dir.findChildFiles (juce::File::findDirectories | juce::File::ignoreHiddenFiles, false))
        {
            BrowserEntry e;
            e.type = BrowserEntry::Type::Folder;
            e.name = sub.getFileName();
            e.icon = BrowserEntry::Icon::Patterns;
            e.target = folder (sub);
            e.count = sub.getNumberOfChildFiles (juce::File::findFiles, wildcard);
            folders.push_back (e);
        }
    }

    for (const auto& f : dir.findChildFiles (juce::File::findFiles | juce::File::ignoreHiddenFiles, recursive, wildcard))
    {
        if (search.trim().isNotEmpty() && ! f.getFileNameWithoutExtension().containsIgnoreCase (search.trim()))
            continue;
        BrowserEntry e;
        e.type = BrowserEntry::Type::Pattern;
        e.name = f.getFileNameWithoutExtension();
        e.item.path = f.getFullPathName();
        e.item.filename = f.getFileName();
        files.push_back (e);
    }

    std::sort (folders.begin(), folders.end(), byNaturalName);
    std::sort (files.begin(), files.end(), byNaturalName);
    folders.insert (folders.end(), files.begin(), files.end());
    return folders;
}

std::vector<BrowserEntry> BrowserModel::folderEntries (const juce::String& dir) const
{
    std::vector<BrowserEntry> folders, files;

    auto prefix = dir;
    if (! prefix.endsWithChar (juce::File::getSeparatorChar()))
        prefix += juce::File::getSeparatorString();

    std::map<juce::String, int> subCounts;

    for (auto& it : db.queryUnderFolder (dir, {}, 0))
    {
        const auto rel = it.path.substring (prefix.length());
        const int sep = rel.indexOfChar (juce::File::getSeparatorChar());

        if (sep > 0)
        {
            ++subCounts[rel.substring (0, sep)];
            continue;
        }
        files.push_back (fileEntry (std::move (it)));
    }

    // Empty sub-folders (e.g. one you just created) are worth showing too.
    for (const auto& sub : juce::File (dir).findChildFiles (juce::File::findDirectories | juce::File::ignoreHiddenFiles, false))
        subCounts.emplace (sub.getFileName(), 0);

    for (const auto& [name, count] : subCounts)
    {
        BrowserEntry e;
        e.type = BrowserEntry::Type::Folder;
        e.name = name;
        e.icon = BrowserEntry::Icon::Folder;
        e.count = count;
        e.target = folder (juce::File (prefix + name), name);
        folders.push_back (std::move (e));
    }

    std::sort (folders.begin(), folders.end(), byNaturalName);
    folders.insert (folders.end(), std::make_move_iterator (files.begin()), std::make_move_iterator (files.end()));
    return folders;
}

std::vector<BrowserEntry> BrowserModel::list (const BrowserLocation& where, const juce::String& search,
                                              bool showFolders, bool& truncated) const
{
    truncated = false;
    const bool searching = search.trim().isNotEmpty();

    if (isPatternLocation (where))
        return patternEntries (juce::File (where.dir), search, searching || ! showFolders);

    if (! searching && showFolders)
    {
        switch (where.kind)
        {
            case BrowserLocation::Kind::Root:   return rootEntries();
            case BrowserLocation::Kind::Group:  return groupEntries (where.group);
            case BrowserLocation::Kind::Folder: return folderEntries (where.dir);
            case BrowserLocation::Kind::Favourites:
                break;
        }
    }

    // Favourites, a search, or folders hidden: a flat list of files.
    const int limit = searching ? kSearchLimit : kFlatLimit;
    std::vector<LibraryItem> items;
    switch (where.kind)
    {
        case BrowserLocation::Kind::Favourites:
            items = db.queryFavourites (search, limit);
            break;
        case BrowserLocation::Kind::Folder:
            items = db.queryUnderFolder (where.dir, search, limit);
            break;
        case BrowserLocation::Kind::Group:
            for (const auto& d : groupDirs (where.group))
            {
                auto part = db.queryUnderFolder (d.getFullPathName(), search, limit);
                items.insert (items.end(), part.begin(), part.end());
            }
            break;
        case BrowserLocation::Kind::Root:
            items = db.queryUnderFolder ({}, search, limit);
            break;
    }

    truncated = (int) items.size() >= limit;
    if ((int) items.size() > limit)
        items.resize ((size_t) limit);

    std::vector<BrowserEntry> out;
    out.reserve (items.size());
    for (auto& it : items)
        out.push_back (fileEntry (std::move (it)));

    std::sort (out.begin(), out.end(), byNaturalName);
    return out;
}

std::vector<LibraryItem> BrowserModel::filesUnder (const BrowserLocation& where) const
{
    switch (where.kind)
    {
        case BrowserLocation::Kind::Favourites: return db.queryFavourites ({}, 0);
        case BrowserLocation::Kind::Folder:     return db.queryUnderFolder (where.dir, {}, 0);
        case BrowserLocation::Kind::Root:       return db.queryUnderFolder ({}, {}, 0);
        case BrowserLocation::Kind::Group:
        {
            std::vector<LibraryItem> out;
            for (const auto& d : groupDirs (where.group))
            {
                auto part = db.queryUnderFolder (d.getFullPathName(), {}, 0);
                out.insert (out.end(), part.begin(), part.end());
            }
            return out;
        }
    }
    return {};
}

std::vector<BrowserLocation> BrowserModel::pathTo (const BrowserLocation& where) const
{
    std::vector<BrowserLocation> crumbs { root() };

    if (where.kind == BrowserLocation::Kind::Root)
        return crumbs;

    if (where.kind == BrowserLocation::Kind::Favourites || where.kind == BrowserLocation::Kind::Group)
    {
        crumbs.push_back (where);
        return crumbs;
    }

    // Folder: find the top-level browser folder it lives in (deepest match wins).
    struct Top { juce::String dir, name, group; };
    std::vector<Top> tops;
    for (const auto& c : kCurated)
        if (auto d = dirForRole (c.role).getFullPathName(); d.isNotEmpty())
            tops.push_back ({ d, c.name, c.group });
    tops.push_back ({ config.patternsRoot().getFullPathName(), "Patterns", {} });
    for (const auto& u : config.get().userFolders)
        tops.push_back ({ u, juce::File (u).getFileName(), {} });

    const Top* best = nullptr;
    for (const auto& t : tops)
        if (isInside (where.dir, t.dir) && (best == nullptr || t.dir.length() > best->dir.length()))
            best = &t;

    if (best == nullptr)
    {
        crumbs.push_back (where);
        return crumbs;
    }

    if (best->group.isNotEmpty())
    {
        BrowserLocation g;
        g.kind = BrowserLocation::Kind::Group;
        g.group = best->group;
        g.title = groupTitle (best->group);
        crumbs.push_back (g);
    }

    crumbs.push_back (folder (juce::File (best->dir), best->name));

    auto rest = where.dir.substring (best->dir.length());
    auto dir = juce::File (best->dir);
    for (const auto& part : juce::StringArray::fromTokens (rest, juce::File::getSeparatorString(), {}))
    {
        if (part.isEmpty())
            continue;
        dir = dir.getChildFile (part);
        crumbs.push_back (folder (dir, part));
    }

    return crumbs;
}

} // namespace foundry
