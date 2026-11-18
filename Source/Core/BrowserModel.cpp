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
        return g == "drums" ? "Drums" : g == "generated" ? "Generated" : g;
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
}

BrowserModel::BrowserModel (ConfigStore& c, LibraryDatabase& d) : config (c), db (d) {}

BrowserLocation BrowserModel::root()
{
    BrowserLocation l;
    l.kind = BrowserLocation::Kind::Root;
    l.title = "Library";
    return l;
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

    e.target.kind = BrowserLocation::Kind::Folder;
    e.target.dir = dir.getFullPathName();
    e.target.title = name;
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

    BrowserEntry fav;
    fav.type = BrowserEntry::Type::Folder;
    fav.name = "Favourites";
    fav.icon = BrowserEntry::Icon::Favourites;
    fav.target.kind = BrowserLocation::Kind::Favourites;
    fav.target.title = fav.name;
    fav.count = db.countFavourites();
    out.push_back (fav);

    for (const juce::String g : { "drums" })
    {
        BrowserEntry e;
        e.type = BrowserEntry::Type::Folder;
        e.name = groupTitle (g);
        e.icon = BrowserEntry::Icon::Drums;
        e.target.kind = BrowserLocation::Kind::Group;
        e.target.group = g;
        e.target.title = e.name;
        for (const auto& d : groupDirs (g))
            e.count += db.countUnderFolder (d.getFullPathName());
        out.push_back (e);
    }

    out.push_back (roleFolder (FolderRole::Bass, "Bass", BrowserEntry::Icon::Bass));
    out.push_back (roleFolder (FolderRole::Riffs, "My Riffs", BrowserEntry::Icon::Riffs));

    BrowserEntry gen;
    gen.type = BrowserEntry::Type::Folder;
    gen.name = groupTitle ("generated");
    gen.icon = BrowserEntry::Icon::Generated;
    gen.target.kind = BrowserLocation::Kind::Group;
    gen.target.group = "generated";
    gen.target.title = gen.name;
    for (const auto& d : groupDirs ("generated"))
        gen.count += db.countUnderFolder (d.getFullPathName());
    out.push_back (gen);

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

std::vector<BrowserEntry> BrowserModel::folderEntries (const juce::String& dir) const
{
    std::vector<BrowserEntry> folders, files;

    auto prefix = dir;
    if (! prefix.endsWithChar (juce::File::getSeparatorChar()))
        prefix += juce::File::getSeparatorString();

    std::map<juce::String, int> subCounts;   // sorted, case-sensitive; re-sorted below

    for (auto& it : db.queryUnderFolder (dir, {}, 0))
    {
        const auto rel = it.path.substring (prefix.length());
        const int sep = rel.indexOfChar (juce::File::getSeparatorChar());

        if (sep > 0)
        {
            ++subCounts[rel.substring (0, sep)];
            continue;
        }

        BrowserEntry e;
        e.type = BrowserEntry::Type::File;
        e.name = juce::File (it.path).getFileNameWithoutExtension();
        e.item = std::move (it);
        files.push_back (std::move (e));
    }

    for (const auto& [name, count] : subCounts)
    {
        BrowserEntry e;
        e.type = BrowserEntry::Type::Folder;
        e.name = name;
        e.icon = BrowserEntry::Icon::Folder;
        e.count = count;
        e.target.kind = BrowserLocation::Kind::Folder;
        e.target.dir = prefix + name;
        e.target.title = name;
        folders.push_back (std::move (e));
    }

    std::sort (folders.begin(), folders.end(),
               [] (const BrowserEntry& a, const BrowserEntry& b) { return a.name.compareNatural (b.name) < 0; });

    folders.insert (folders.end(), std::make_move_iterator (files.begin()), std::make_move_iterator (files.end()));
    return folders;
}

std::vector<BrowserEntry> BrowserModel::list (const BrowserLocation& where, const juce::String& search,
                                              bool& truncated) const
{
    truncated = false;
    const bool searching = search.trim().isNotEmpty();

    if (! searching)
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

    // Favourites, or a search: a flat list of files.
    std::vector<LibraryItem> items;
    switch (where.kind)
    {
        case BrowserLocation::Kind::Favourites:
            items = db.queryFavourites (search, searching ? kSearchLimit : 0);
            break;
        case BrowserLocation::Kind::Folder:
            items = db.queryUnderFolder (where.dir, search, kSearchLimit);
            break;
        case BrowserLocation::Kind::Group:
            for (const auto& d : groupDirs (where.group))
            {
                auto part = db.queryUnderFolder (d.getFullPathName(), search, kSearchLimit);
                items.insert (items.end(), part.begin(), part.end());
            }
            break;
        case BrowserLocation::Kind::Root:
            items = db.queryUnderFolder ({}, search, kSearchLimit);
            break;
    }

    truncated = searching && (int) items.size() >= kSearchLimit;
    if ((int) items.size() > kSearchLimit && searching)
        items.resize ((size_t) kSearchLimit);

    std::vector<BrowserEntry> out;
    out.reserve (items.size());
    for (auto& it : items)
    {
        BrowserEntry e;
        e.type = BrowserEntry::Type::File;
        e.name = juce::File (it.path).getFileNameWithoutExtension();
        e.item = std::move (it);
        out.push_back (std::move (e));
    }
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

    // Folder: find the curated library folder it lives in (deepest match wins).
    const CuratedFolder* best = nullptr;
    juce::String bestDir;
    for (const auto& c : kCurated)
    {
        const auto d = dirForRole (c.role).getFullPathName();
        if (d.isNotEmpty() && isInside (where.dir, d) && d.length() > bestDir.length())
        {
            best = &c;
            bestDir = d;
        }
    }

    if (best == nullptr)
    {
        crumbs.push_back (where);
        return crumbs;
    }

    if (juce::String (best->group).isNotEmpty())
    {
        BrowserLocation g;
        g.kind = BrowserLocation::Kind::Group;
        g.group = best->group;
        g.title = groupTitle (best->group);
        crumbs.push_back (g);
    }

    BrowserLocation top;
    top.kind = BrowserLocation::Kind::Folder;
    top.dir = bestDir;
    top.title = best->name;
    crumbs.push_back (top);

    auto rest = where.dir.substring (bestDir.length());
    auto dir = juce::File (bestDir);
    for (const auto& part : juce::StringArray::fromTokens (rest, juce::File::getSeparatorString(), {}))
    {
        if (part.isEmpty())
            continue;
        dir = dir.getChildFile (part);
        BrowserLocation l;
        l.kind = BrowserLocation::Kind::Folder;
        l.dir = dir.getFullPathName();
        l.title = part;
        crumbs.push_back (l);
    }

    return crumbs;
}

} // namespace foundry
