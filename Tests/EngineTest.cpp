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
#include "../Source/Core/BrowserModel.h"
#include "../Source/Core/RiffEngine.h"
#include "../Source/Core/ThumbnailCache.h"
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

    /** Writes a test signal: dc != 0 gives a constant level, otherwise a ramp
        0..1 over the file (so a value read back reveals where in the file it
        came from). */
    juce::File writeSignal (foundry::AudioIO& io, const juce::File& file, double sr, double seconds, float dc)
    {
        const int n = juce::roundToInt (sr * seconds);
        juce::AudioBuffer<float> b (1, n);
        for (int i = 0; i < n; ++i)
            b.setSample (0, i, dc != 0.0f ? dc : (float) i / (float) n);
        juce::String err;
        io.writeWav (file, b, sr, 24, err);
        return file;
    }

    float rms (const juce::AudioBuffer<float>& b, int start, int len)
    {
        start = juce::jlimit (0, b.getNumSamples(), start);
        len = juce::jlimit (0, b.getNumSamples() - start, len);
        return len > 0 ? b.getRMSLevel (0, start, len) : 0.0f;
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

    // --- patterns: every generated loop remembers its groove and its sounds ---
    {
        expect (! d1.paths.isEmpty(), "drum batch produced files");
        if (! d1.paths.isEmpty())
        {
            const juce::File loop (d1.paths[0]);
            foundry::PatternFile pf;
            expect (foundry::PatternFile::load (foundry::patternSidecarFor (loop), pf), "drum loop has a pattern saved next to it");
            expect (pf.kind == foundry::PatternKind::Drums, "drum sidecar is a drum pattern");
            bool same = pf.grid.bars == drums.pattern.bars;
            for (int r = 0; r < 4; ++r)
                for (int st = 0; st < drums.pattern.numSteps(); ++st)
                    same = same && pf.grid.getStep (r, st) == drums.pattern.getStep (r, st);
            expect (same, "the saved grid is the pattern the loop was made from");
            expect (juce::File (pf.samples[0]).existsAsFile() && juce::File (pf.samples[1]).existsAsFile(),
                    "the saved pattern names the kick and clap it used");
        }

        if (! b1.paths.isEmpty())
        {
            foundry::PatternFile pf;
            expect (foundry::PatternFile::load (foundry::patternSidecarFor (juce::File (b1.paths[0])), pf),
                    "bass loop has a pattern saved next to it");
            expect (pf.kind == foundry::PatternKind::Bass && pf.hasSamples(), "bass sidecar holds its sources");
        }

        // Bass pattern mode with a locked source.
        foundry::BassRequest pb = bass;
        pb.quantity = 1;
        pb.bars = 1;
        pb.usePattern = true;
        pb.pattern.setBars (1);
        pb.pattern.clear();
        pb.pattern.setStep (0, 0, true);
        pb.pattern.setStep (1, 8, true);
        pb.pattern.setStep (0, 12, true);
        const auto lockedA = bassDir.getChildFile ("Bass_B.wav").getFullPathName();
        pb.locks[0].locked = true;
        pb.locks[0].path = lockedA;

        auto pr = foundry::generateBassBatch (pb, db, shots, nullptr, nullptr);
        expect (pr.ok && pr.filesWritten == 1, "bass pattern mode renders: " + pr.error);
        if (pr.ok && ! pr.paths.isEmpty())
        {
            foundry::PatternFile out;
            expect (foundry::PatternFile::load (foundry::patternSidecarFor (juce::File (pr.paths[0])), out), "pattern-mode sidecar");
            expect (out.grid.getStep (0, 0) && out.grid.getStep (1, 8) && out.grid.getStep (0, 12) && ! out.grid.getStep (0, 4),
                    "bass pattern mode plays exactly the drawn grid");
            expect (out.samples[0] == lockedA, "a locked bass source is used as-is");

            juce::AudioBuffer<float> pw;
            double psr = 0.0;
            expect (io.loadWav (juce::File (pr.paths[0]), pw, psr, err), "load pattern-mode bass");
            const int stepN = foundry::AudioIO::beatsToSamples (0.25, pb.bpm, 48000.0);
            expect (pw.getRMSLevel (0, 4 * stepN + stepN / 2, stepN / 2) < pw.getRMSLevel (0, 0, stepN / 2),
                    "nothing new starts on a step left empty");
        }

        foundry::BassRequest empty = pb;
        empty.pattern.clear();
        auto er = foundry::generateBassBatch (empty, db, shots, nullptr, nullptr);
        expect (! er.ok && er.error.containsIgnoreCase ("empty"), "an empty bass pattern is refused with a clear message");

        // Pattern files round-trip.
        foundry::PatternFile a;
        a.kind = foundry::PatternKind::Bass;
        a.grid.setBars (2);
        a.grid.clear();
        a.grid.setStep (2, 17, true);
        a.samples[2] = "/x/y.wav";
        a.bpm = 150.0;
        foundry::PatternFile b;
        expect (foundry::PatternFile::fromJson (a.toJson(), b), "pattern JSON parses");
        expect (b.kind == a.kind && b.grid.bars == 2 && b.grid.getStep (2, 17) && ! b.grid.getStep (2, 16)
                    && b.samples[2] == "/x/y.wav" && std::abs (b.bpm - 150.0) < 0.01,
                "pattern JSON round-trips");
    }

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
    preview.render (block);
    expect (isFiniteBuffer (block), "preview finite");
    expect (block.getMagnitude (0, 0, 512) > 0.0f, "preview audible");
    expect (preview.hasClip(), "preview reports a loaded clip");
    expect (preview.clipLengthSamples() > 0, "preview knows the clip length");
    preview.stop();

    // config round-trip
    auto cfgFile = root.getChildFile ("config.json");
    expect (config.save (cfgFile, err), "save config");
    foundry::ConfigStore loaded;
    expect (loaded.load (cfgFile, err), "load config");
    expect (loaded.root() == root, "config root");

    // ======================================================================
    // Riff Maker
    // ======================================================================
    {
        constexpr double sr = 48000.0;
        constexpr double bpm = 120.0;            // a 16th = 0.125 s = 6000 samples
        constexpr int step = 6000;

        auto sigDir = root.getChildFile ("Signals");
        sigDir.createDirectory();
        const auto ramp  = writeSignal (io, sigDir.getChildFile ("ramp.wav"), sr, 4.0, 0.0f);
        const auto plus  = writeSignal (io, sigDir.getChildFile ("plus.wav"), sr, 4.0, 0.5f);
        const auto minus = writeSignal (io, sigDir.getChildFile ("minus.wav"), sr, 4.0, -0.25f);

        foundry::SampleCache cache;

        // JSON round trip
        {
            foundry::RiffSketch a;
            a.gridSteps = 2;
            a.slots[0].path = ramp.getFullPathName();
            a.slots[0].offsetSeconds = 0.125;
            a.slots[0].lengthSteps = 3;
            a.slots[0].gain = 0.5f;
            a.slots[3].locked = true;
            a.lastSeed = 1234567890123LL;

            foundry::RiffSketch b;
            expect (foundry::RiffSketch::fromJson (a.toJson(), b), "riff JSON parses");
            expect (a == b, "riff JSON round-trips every slot field");
            expect (b.lastSeed == a.lastSeed, "riff JSON keeps the dice seed");
            expect (! foundry::RiffSketch::fromJson ("{\"foo\":1}", b), "non-riff JSON is rejected");

            foundry::RiffSketch held;
            held.slots[2].path = "x.wav";
            held.slots[2].lengthSteps = 9;          // 1/4 grid: rings into slots 4 and 5
            expect (held.heldBy (3) == 2 && held.heldBy (4) == 2 && held.heldBy (5) == -1,
                    "a slice longer than the grid holds the empty slots it rings through");
            expect (held.heldBy (1) == -1 && held.heldBy (2) == -1, "slots before it are silent; a full slot isn't 'held'");
            expect (foundry::stepsName (4) == "1/4" && foundry::stepsName (3) == "3/16" && foundry::stepsName (16) == "1 bar",
                    "length names");
        }

        // Grid timing and slice length: eight 1/8 slices on a 1/4 grid.
        {
            foundry::RiffSketch sk;
            for (auto& sl : sk.slots)
            {
                sl.path = plus.getFullPathName();
                sl.lengthSteps = 2;
            }

            auto out = foundry::RiffEngine::renderOffline (sk, cache, bpm, sr, 1);
            expect (out.getNumSamples() == 8 * 4 * step, "one pass of a 2-bar riff is 2 bars long");
            expect (isFiniteBuffer (out), "riff render is finite");

            bool timingOk = true;
            for (int k = 0; k < 8; ++k)
            {
                const int start = k * 4 * step;
                const float on  = rms (out, start + 400, 2 * step - 800);       // inside the slice
                const float off = rms (out, start + 2 * step + 300, 2 * step - 600);  // the gap after it
                if (on < 0.4f || off > 0.001f)
                    timingOk = false;
            }
            expect (timingOk, "each slot sounds on its grid position for exactly its length");
        }

        // Start offset (slip-edit): the slot plays from that point of the file.
        {
            foundry::RiffSketch sk;
            sk.slots[0].path = ramp.getFullPathName();
            sk.slots[0].offsetSeconds = 1.0;              // ramp value 0.25 at 1 s of 4 s

            auto out = foundry::RiffEngine::renderOffline (sk, cache, bpm, sr, 1);
            const float v = out.getSample (0, 1000);     // past the start fade
            const float expected = (float) (sr * 1.0 + 1000) / (float) (sr * 4.0);
            expect (std::abs (v - expected) < 0.01f,
                    "slot starts at its offset in the sample (got " + juce::String (v, 4)
                        + ", expected " + juce::String (expected, 4) + ")");
        }

        // A long slice rings on through a rest, but is cut by the next sample.
        {
            foundry::RiffSketch sk;
            sk.slots[0].path = plus.getFullPathName();
            sk.slots[0].lengthSteps = 8;                  // half a bar on a 1/4 grid
            sk.slots[2].path = plus.getFullPathName();
            sk.slots[2].lengthSteps = 8;
            sk.slots[3].path = minus.getFullPathName();   // starts while slot 2 would still ring

            auto out = foundry::RiffEngine::renderOffline (sk, cache, bpm, sr, 1);
            expect (rms (out, 4 * step + 500, 3 * step) > 0.4f, "slice rings on through an empty slot");
            const float afterChoke = out.getSample (0, 12 * step + 1200);
            expect (std::abs (afterChoke + 0.25f) < 0.01f,
                    "the next sample cuts the previous one off (got " + juce::String (afterChoke, 4) + ")");
        }

        // A seamless loop folds the ring-out of the last slot into the start.
        {
            foundry::RiffSketch sk;
            sk.slots[7].path = plus.getFullPathName();
            sk.slots[7].lengthSteps = 8;                  // rings past the end of the riff

            auto loop = foundry::RiffEngine::renderLoop (sk, cache, bpm, sr);
            expect (loop.getNumSamples() == 8 * 4 * step, "loop render is exactly one riff long");
            expect (rms (loop, 1000, 2 * step) > 0.4f, "the tail of the last slot wraps to the start of the loop");
        }

        // Missing files become rests and are reported.
        {
            foundry::RiffSketch sk;
            sk.slots[0].path = sigDir.getChildFile ("gone.wav").getFullPathName();
            juce::StringArray problems;
            auto out = foundry::RiffEngine::renderOffline (sk, cache, bpm, sr, 1, &problems);
            expect (problems.contains ("gone.wav"), "missing sample is reported");
            expect (out.getMagnitude (0, 0, out.getNumSamples()) == 0.0f, "missing sample plays as a rest");
        }

        // Live engine locked to a host timeline, including a jump.
        {
            foundry::RiffSketch sk;
            sk.slots[1].path = plus.getFullPathName();    // beat 2
            foundry::RiffEngine engine;
            engine.publish (foundry::buildRiffSnapshot (sk, cache, sr));
            engine.prepare (sr);
            engine.setVolume (1.0f);

            foundry::RiffEngine::HostInfo host;
            host.playing = true;
            host.ppqValid = true;
            host.bpm = bpm;

            juce::AudioBuffer<float> blockBuf (2, 512);
            auto runAt = [&] (double ppq)
            {
                host.ppq = ppq;
                blockBuf.clear();
                engine.process (blockBuf, host);
                return blockBuf.getMagnitude (0, 0, 512);
            };

            expect (runAt (0.0) == 0.0f, "beat 1 is a rest");
            expect (runAt (1.5) > 0.3f, "jumping into the middle of slot 2 picks it up straight away");
            host.playing = false;
            runAt (1.6);
            runAt (1.6);
            expect (runAt (1.6) == 0.0f, "stopping the host transport silences the riff");
        }

        // Browser: curated folders, real sub-folders, search, favourites.
        {
            auto kicks = config.folder (foundry::FolderRole::Kick);
            kicks.getChildFile ("Hard").createDirectory();
            expect (writeShot (io, kicks.getChildFile ("Hard").getChildFile ("Kick_Hard.wav"), 48000.0, 50.0), "write nested kick");
            auto rescan = foundry::scanLibrary (config, db, nullptr, nullptr);
            expect (rescan.ok, "rescan: " + rescan.error);

            foundry::BrowserModel model (config, db);
            bool truncated = false;

            auto top = model.list (foundry::BrowserModel::root(), {}, true, truncated);
            auto findEntry = [] (const std::vector<foundry::BrowserEntry>& v, const juce::String& name) -> const foundry::BrowserEntry*
            {
                for (auto& e : v) if (e.name == name) return &e;
                return nullptr;
            };

            const auto* drumsEntry = findEntry (top, "Drums");
            expect (drumsEntry != nullptr && drumsEntry->isFolder() && drumsEntry->count >= 7,
                    "root shows a Drums folder counting every drum sample");
            expect (findEntry (top, "Favourites") != nullptr && findEntry (top, "My Riffs") != nullptr
                        && findEntry (top, "Bass") != nullptr && findEntry (top, "Batch Generated") != nullptr,
                    "root shows Favourites, Bass, My Riffs and Batch Generated");

            auto drumsList = model.list (drumsEntry->target, {}, true, truncated);
            const auto* kicksEntry = findEntry (drumsList, "Kicks");
            expect (kicksEntry != nullptr && kicksEntry->count == 3, "Drums > Kicks counts nested files too");

            auto kicksList = model.list (kicksEntry->target, {}, true, truncated);
            expect (! kicksList.empty() && kicksList.front().isFolder() && kicksList.front().name == "Hard",
                    "a real sub-folder is listed first");
            int files = 0;
            for (auto& e : kicksList) if (! e.isFolder()) ++files;
            expect (files == 2, "only files directly in the folder are listed beside it");

            auto crumbs = model.pathTo (kicksList.front().target);
            expect (crumbs.size() == 4 && crumbs[1].title == "Drums" && crumbs[2].title == "Kicks" && crumbs[3].title == "Hard",
                    "breadcrumb: Library > Drums > Kicks > Hard");

            auto hits = model.list (drumsEntry->target, "hard", true, truncated);
            expect (hits.size() == 1 && hits[0].name == "Kick_Hard", "search inside a folder finds nested files");

            auto pool = model.filesUnder (drumsEntry->target);
            expect (pool.size() >= 7, "the dice can draw from everything under a folder");

            expect (db.setFavorite (kicksList.back().item.id, true, err), "favourite a kick");
            auto favs = model.list (top.front().target, {}, true, truncated);
            expect (favs.size() == 1, "Favourites lists favourited samples");

            // Thumbnails arrive from the background thread.
            foundry::ThumbnailCache thumbs (db);
            const auto path = kicksList.back().item.path;
            thumbs.request (path);
            for (int i = 0; i < 200 && thumbs.request (path) == nullptr; ++i)
                juce::Thread::sleep (10);
            auto t = thumbs.request (path);
            expect (t != nullptr && ! t->failed && (int) t->peaks.size() == foundry::ThumbnailCache::kBins,
                    "thumbnail computed in the background");
            expect (t != nullptr && t->seconds > 0.1, "thumbnail knows the duration");

            // Flat view (folders hidden) lists every file below a folder.
            auto flat = model.list (drumsEntry->target, {}, false, truncated);
            bool allFiles = ! flat.empty();
            for (auto& e : flat) allFiles = allFiles && e.isFile();
            expect (allFiles && flat.size() >= 7, "flat view lists every drum sample with no folders");

            // Patterns folder lists pattern files from disk.
            foundry::PatternFile libPattern;
            libPattern.grid = foundry::defaultDrumPattern (foundry::Genre::Riddim, 1);
            expect (libPattern.save (config.patternDir (false).getChildFile (juce::String ("Half Time") + foundry::kPatternExtension)),
                    "save a pattern to the library");
            const auto* patTop = findEntry (top, "Patterns");
            expect (patTop != nullptr, "root shows a Patterns folder");
            if (patTop != nullptr)
            {
                auto patRoot = model.list (patTop->target, {}, true, truncated);
                const auto* drumsPat = findEntry (patRoot, "Drums");
                expect (drumsPat != nullptr && drumsPat->count == 1, "Patterns > Drums counts its patterns");
                if (drumsPat != nullptr)
                {
                    auto pats = model.list (drumsPat->target, {}, true, truncated);
                    expect (pats.size() == 1 && pats[0].isPattern() && pats[0].name == "Half Time",
                            "pattern files appear as pattern tiles");
                }
            }

            bool hasAdd = false;
            for (auto& e : top) hasAdd = hasAdd || e.type == foundry::BrowserEntry::Type::AddFolder;
            expect (hasAdd, "root offers an Add folder tile");

            // Your own folders: added to the browser and indexed, keeping categories.
            auto mine = root.getChildFile ("MyStuff");
            mine.createDirectory();
            expect (writeShot (io, mine.getChildFile ("Weird_Hit.wav"), 48000.0, 300.0), "write custom sample");
            config.get().userFolders.add (mine.getFullPathName());
            auto scan3 = foundry::scanLibrary (config, db, nullptr, nullptr);
            expect (scan3.ok, "scan with a custom folder: " + scan3.error);
            auto top2 = model.list (foundry::BrowserModel::root(), {}, true, truncated);
            const auto* mineEntry = findEntry (top2, "MyStuff");
            expect (mineEntry != nullptr && mineEntry->custom && mineEntry->count == 1, "a custom folder shows at the top level");
            expect (db.findByPath (mine.getChildFile ("Weird_Hit.wav").getFullPathName()).has_value(), "custom folder is indexed");

            std::vector<juce::uint8> cached;
            double secs = 0.0;
            const juce::File pf (path);
            expect (db.loadThumb (path, pf.getLastModificationTime().toMilliseconds(), pf.getSize(), cached, secs),
                    "thumbnail persisted in the library database");
        }
    }

    root.deleteRecursively();

    if (gFails == 0)
        std::cout << "EngineTest: all checks passed.\n";
    else
        std::cerr << "EngineTest: " << gFails << " failure(s).\n";

    return gFails == 0 ? 0 : 1;
}
