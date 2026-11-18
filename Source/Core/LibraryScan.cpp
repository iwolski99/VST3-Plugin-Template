#include "LibraryScan.h"
#include <algorithm>
#include <set>

namespace foundry
{

namespace
{

    Category inferGeneratedCategory (FolderRole role, const juce::File& file, const juce::File& root)
    {
        const auto rel = file.getRelativePathFrom (root).replaceCharacter ('\\', '/');

        if (role == FolderRole::GeneratedDrums)
        {
            if (rel.containsIgnoreCase ("Dubstep"))
                return Category::DrumLoopsDubstep;
            return Category::DrumLoopsRiddim;
        }

        if (role == FolderRole::Riffs)
            return Category::Riff;

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
    for (int i = 0; i < kNumFolderRoles; ++i)
    {
        const auto role = (FolderRole) i;
        auto dir = config.folder (role);

        // Rendered riffs always live somewhere, mapped or not.
        if (role == FolderRole::Riffs && ! dir.isDirectory())
            dir = config.riffDir();

        if (dir.isDirectory())
            roots.push_back ({ role, dir });
    }

    const bool anySourceMapped = std::any_of (roots.begin(), roots.end(),
                                              [] (const auto& r) { return r.first != FolderRole::Riffs; });
    if (! anySourceMapped && config.get().userFolders.isEmpty())
    {
        out.error = "No source folders are mapped. Open Settings and assign folders, or create the recommended structure.";
        return out;
    }

    juce::Array<juce::File> files;
    juce::Array<FolderRole> roles;
    juce::Array<bool> custom;   // true: a folder the user added in the browser

    for (const auto& r : roots)
    {
        auto found = r.second.findChildFiles (juce::File::findFiles, true, audioFileWildcard());
        for (auto& f : found)
        {
            files.add (f);
            roles.add (r.first);
            custom.add (false);
        }
    }

    for (const auto& path : config.get().userFolders)
    {
        const juce::File dir (path);
        if (! dir.isDirectory())
            continue;
        for (auto& f : dir.findChildFiles (juce::File::findFiles, true, audioFileWildcard()))
        {
            files.add (f);
            roles.add (FolderRole::Kick);   // unused for custom folders
            custom.add (true);
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
        if (! isSupportedAudioFile (f))
            continue;

        if (seen.count (f.getFullPathName()) != 0)
            continue;   // inside both a library folder and an added folder

        const auto role = roles.getUnchecked (i);
        const bool isCustom = custom.getUnchecked (i);
        auto cat = isCustom ? Category::Unknown : inferGeneratedCategory (role, f, config.folder (role));

        LibraryItem it;
        bool alreadyIndexed = false;
        if (auto existing = db.findByPath (f.getFullPathName()))
        {
            it = *existing;
            alreadyIndexed = true;
        }

        // A file moved into one of your own folders keeps what it was
        // (a kick stays a kick, a riff stays a riff).
        if (isCustom && alreadyIndexed && it.category != Category::Unknown)
            cat = it.category;

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
        if (alreadyIndexed)
            ++out.kept;
        else
            ++out.added;

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
