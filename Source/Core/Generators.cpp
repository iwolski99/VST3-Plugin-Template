#include "Generators.h"
#include <numeric>

namespace foundry
{

namespace
{
    juce::int64 nowMs()
    {
        return juce::Time::currentTimeMillis();
    }

    juce::String padded4 (int n)
    {
        return juce::String (n).paddedLeft ('0', 4);
    }

    bool pickPath (const SampleLock& lock,
                   const std::vector<LibraryItem>& pool,
                   juce::Random& rng,
                   juce::String& out,
                   bool required,
                   const juce::String& label,
                   juce::String& error)
    {
        if (lock.locked && lock.path.isNotEmpty() && juce::File (lock.path).existsAsFile())
        {
            out = lock.path;
            return true;
        }

        std::vector<const LibraryItem*> ok;
        ok.reserve (pool.size());
        for (const auto& it : pool)
            if (it.existsOnDisk && juce::File (it.path).existsAsFile())
                ok.push_back (&it);

        if (ok.empty())
        {
            if (required)
            {
                error = "No samples were found in the configured " + label + " folder.";
                return false;
            }
            out.clear();
            return true;
        }

        out = ok[(size_t) rng.nextInt ((int) ok.size())]->path;
        return true;
    }

    bool loadStereoAt (AudioIO& io, const juce::File& file, double targetSr,
                       juce::AudioBuffer<float>& dest, juce::String& error)
    {
        juce::AudioBuffer<float> raw;
        double sr = 0.0;
        if (! io.loadWav (file, raw, sr, error))
            return false;

        dest = AudioIO::toStereo (AudioIO::resample (raw, sr, targetSr));
        return dest.getNumSamples() > 0;
    }

    bool insertGenerated (LibraryDatabase& db, const juce::File& file, Category cat,
                          Genre genre, double bpm, int bars, const juce::String& genType,
                          juce::int64 seed, const juce::String& patternId,
                          const juce::String& parents, int depth, juce::String& error)
    {
        LibraryItem it;
        it.filename = file.getFileName();
        it.path = file.getFullPathName();
        it.fileType = "loop";
        it.category = cat;
        it.bpm = bpm;
        it.bars = bars;
        it.genre = genreName (genre);
        it.dateCreated = nowMs();
        it.dateIndexed = it.dateCreated;
        it.generationType = genType;
        it.generationSeed = seed;
        it.parentSourceIds = parents;
        it.patternId = patternId;
        it.engine = genType;
        it.generationDepth = depth;
        it.existsOnDisk = true;
        return db.upsert (it, error);
    }

    struct BassHit
    {
        int step = 0;       // 16th-note index in the loop
        int source = 0;     // which one-shot
    };

    std::vector<BassHit> makeBassPattern (Genre genre, Variation variation,
                                          int bars, int numSources, juce::Random& rng,
                                          juce::String& patternId)
    {
        const int steps = bars * kStepsPerBar;
        std::vector<BassHit> hits;
        const int srcN = juce::jmax (1, numSources);

        auto add = [&] (int step, int src)
        {
            if (step >= 0 && step < steps)
                hits.push_back ({ step, juce::jlimit (0, srcN - 1, src) });
        };

        const int roll = rng.nextInt (genre == Genre::Dubstep ? 6 : 5);

        if (variation == Variation::Conservative)
        {
            patternId = "quarters";
            for (int s = 0; s < steps; s += 4)
                add (s, (s / 4) % srcN);
            return hits;
        }

        switch (roll)
        {
            case 0:
                patternId = "eighths";
                for (int s = 0; s < steps; s += 2)
                    add (s, (s / 2) % srcN);
                break;

            case 1:
                patternId = "call_response";
                for (int bar = 0; bar < bars; ++bar)
                {
                    const int base = bar * kStepsPerBar;
                    const int srcA = bar % srcN;
                    const int srcB = (bar + 1) % srcN;
                    add (base + 0, srcA);
                    add (base + 2, srcA);
                    add (base + 8, srcB);
                    add (base + 10, srcB);
                    if (variation == Variation::Wild)
                    {
                        add (base + 4, srcA);
                        add (base + 12, srcB);
                    }
                }
                break;

            case 2:
                patternId = "chops_rests";
                for (int bar = 0; bar < bars; ++bar)
                {
                    const int base = bar * kStepsPerBar;
                    add (base + 0, 0);
                    add (base + 3, 0);
                    add (base + 6, srcN > 1 ? 1 : 0);
                    add (base + 8, srcN > 1 ? 1 : 0);
                    add (base + 14, srcN > 2 ? 2 : 0);
                }
                break;

            case 3:
                patternId = "syncopated";
                for (int bar = 0; bar < bars; ++bar)
                {
                    const int base = bar * kStepsPerBar;
                    add (base + 0, 0);
                    add (base + 3, srcN > 1 ? 1 : 0);
                    add (base + 6, 0);
                    add (base + 10, srcN > 1 ? 1 : 0);
                    add (base + 13, srcN > 2 ? 2 : 0);
                }
                break;

            case 4:
                patternId = "straight_phrase";
                for (int bar = 0; bar < bars; ++bar)
                {
                    const int base = bar * kStepsPerBar;
                    add (base + 0, 0);
                    add (base + 4, 0);
                    add (base + 8, srcN > 1 ? 1 : 0);
                    add (base + 12, srcN > 1 ? 1 : 0);
                    if (variation != Variation::Conservative)
                        add (base + 14, srcN > 2 ? 2 : 0);
                }
                break;

            default:
                patternId = "triplet_burst";
                for (int bar = 0; bar < bars; ++bar)
                {
                    const int base = bar * kStepsPerBar;
                    add (base + 0, 0);
                    add (base + 8, srcN > 1 ? 1 : 0);
                    // Approximate 8th-triplet burst in the last beat of the bar.
                    add (base + 12, srcN > 2 ? 2 : 0);
                    add (base + 13, srcN > 2 ? 2 : 0);
                    add (base + 14, srcN > 1 ? 1 : 0);
                }
                break;
        }

        if (variation == Variation::Wild && ! hits.empty())
        {
            // Drop or nudge a couple of hits so wild batches don't clone each other.
            const int drops = rng.nextInt (3);
            for (int i = 0; i < drops && hits.size() > 3; ++i)
                hits.erase (hits.begin() + rng.nextInt ((int) hits.size()));

            for (auto& h : hits)
                if (rng.nextFloat() < 0.18f)
                    h.source = rng.nextInt (srcN);
        }

        return hits;
    }

    std::vector<int> spliceSequence (SpliceMode mode, int numSlices, int numSources,
                                     Variation variation, juce::Random& rng)
    {
        std::vector<int> seq ((size_t) juce::jmax (1, numSlices), 0);
        const int nsrc = juce::jmax (1, numSources);

        auto wrap = [nsrc] (int v) { return ((v % nsrc) + nsrc) % nsrc; };

        switch (mode)
        {
            case SpliceMode::Alternating:
                for (int i = 0; i < numSlices; ++i)
                    seq[(size_t) i] = wrap (i);
                break;

            case SpliceMode::BlockSwap:
            {
                const int block = juce::jmax (1, numSlices / (nsrc * 2));
                int src = 0, left = block;
                for (int i = 0; i < numSlices; ++i)
                {
                    seq[(size_t) i] = wrap (src);
                    if (--left <= 0)
                    {
                        ++src;
                        left = block;
                    }
                }
                break;
            }

            case SpliceMode::CallAndResponse:
                for (int i = 0; i < numSlices; ++i)
                {
                    const int in4 = i % 4;
                    if (in4 < 2)
                        seq[(size_t) i] = 0;
                    else if (in4 == 2)
                        seq[(size_t) i] = wrap (1);
                    else
                        seq[(size_t) i] = wrap (nsrc > 2 ? 2 : 0);
                }
                break;

            case SpliceMode::Syncopated:
                for (int i = 0; i < numSlices; ++i)
                    seq[(size_t) i] = wrap ((i % 3 == 0) ? 0 : (i % 2 == 0 ? 1 : 2));
                break;

            case SpliceMode::Burst:
                for (int i = 0; i < numSlices; ++i)
                {
                    const int inBar = i % 8;
                    seq[(size_t) i] = (inBar >= 6) ? wrap (i) : 0;
                }
                break;

            case SpliceMode::Weighted:
            default:
                for (int i = 0; i < numSlices; ++i)
                {
                    const float r = rng.nextFloat();
                    if (r < 0.50f)
                        seq[(size_t) i] = 0;
                    else if (r < 0.80f)
                        seq[(size_t) i] = wrap (1);
                    else
                        seq[(size_t) i] = wrap (2);
                }
                break;
        }

        if (variation == Variation::Wild)
        {
            for (int i = 0; i < numSlices; ++i)
                if (rng.nextFloat() < 0.22f)
                    seq[(size_t) i] = rng.nextInt (nsrc);
        }
        else if (variation == Variation::Conservative)
        {
            // Hold values a little longer.
            for (int i = 1; i < numSlices; i += 2)
                seq[(size_t) i] = seq[(size_t) i - 1];
        }

        return seq;
    }
}

GenerateOutcome generateDrumBatch (const DrumRequest& req,
                                   LibraryDatabase& db,
                                   const std::vector<LibraryItem>& kicks,
                                   const std::vector<LibraryItem>& claps,
                                   const std::vector<LibraryItem>& hats,
                                   const std::vector<LibraryItem>& rides,
                                   CancelFn cancel, ProgressFn progress)
{
    GenerateOutcome out;
    AudioIO io;

    if (! req.renderFull && ! req.renderStems)
    {
        out.error = "Enable Full Loop, Stems, or both before generating.";
        return out;
    }

    if (req.quantity < 1)
    {
        out.error = "Quantity must be at least 1.";
        return out;
    }

    if (! req.outputDir.isDirectory() && ! req.outputDir.createDirectory())
    {
        out.error = "Invalid render destination: " + req.outputDir.getFullPathName();
        return out;
    }

    const int fade = AudioIO::fadeSamplesFor (req.sampleRate);
    const int loopN = AudioIO::loopLengthSamples (req.pattern.bars, req.bpm, req.sampleRate);
    const int steps = req.pattern.numSteps();
    const double stepBeats = 4.0 / (double) kStepsPerBar;

    const juce::String prefix = genreName (req.genre) + "_Drum_";
    int seq = db.nextSequence (prefix);

    const int total = req.quantity;
    if (progress)
        progress (0, total, "Generating drum loops");

    for (int n = 0; n < total; ++n)
    {
        if (cancel && cancel())
        {
            out.ok = out.filesWritten > 0;
            if (! out.ok)
                out.error = "Generation cancelled.";
            return out;
        }

        const juce::int64 seed = req.baseSeed != 0 ? (req.baseSeed + n) : (nowMs() + n * 9973);
        juce::Random rng (seed);

        juce::String kickPath, clapPath, hatPath, ridePath;
        if (! pickPath (req.kick, kicks, rng, kickPath, true,  "Kick", out.error))
            return out;
        if (! pickPath (req.clap, claps, rng, clapPath, true,  "Clap", out.error))
            return out;
        if (! pickPath (req.hat,  hats,  rng, hatPath,  false, "Closed Hat", out.error))
            return out;
        if (! pickPath (req.ride, rides, rng, ridePath, false, "Ride", out.error))
            return out;

        juce::AudioBuffer<float> samples[4];
        const juce::String paths[4] = { kickPath, clapPath, hatPath, ridePath };
        const char* labels[4] = { "KICK", "CLAP", "HAT", "RIDE" };

        for (int r = 0; r < 4; ++r)
        {
            if (paths[r].isEmpty())
                continue;
            if (! loadStereoAt (io, juce::File (paths[r]), req.sampleRate, samples[r], out.error))
                return out;
        }

        juce::AudioBuffer<float> stems[4];
        juce::AudioBuffer<float> full (2, loopN);
        full.clear();

        for (int r = 0; r < 4; ++r)
        {
            stems[r].setSize (2, loopN);
            stems[r].clear();

            if (samples[r].getNumSamples() <= 0)
                continue;

            for (int s = 0; s < steps; ++s)
            {
                if (! req.pattern.getStep (r, s))
                    continue;

                const int pos = AudioIO::beatsToSamples ((double) s * stepBeats, req.bpm, req.sampleRate);
                AudioIO::mixAt (stems[r], samples[r], pos, 1.0f,
                                AudioIO::kOneShotFadeInSamples, 0);
            }

            AudioIO::fadeLoopEnds (stems[r], fade);
            if (req.protectPeaks)
                AudioIO::protectPeaks (stems[r]);

            for (int ch = 0; ch < 2; ++ch)
                full.addFrom (ch, 0, stems[r], ch, 0, loopN);
        }

        AudioIO::fadeLoopEnds (full, fade);
        if (req.protectPeaks)
            AudioIO::protectPeaks (full);

        const auto parents = juce::StringArray { kickPath, clapPath, hatPath, ridePath }.joinIntoString (";");
        juce::String err;

        // Every loop remembers its groove and its sounds, so it can be
        // reopened in the sequencer and regenerated with a different pattern.
        PatternFile sidecar;
        sidecar.kind = PatternKind::Drums;
        sidecar.grid = req.pattern;
        sidecar.samples = { kickPath, clapPath, hatPath, ridePath };
        sidecar.bpm = req.bpm;
        sidecar.genre = req.genre;

        if (req.renderFull)
        {
            const auto stem = prefix + padded4 (seq);
            auto file = AudioIO::uniqueWavPath (req.outputDir, stem + "_FULL");
            if (! io.writeWav (file, full, req.sampleRate, req.bitDepth, out.error))
                return out;
            if (! insertGenerated (db, file, drumLoopCategory (req.genre), req.genre, req.bpm,
                                   req.pattern.bars, "drum", seed, "drum_grid", parents, 0, err))
            {
                out.error = err;
                return out;
            }
            sidecar.save (patternSidecarFor (file));
            out.paths.add (file.getFullPathName());
            ++out.filesWritten;
        }

        if (req.renderStems)
        {
            for (int r = 0; r < 4; ++r)
            {
                if (samples[r].getNumSamples() <= 0)
                    continue;

                const auto stem = prefix + padded4 (seq) + "_" + labels[r];
                auto file = AudioIO::uniqueWavPath (req.outputDir, stem);
                if (! io.writeWav (file, stems[r], req.sampleRate, req.bitDepth, out.error))
                    return out;
                if (! insertGenerated (db, file, drumLoopCategory (req.genre), req.genre, req.bpm,
                                       req.pattern.bars, "drum_stem", seed, labels[r], paths[r], 0, err))
                {
                    out.error = err;
                    return out;
                }
                sidecar.save (patternSidecarFor (file));
                out.paths.add (file.getFullPathName());
                ++out.filesWritten;
            }
        }

        ++seq;
        if (progress)
            progress (n + 1, total, "Generating drum loops");
    }

    out.ok = true;
    return out;
}

GenerateOutcome generateBassBatch (const BassRequest& req,
                                   LibraryDatabase& db,
                                   const std::vector<LibraryItem>& bassShots,
                                   CancelFn cancel, ProgressFn progress)
{
    GenerateOutcome out;
    AudioIO io;

    std::vector<const LibraryItem*> pool;
    for (const auto& it : bassShots)
        if (it.existsOnDisk && juce::File (it.path).existsAsFile())
            pool.push_back (&it);

    if (pool.empty())
    {
        out.error = "No samples were found in the configured Bass One-Shot folder.";
        return out;
    }

    if (! req.outputDir.isDirectory() && ! req.outputDir.createDirectory())
    {
        out.error = "Invalid render destination: " + req.outputDir.getFullPathName();
        return out;
    }

    // In pattern mode the grid decides how many sources are needed.
    int patternRows = 0;
    if (req.usePattern)
        for (int r = 0; r < 4; ++r)
            for (int st = 0; st < req.pattern.numSteps(); ++st)
                if (req.pattern.getStep (r, st))
                    patternRows = r + 1;

    if (req.usePattern && patternRows == 0)
    {
        out.error = "The bass pattern is empty - click some steps in the grid first.";
        return out;
    }

    const int want = req.usePattern ? patternRows
                                    : juce::jlimit (1, (int) pool.size(), juce::jlimit (2, 4, req.sourceCount));
    const int fade = AudioIO::fadeSamplesFor (req.sampleRate);
    const int loopN = AudioIO::loopLengthSamples (req.bars, req.bpm, req.sampleRate);
    const double stepBeats = 4.0 / (double) kStepsPerBar;
    const juce::String prefix = genreName (req.genre) + "_Bass_";
    int seq = db.nextSequence (prefix);
    const int total = juce::jmax (1, req.quantity);

    if (progress)
        progress (0, total, "Generating bass loops");

    for (int n = 0; n < total; ++n)
    {
        if (cancel && cancel())
        {
            out.ok = out.filesWritten > 0;
            if (! out.ok)
                out.error = "Generation cancelled.";
            return out;
        }

        const juce::int64 seed = req.baseSeed != 0 ? (req.baseSeed + n) : (nowMs() + n * 7919);
        juce::Random rng (seed);

        std::vector<int> order ((size_t) pool.size());
        std::iota (order.begin(), order.end(), 0);
        for (int i = (int) order.size() - 1; i > 0; --i)
            std::swap (order[(size_t) i], order[(size_t) rng.nextInt (i + 1)]);

        const int used = req.usePattern ? want : juce::jmin (want, (int) order.size());
        std::vector<juce::AudioBuffer<float>> shots ((size_t) used);
        juce::StringArray parentPaths;

        size_t nextRandom = 0;
        for (int i = 0; i < used; ++i)
        {
            juce::String path;
            const auto& lock = req.locks[(size_t) juce::jmin (i, 3)];
            if (lock.locked && lock.path.isNotEmpty() && juce::File (lock.path).existsAsFile())
                path = lock.path;
            else
                path = pool[(size_t) order[nextRandom++ % order.size()]]->path;

            parentPaths.add (path);
            if (! loadStereoAt (io, juce::File (path), req.sampleRate, shots[(size_t) i], out.error))
                return out;
        }

        juce::String patternId;
        std::vector<BassHit> hits;
        if (req.usePattern)
        {
            patternId = "user_pattern";
            for (int st = 0; st < req.pattern.numSteps() && st < req.bars * kStepsPerBar; ++st)
                for (int r = 0; r < used; ++r)
                    if (req.pattern.getStep (r, st))
                        hits.push_back ({ st, r });
        }
        else
        {
            hits = makeBassPattern (req.genre, req.variation, req.bars, used, rng, patternId);
        }

        juce::AudioBuffer<float> full (2, loopN);
        full.clear();

        for (const auto& h : hits)
        {
            if (h.source < 0 || h.source >= used)
                continue;
            const int pos = AudioIO::beatsToSamples ((double) h.step * stepBeats, req.bpm, req.sampleRate);
            AudioIO::mixAt (full, shots[(size_t) h.source], pos, 1.0f,
                            AudioIO::kOneShotFadeInSamples, 0);
        }

        AudioIO::fadeLoopEnds (full, fade);
        if (req.protectPeaks)
            AudioIO::protectPeaks (full);

        const auto stem = prefix + padded4 (seq);
        auto file = AudioIO::uniqueWavPath (req.outputDir, stem);
        if (! io.writeWav (file, full, req.sampleRate, req.bitDepth, out.error))
            return out;

        juce::String err;
        if (! insertGenerated (db, file, Category::BassLoopsBase, req.genre, req.bpm, req.bars,
                               "bass", seed, patternId, parentPaths.joinIntoString (";"), 0, err))
        {
            out.error = err;
            return out;
        }

        {
            PatternFile sidecar;
            sidecar.kind = PatternKind::Bass;
            sidecar.grid.setBars (req.bars);
            sidecar.grid.clear();
            for (const auto& h : hits)
                if (h.source >= 0 && h.source < 4)
                    sidecar.grid.setStep (h.source, h.step, true);
            for (int i = 0; i < juce::jmin (4, parentPaths.size()); ++i)
                sidecar.samples[(size_t) i] = parentPaths[i];
            sidecar.bpm = req.bpm;
            sidecar.genre = req.genre;
            sidecar.save (patternSidecarFor (file));
        }

        out.paths.add (file.getFullPathName());
        ++out.filesWritten;
        ++seq;

        if (progress)
            progress (n + 1, total, "Generating bass loops");
    }

    out.ok = true;
    return out;
}

GenerateOutcome generateSpliceBatch (const SpliceRequest& req,
                                     LibraryDatabase& db,
                                     CancelFn cancel, ProgressFn progress)
{
    GenerateOutcome out;
    AudioIO io;

    if (req.sources.size() < 2)
    {
        out.error = "Select at least two bass loops to splice.";
        return out;
    }

    if (! req.outputDir.isDirectory() && ! req.outputDir.createDirectory())
    {
        out.error = "Invalid render destination: " + req.outputDir.getFullPathName();
        return out;
    }

    const int fade = AudioIO::fadeSamplesFor (req.sampleRate);
    const int loopN = AudioIO::loopLengthSamples (req.bars, req.bpm, req.sampleRate);
    const juce::String prefix = "Spliced_" + genreName (req.genre) + "_";
    int seq = db.nextSequence (prefix);
    const int total = juce::jmax (1, req.quantity);
    const int nsrc = (int) req.sources.size();

    std::vector<juce::AudioBuffer<float>> loops ((size_t) nsrc);
    juce::StringArray parentPaths;

    for (int i = 0; i < nsrc; ++i)
    {
        parentPaths.add (req.sources[(size_t) i].getFullPathName());
        if (! loadStereoAt (io, req.sources[(size_t) i], req.sampleRate, loops[(size_t) i], out.error))
            return out;

        if (loops[(size_t) i].getNumSamples() < 64)
        {
            out.error = "Source loop is too short: " + req.sources[(size_t) i].getFileName();
            return out;
        }
    }

    if (progress)
        progress (0, total, "Generating riff variations");

    for (int n = 0; n < total; ++n)
    {
        if (cancel && cancel())
        {
            out.ok = out.filesWritten > 0;
            if (! out.ok)
                out.error = "Generation cancelled.";
            return out;
        }

        const juce::int64 seed = req.baseSeed != 0 ? (req.baseSeed + n) : (nowMs() + n * 6841);
        juce::Random rng (seed);

        const double totalBeats = req.bars * 4.0;
        std::vector<double> sliceLens;
        double acc = 0.0;

        while (acc < totalBeats - 1.0e-6)
        {
            juce::Random* mixed = req.slice == SliceRes::Mixed ? &rng : nullptr;
            double len = sliceBeats (req.slice, mixed);
            if (acc + len > totalBeats)
                len = totalBeats - acc;
            if (len <= 1.0e-6)
                break;
            sliceLens.push_back (len);
            acc += len;
        }

        const int numSlices = (int) sliceLens.size();
        if (numSlices <= 0)
        {
            out.error = "Slice resolution produced an empty pattern.";
            return out;
        }

        auto seqSrc = spliceSequence (req.mode, numSlices, nsrc, req.variation, rng);

        juce::AudioBuffer<float> full (2, loopN);
        full.clear();

        int destPos = 0;
        for (int i = 0; i < numSlices; ++i)
        {
            const int sliceN = AudioIO::beatsToSamples (sliceLens[(size_t) i], req.bpm, req.sampleRate);
            if (sliceN <= 0)
                continue;

            const int srcIndex = juce::jlimit (0, nsrc - 1, seqSrc[(size_t) i]);
            const auto& src = loops[(size_t) srcIndex];
            const int srcN = src.getNumSamples();
            const int srcPos = destPos % juce::jmax (1, srcN);
            const int take = juce::jmin (sliceN, srcN);

            juce::AudioBuffer<float> slice (2, sliceN);
            slice.clear();

            for (int ch = 0; ch < 2; ++ch)
            {
                const int sch = juce::jmin (ch, src.getNumChannels() - 1);
                int copied = 0;
                int readAt = srcPos;

                while (copied < take)
                {
                    const int chunk = juce::jmin (take - copied, srcN - readAt);
                    if (chunk <= 0)
                    {
                        readAt = 0;
                        continue;
                    }
                    slice.copyFrom (ch, copied, src, sch, readAt, chunk);
                    copied += chunk;
                    readAt = 0;
                }
            }

            const int xf = juce::jmin (fade, sliceN / 3);
            AudioIO::mixAt (full, slice, destPos, 1.0f, xf, xf);
            destPos += sliceN;
            if (destPos >= loopN)
                break;
        }

        AudioIO::fadeLoopEnds (full, fade);
        if (req.protectPeaks)
            AudioIO::protectPeaks (full);

        const auto stem = prefix + padded4 (seq);
        auto file = AudioIO::uniqueWavPath (req.outputDir, stem);
        if (! io.writeWav (file, full, req.sampleRate, req.bitDepth, out.error))
            return out;

        juce::String err;
        const auto parents = req.parentIds.isNotEmpty() ? req.parentIds
                                                        : parentPaths.joinIntoString (";");
        if (! insertGenerated (db, file, Category::BassLoopsSpliced, req.genre, req.bpm, req.bars,
                               "splice", seed, spliceModeName (req.mode),
                               parents, req.parentDepth + 1, err))
        {
            out.error = err;
            return out;
        }

        out.paths.add (file.getFullPathName());
        ++out.filesWritten;
        ++seq;

        if (progress)
            progress (n + 1, total, "Generating riff variations");
    }

    out.ok = true;
    return out;
}

juce::StringArray bassPatternIds (Genre, Variation)
{
    return { "quarters", "eighths", "call_response", "chops_rests", "syncopated",
             "straight_phrase", "triplet_burst" };
}

} // namespace foundry
