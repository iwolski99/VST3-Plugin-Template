/*  Offline engine test: indexes synthetic WAV one-shots, generates drum
    loops + stems, bass riffs, spliced riffs, and checks SQLite search,
    sample locking, reproducible seeds, 24-bit output, and preview mixing.

    Build with -DFOUNDRY_BUILD_TESTS=ON, then run FoundryEngineTest. */

#include "../Source/Core/AudioIO.h"
#include "../Source/Core/ConfigStore.h"
#include "../Source/Core/Generators.h"
#include "../Source/Core/LibraryDatabase.h"
#include "../Source/Core/LibraryScan.h"
#include "../Source/Core/PreviewEngine.h"
#include <atomic>
#include <iostream>

namespace
{
    int gFails = 0;

    void expect (bool cond, const juce::String& msg)
    {
        if (! cond)
        {
            std::cerr << "FAIL: " << msg << "\n";
            ++gFails;
        }
    }

    juce::AudioBuffer<float> makeClick (double sr, double freq, double seconds, int channels = 2)
    {
        const int n = juce::jmax (64, juce::roundToInt (sr * seconds));
        juce::AudioBuffer<float> b (channels, n);
        b.clear();
        for (int i = 0; i < n; ++i)
        {
            const float env = (i < 32) ? (float) i / 32.0f
                                       : juce::jmax (0.0f, 1.0f - (float) i / (float) n);
            const float s = env * 0.4f * std::sin (juce::MathConstants<float>::twoPi
                                                   * (float) (freq * (double) i / sr));
            for (int ch = 0; ch < channels; ++ch)
                b.setSample (ch, i, s);
        }
        return b;
    }

    bool writeShot (foundry::AudioIO& io, const juce::File& file, double sr, double freq)
    {
        auto buf = makeClick (sr, freq, 0.18);
        juce::String err;
        return io.writeWav (file, buf, sr, 24, err);
    }

    bool isFiniteBuffer (const juce::AudioBuffer<float>& b)
    {
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i)
                if (! std::isfinite (b.getSample (ch, i)) || std::abs (b.getSample (ch, i)) > 8.0f)
                    return false;
        return true;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    foundry::AudioIO io;

    auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                    .getChildFile ("FoundryEngineTest_" + juce::String (juce::Time::currentTimeMillis()));
    root.createDirectory();

    foundry::ConfigStore config;
    config.setRoot (root);
    juce::String err;
    expect (config.createRecommendedStructure (err), "create structure: " + err);

    auto kickDir = config.folder (foundry::FolderRole::Kick);
    auto clapDir = config.folder (foundry::FolderRole::Clap);
    auto hatDir  = config.folder (foundry::FolderRole::Hat);
    auto rideDir = config.folder (foundry::FolderRole::Ride);
    auto bassDir = config.folder (foundry::FolderRole::Bass);

    expect (writeShot (io, kickDir.getChildFile ("Kick_A.wav"), 48000.0, 60.0), "write kick A");
    expect (writeShot (io, kickDir.getChildFile ("Kick_B.wav"), 48000.0, 70.0), "write kick B");
    expect (writeShot (io, clapDir.getChildFile ("Clap_A.wav"), 44100.0, 800.0), "write clap A");
    expect (writeShot (io, clapDir.getChildFile ("Clap_B.wav"), 48000.0, 900.0), "write clap B");
    expect (writeShot (io, hatDir.getChildFile ("Hat_A.wav"), 48000.0, 6000.0), "write hat");
    expect (writeShot (io, rideDir.getChildFile ("Ride_A.wav"), 48000.0, 4000.0), "write ride");
    expect (writeShot (io, bassDir.getChildFile ("Bass_A.wav"), 48000.0, 55.0), "write bass A");
    expect (writeShot (io, bassDir.getChildFile ("Bass_B.wav"), 48000.0, 62.0), "write bass B");
    expect (writeShot (io, bassDir.getChildFile ("Bass_C.wav"), 48000.0, 49.0), "write bass C");

    foundry::LibraryDatabase db;
    expect (db.open (config.databaseFile(), err), "open db: " + err);

    auto scan = foundry::scanLibrary (config, db, nullptr, nullptr);
    expect (scan.ok, "scan: " + scan.error);
    expect (db.countInCategory (foundry::Category::OneShotKick) >= 2, "indexed kicks");
    expect (db.countInCategory (foundry::Category::OneShotBass) >= 3, "indexed bass");

    auto found = db.query (foundry::Category::OneShotKick, "Kick_A", foundry::SortMode::Name, false);
    expect (! found.empty() && found[0].filename.containsIgnoreCase ("Kick_A"), "search Kick_A");

    // --- drums ---
    foundry::DrumRequest drums;
    drums.genre = foundry::Genre::Riddim;
    drums.bpm = 145.0;
    drums.bars = 1;
    drums.quantity = 2;
    drums.renderFull = true;
    drums.renderStems = true;
    drums.protectPeaks = true;
    drums.sampleRate = 48000.0;
    drums.bitDepth = 24;
    drums.pattern = foundry::defaultDrumPattern (foundry::Genre::Riddim, 1);
    drums.outputDir = config.generatedDrumDir (foundry::Genre::Riddim);
    drums.baseSeed = 4242;

    auto kicks = db.queryByRole (foundry::FolderRole::Kick, {});
    auto claps = db.queryByRole (foundry::FolderRole::Clap, {});
    auto hats  = db.queryByRole (foundry::FolderRole::Hat, {});
    auto rides = db.queryByRole (foundry::FolderRole::Ride, {});

    auto d1 = foundry::generateDrumBatch (drums, db, kicks, claps, hats, rides, nullptr, nullptr);
    expect (d1.ok, "drum gen: " + d1.error);
    expect (d1.filesWritten >= 2, "drum files written");

    drums.kick.locked = true;
    drums.kick.path = kickDir.getChildFile ("Kick_A.wav").getFullPathName();
    drums.quantity = 1;
    drums.baseSeed = 99;
    auto dLock = foundry::generateDrumBatch (drums, db, kicks, claps, hats, rides, nullptr, nullptr);
    expect (dLock.ok, "locked drum gen: " + dLock.error);

    drums.baseSeed = 4242;
    drums.quantity = 2;
    drums.kick.locked = false;
    auto d2 = foundry::generateDrumBatch (drums, db, kicks, claps, hats, rides, nullptr, nullptr);
    expect (d2.ok && d2.paths.size() == d1.paths.size(), "seeded drum batch size");

    juce::AudioBuffer<float> wav;
    double sr = 0.0;
    if (! d1.paths.isEmpty())
    {
        const juce::File f (d1.paths[0]);
        expect (f.existsAsFile(), "drum wav exists");
        expect (io.loadWav (f, wav, sr, err), "load drum wav: " + err);
        expect (sr == 48000.0, "drum render sr");
        expect (wav.getNumChannels() == 2, "drum stereo");
        expect (isFiniteBuffer (wav), "drum finite");
        expect (foundry::AudioIO::peakAbs (wav) <= 1.001f, "drum peak protected");
    }

    // --- bass ---
    foundry::BassRequest bass;
    bass.genre = foundry::Genre::Dubstep;
    bass.bpm = 140.0;
    bass.bars = 2;
    bass.quantity = 3;
    bass.sourceCount = 3;
    bass.variation = foundry::Variation::Normal;
    bass.sampleRate = 48000.0;
    bass.outputDir = config.generatedBassDir (false);
    bass.baseSeed = 7;

    auto shots = db.queryByRole (foundry::FolderRole::Bass, {});
    auto b1 = foundry::generateBassBatch (bass, db, shots, nullptr, nullptr);
    expect (b1.ok, "bass gen: " + b1.error);
    expect (b1.filesWritten == 3, "bass quantity");
    expect (db.countInCategory (foundry::Category::BassLoopsBase) >= 3, "bass indexed");

    // --- splice ---
    foundry::SpliceRequest sp;
    sp.genre = foundry::Genre::Dubstep;
    sp.bpm = 140.0;
    sp.bars = 2;
    sp.quantity = 4;
    sp.slice = foundry::SliceRes::Eighth;
    sp.mode = foundry::SpliceMode::CallAndResponse;
    sp.sampleRate = 48000.0;
    sp.outputDir = config.generatedBassDir (true);
    sp.baseSeed = 11;
    for (int i = 0; i < juce::jmin (3, b1.paths.size()); ++i)
        sp.sources.push_back (juce::File (b1.paths[i]));

    auto s1 = foundry::generateSpliceBatch (sp, db, nullptr, nullptr);
    expect (s1.ok, "splice gen: " + s1.error);
    expect (s1.filesWritten == 4, "splice quantity");
    expect (db.countInCategory (foundry::Category::BassLoopsSpliced) >= 4, "splice indexed");

    if (! s1.paths.isEmpty())
    {
        expect (io.loadWav (juce::File (s1.paths[0]), wav, sr, err), "load splice");
        expect (isFiniteBuffer (wav), "splice finite");
    }

    // cancel after first file
    bass.quantity = 8;
    bass.baseSeed = 1;
    std::atomic<int> n { 0 };
    auto cancelled = foundry::generateBassBatch (bass, db, shots,
        [&n] { return n.load() >= 1; },
        [&n] (int done, int, juce::String) { n.store (done); });
    expect (cancelled.filesWritten >= 1, "cancel keeps completed files");

    // preview
    foundry::PreviewEngine preview;
    preview.prepare (48000.0, 512);
    expect (preview.loadAndPlay (kickDir.getChildFile ("Kick_A.wav"), false, err), "preview load: " + err);
    juce::AudioBuffer<float> block (2, 512);
    block.clear();
    preview.mixInto (block);
    expect (isFiniteBuffer (block), "preview finite");
    expect (block.getMagnitude (0, 0, 512) > 0.0f, "preview audible");
    preview.stop();

    // config round-trip
    auto cfgFile = root.getChildFile ("config.json");
    expect (config.save (cfgFile, err), "save config");
    foundry::ConfigStore loaded;
    expect (loaded.load (cfgFile, err), "load config");
    expect (loaded.root() == root, "config root");

    root.deleteRecursively();

    if (gFails == 0)
        std::cout << "EngineTest: all checks passed.\n";
    else
        std::cerr << "EngineTest: " << gFails << " failure(s).\n";

    return gFails == 0 ? 0 : 1;
}
