#include "LibraryScan.h"
#include <set>

namespace foundry
{

namespace
{
    bool isWav (const juce::File& f)
    {
        return f.hasFileExtension (".wav");
    }

    Category inferGeneratedCategory (FolderRole role, const juce::File& file, const juce::File& root)
    {
        const auto rel = file.getRelativePathFrom (root).replaceCharacter ('\\', '/');

        if (role == FolderRole::GeneratedDrums)
        {
            if (rel.containsIgnoreCase ("Dubstep"))
                return Category::DrumLoopsDubstep;
            return Category::DrumLoopsRiddim;
        }

        if (role == FolderRole::GeneratedBass)
        {
            if (rel.containsIgnoreCase ("Spliced"))
                return Category::BassLoopsSpliced;
            return Category::BassLoopsBase;
        }

        return categoryForRole (role);
    }

    juce::String inferType (Category c)
    {
        return isGeneratedCategory (c) ? "loop" : "oneshot";
    }
}

ScanOutcome scanLibrary (ConfigStore& config, LibraryDatabase& db,
                         std::function<bool()> cancel,
                         std::function<void (int done, int total, juce::String status)> progress)
{
    ScanOutcome out;

    if (! db.isOpen())
    {
        out.error = "Library database is not open.";
        return out;
    }

    std::vector<std::pair<FolderRole, juce::File>> roots;
    for (int i = 0; i < 8; ++i)
    {
        const auto role = (FolderRole) i;
        auto dir = config.folder (role);
        if (dir.isDirectory())
            roots.push_back ({ role, dir });
    }

    if (roots.empty())
    {
        out.error = "No source folders are mapped. Open Settings and assign folders, or create the recommended structure.";
        return out;
    }

    juce::Array<juce::File> files;
    juce::Array<FolderRole> roles;

    for (const auto& r : roots)
    {
        auto found = r.second.findChildFiles (juce::File::findFiles, true, "*.wav");
        for (auto& f : found)
        {
            files.add (f);
            roles.add (r.first);
        }
    }

    const int total = files.size();
    if (progress)
        progress (0, juce::jmax (1, total), "Refreshing library");

    std::set<juce::String> seen;

    for (int i = 0; i < files.size(); ++i)
    {
        if (cancel && cancel())
        {
            out.ok = true;
            out.error = "Scan cancelled.";
            return out;
        }

        const auto& f = files.getReference (i);
        if (! isWav (f))
            continue;

        const auto role = roles.getUnchecked (i);
        const auto cat = inferGeneratedCategory (role, f, roots.empty() ? f.getParentDirectory()
                                                                        : config.folder (role));

        LibraryItem it;
        if (auto existing = db.findByPath (f.getFullPathName()))
            it = *existing;

        it.filename = f.getFileName();
        it.path = f.getFullPathName();
        it.fileType = it.fileType.isNotEmpty() ? it.fileType : inferType (cat);
        it.category = cat;
        if (it.dateCreated == 0)
            it.dateCreated = f.getCreationTime().toMilliseconds();
        it.dateIndexed = juce::Time::currentTimeMillis();
        it.existsOnDisk = true;
        if (it.generationType.isEmpty())
            it.generationType = isGeneratedCategory (cat) ? "generated" : "source";

        juce::String err;
        if (! db.upsert (it, err))
        {
            out.error = err;
            return out;
        }

        seen.insert (it.path);
        if (it.id == 0)
            ++out.added;
        else
            ++out.kept;

        if (progress && (i % 25 == 0 || i + 1 == total))
            progress (i + 1, juce::jmax (1, total), "Refreshing library");
    }

    auto indexed = db.allIndexedPaths();
    for (const auto& p : indexed)
    {
        if (seen.count (p) == 0)
        {
            const juce::File f (p);
            if (! f.existsAsFile())
            {
                db.markMissing (p);
                ++out.removed;
            }
        }
    }

    out.ok = true;
    return out;
}

} // namespace foundry
