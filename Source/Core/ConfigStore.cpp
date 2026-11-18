#include "ConfigStore.h"

namespace foundry
{

ConfigStore::ConfigStore() = default;

juce::File ConfigStore::root() const
{
    return juce::File (config.rootPath);
}

void ConfigStore::setRoot (const juce::File& dir)
{
    config.rootPath = dir.getFullPathName();
}

juce::File ConfigStore::folder (FolderRole role) const
{
    const auto idx = (size_t) role;
    if (idx >= config.folderPaths.size())
        return {};

    const auto& p = config.folderPaths[idx];
    return p.isNotEmpty() ? juce::File (p) : juce::File();
}

void ConfigStore::setFolder (FolderRole role, const juce::File& dir)
{
    const auto idx = (size_t) role;
    if (idx < config.folderPaths.size())
        config.folderPaths[idx] = dir.getFullPathName();
}

juce::File ConfigStore::databaseFile() const
{
    auto r = root();

    if (r.isDirectory())
    {
        auto dbDir = r.getChildFile ("Database");
        if (! dbDir.isDirectory())
            dbDir.createDirectory();

        return dbDir.getChildFile ("library.db");
    }

    return defaultDatabaseFile();
}

juce::File ConfigStore::generatedDrumDir (Genre g) const
{
    auto mapped = folder (FolderRole::GeneratedDrums);
    auto base = mapped.isDirectory() ? mapped
                                     : root().getChildFile ("Generated").getChildFile ("Drum_Loops");
    return base.getChildFile (genreName (g));
}

juce::File ConfigStore::generatedBassDir (bool spliced) const
{
    auto mapped = folder (FolderRole::GeneratedBass);
    auto base = mapped.isDirectory() ? mapped
                                     : root().getChildFile ("Generated").getChildFile ("Bass_Loops");
    return base.getChildFile (spliced ? "Spliced" : "Base");
}

juce::File ConfigStore::riffDir() const
{
    auto mapped = folder (FolderRole::Riffs);
    if (mapped.isDirectory())
        return mapped;

    auto r = root();
    auto dir = r.isDirectory() ? r.getChildFile ("Riffs") : appDataDir().getChildFile ("Riffs");
    if (! dir.isDirectory())
        dir.createDirectory();
    return dir;
}

double ConfigStore::resolveBpm (Genre g, double hostBpm) const
{
    switch (config.tempoMode)
    {
        case TempoMode::Host:
            return hostBpm > 20.0 && hostBpm < 400.0 ? hostBpm : genreDefaultBpm (g);
        case TempoMode::Custom:
            return juce::jlimit (40.0, 300.0, config.customBpm);
        case TempoMode::GenreDefault:
        default:
            return genreDefaultBpm (g);
    }
}

double ConfigStore::resolveRenderSampleRate (double hostSr) const
{
    if (config.sampleRateMode == "44100")
        return 44100.0;
    if (config.sampleRateMode == "48000")
        return 48000.0;

    return hostSr > 1000.0 ? hostSr : 48000.0;
}

bool ConfigStore::createRecommendedStructure (juce::String& error)
{
    auto r = root();
    if (! r.isDirectory())
    {
        error = "Choose a root library folder first.";
        return false;
    }

    const std::pair<FolderRole, juce::String> map[] = {
        { FolderRole::Kick,           "Sources/Drums/Kicks" },
        { FolderRole::Snare,          "Sources/Drums/Snares" },
        { FolderRole::Clap,           "Sources/Drums/Claps" },
        { FolderRole::Hat,            "Sources/Drums/Closed_Hats" },
        { FolderRole::Ride,           "Sources/Drums/Rides" },
        { FolderRole::Bass,           "Sources/Bass_OneShots" },
        { FolderRole::GeneratedDrums, "Generated/Drum_Loops" },
        { FolderRole::GeneratedBass,  "Generated/Bass_Loops" },
        { FolderRole::Riffs,          "Riffs" },
    };

    for (const auto& m : map)
    {
        auto dir = r.getChildFile (m.second);
        if (! dir.createDirectory() && ! dir.isDirectory())
        {
            error = "Could not create " + dir.getFullPathName();
            return false;
        }
        setFolder (m.first, dir);
    }

    auto dbDir = r.getChildFile ("Database");
    if (! dbDir.createDirectory() && ! dbDir.isDirectory())
    {
        error = "Could not create Database folder.";
        return false;
    }

    r.getChildFile ("Generated").getChildFile ("Drum_Loops").getChildFile ("Riddim").createDirectory();
    r.getChildFile ("Generated").getChildFile ("Drum_Loops").getChildFile ("Dubstep").createDirectory();
    r.getChildFile ("Generated").getChildFile ("Bass_Loops").getChildFile ("Base").createDirectory();
    r.getChildFile ("Generated").getChildFile ("Bass_Loops").getChildFile ("Spliced").createDirectory();

    return true;
}

void ConfigStore::saveCustomPattern (const juce::String& name, const DrumPattern& p)
{
    const auto n = name.trim();
    if (n.isEmpty())
        return;

    const int existing = config.customPatternNames.indexOf (n);
    const auto json = p.toJson();

    if (existing >= 0)
    {
        config.customPatternJson.set (existing, json);
    }
    else
    {
        config.customPatternNames.add (n);
        config.customPatternJson.add (json);
    }
}

bool ConfigStore::loadCustomPattern (const juce::String& name, DrumPattern& p) const
{
    const int i = config.customPatternNames.indexOf (name);
    if (i < 0 || i >= config.customPatternJson.size())
        return false;

    p = DrumPattern::fromJson (config.customPatternJson[i], config.defaultBars);
    return true;
}

bool ConfigStore::load (const juce::File& file, juce::String& error)
{
    lastFile = file;

    if (! file.existsAsFile())
        return true;

    auto parsed = juce::JSON::parse (file);
    auto* obj = parsed.getDynamicObject();
    if (obj == nullptr)
    {
        error = "Config file is not valid JSON.";
        return false;
    }

    config.rootPath = obj->getProperty ("rootDirectory").toString();
    config.defaultGenre = genreFromName (obj->getProperty ("defaultGenre").toString());
    config.tempoMode = (TempoMode) juce::jlimit (0, 2, (int) obj->getProperty ("tempoMode"));
    config.customBpm = (double) obj->getProperty ("customBpm");
    if (config.customBpm < 40.0)
        config.customBpm = kRiddimBpm;
    config.defaultBars = obj->hasProperty ("defaultBars")
        ? juce::jlimit (1, 4, (int) obj->getProperty ("defaultBars"))
        : 2;
    config.defaultQuantity = obj->hasProperty ("defaultQuantity")
        ? juce::jlimit (kMinQuantity, kMaxQuantity, (int) obj->getProperty ("defaultQuantity"))
        : 10;
    config.previewVolume = juce::jlimit (0.0f, 1.0f, (float) (double) obj->getProperty ("previewVolume"));
    if (! obj->hasProperty ("previewVolume"))
        config.previewVolume = 0.80f;
    config.sampleRateMode = obj->getProperty ("sampleRateMode").toString();
    if (config.sampleRateMode.isEmpty())
        config.sampleRateMode = "host";
    config.bitDepth = 24;
    config.renderProtection = obj->getProperty ("renderProtection");
    if (! obj->hasProperty ("renderProtection"))
        config.renderProtection = true;
    config.followTransport = obj->getProperty ("followTransport");
    if (! obj->hasProperty ("followTransport"))
        config.followTransport = true;
    config.uiZoom = juce::jlimit (50, 200, (int) obj->getProperty ("uiZoom"));
    if (config.uiZoom == 0)
        config.uiZoom = 100;
    // Older versions stored 0..3 for Library/Drums/Bass/Splicer; anything past
    // the first tab now lives under Generator.
    config.lastTab = (int) obj->getProperty ("lastTab") > 0 ? 1 : 0;
    config.browserListView = (bool) obj->getProperty ("browserListView");
    config.savedDrumPatternJson = obj->getProperty ("savedDrumPattern").toString();

    if (auto* folders = obj->getProperty ("folders").getDynamicObject())
    {
        for (int i = 0; i < kNumFolderRoles; ++i)
        {
            const auto key = folderRoleKey ((FolderRole) i);
            config.folderPaths[(size_t) i] = folders->getProperty (key).toString();
        }
    }

    config.customPatternNames.clear();
    config.customPatternJson.clear();

    if (auto* arr = obj->getProperty ("customPatterns").getArray())
    {
        for (const auto& v : *arr)
        {
            if (auto* p = v.getDynamicObject())
            {
                config.customPatternNames.add (p->getProperty ("name").toString());
                config.customPatternJson.add (p->getProperty ("json").toString());
            }
        }
    }

    return true;
}

bool ConfigStore::savePreferences (const juce::File& file, juce::String& error) const
{
    // Re-read what is on disk and only overwrite the "last used" values this
    // instance owns. Every plugin instance loads the config when it is created
    // and saves when it is destroyed, so writing the whole struct here would
    // let an instance opened earlier silently revert library folders another
    // instance changed in Settings since.
    ConfigStore onDisk;
    juce::String loadErr;
    if (! onDisk.load (file, loadErr))
        return save (file, error);

    auto& d = onDisk.get();
    d.defaultGenre         = config.defaultGenre;
    d.defaultBars          = config.defaultBars;
    d.defaultQuantity      = config.defaultQuantity;
    d.previewVolume        = config.previewVolume;
    d.followTransport      = config.followTransport;
    d.uiZoom               = config.uiZoom;
    d.lastTab              = config.lastTab;
    d.browserListView      = config.browserListView;
    d.savedDrumPatternJson = config.savedDrumPatternJson;
    return onDisk.save (file, error);
}

bool ConfigStore::save (const juce::File& file, juce::String& error) const
{
    auto obj = juce::DynamicObject::Ptr (new juce::DynamicObject());
    obj->setProperty ("rootDirectory", config.rootPath);
    obj->setProperty ("defaultGenre", genreName (config.defaultGenre));
    obj->setProperty ("tempoMode", (int) config.tempoMode);
    obj->setProperty ("customBpm", config.customBpm);
    obj->setProperty ("defaultBars", config.defaultBars);
    obj->setProperty ("defaultQuantity", config.defaultQuantity);
    obj->setProperty ("previewVolume", config.previewVolume);
    obj->setProperty ("sampleRateMode", config.sampleRateMode);
    obj->setProperty ("bitDepth", 24);
    obj->setProperty ("renderProtection", config.renderProtection);
    obj->setProperty ("followTransport", config.followTransport);
    obj->setProperty ("uiZoom", config.uiZoom);
    obj->setProperty ("lastTab", config.lastTab);
    obj->setProperty ("browserListView", config.browserListView);
    obj->setProperty ("savedDrumPattern", config.savedDrumPatternJson);

    auto folders = juce::DynamicObject::Ptr (new juce::DynamicObject());
    for (int i = 0; i < kNumFolderRoles; ++i)
        folders->setProperty (folderRoleKey ((FolderRole) i), config.folderPaths[(size_t) i]);
    obj->setProperty ("folders", juce::var (folders.get()));

    juce::Array<juce::var> patterns;
    for (int i = 0; i < config.customPatternNames.size(); ++i)
    {
        auto p = juce::DynamicObject::Ptr (new juce::DynamicObject());
        p->setProperty ("name", config.customPatternNames[i]);
        p->setProperty ("json", i < config.customPatternJson.size() ? config.customPatternJson[i]
                                                                    : juce::String());
        patterns.add (juce::var (p.get()));
    }
    obj->setProperty ("customPatterns", patterns);

    const auto text = juce::JSON::toString (juce::var (obj.get()), true);

    if (! file.getParentDirectory().isDirectory())
        file.getParentDirectory().createDirectory();

    if (! file.replaceWithText (text))
    {
        error = "Could not write config to " + file.getFullPathName();
        return false;
    }

    return true;
}

} // namespace foundry
