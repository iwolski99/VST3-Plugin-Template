#include "JobWorker.h"

namespace foundry
{

JobWorker::JobWorker()
    : juce::Thread ("FoundryWorker")
{
}

JobWorker::~JobWorker()
{
    requestCancel();
    stopThread (12000);
}

juce::String JobWorker::title() const
{
    const juce::ScopedLock sl (status.textLock);
    return status.title;
}

juce::String JobWorker::lastMessage() const
{
    const juce::ScopedLock sl (status.textLock);
    return status.message;
}

void JobWorker::setText (juce::String t, juce::String m)
{
    const juce::ScopedLock sl (status.textLock);
    status.title = std::move (t);
    status.message = std::move (m);
}

bool JobWorker::enqueue (Kind k)
{
    if (status.running.load())
        return false;

    if (isThreadRunning())
        stopThread (4000);

    kind.store (k);
    status.cancelRequested.store (false);
    status.done.store (0);
    status.total.store (1);
    status.running.store (true);
    startThread();
    return true;
}

bool JobWorker::startScan (ConfigStore& c, LibraryDatabase& d)
{
    const juce::ScopedLock sl (jobLock);
    if (status.running.load())
        return false;
    config = &c;
    db = &d;
    setText ("Refreshing library", "Scanning folders…");
    return enqueue (Kind::Scan);
}

bool JobWorker::startDrums (ConfigStore& c, LibraryDatabase& d, DrumRequest req)
{
    const juce::ScopedLock sl (jobLock);
    if (status.running.load())
        return false;
    config = &c;
    db = &d;
    drumReq = std::move (req);
    setText ("Generating drum loops", "Preparing…");
    return enqueue (Kind::Drums);
}

bool JobWorker::startBass (ConfigStore& c, LibraryDatabase& d, BassRequest req,
                           const std::vector<LibraryItem>& shots)
{
    const juce::ScopedLock sl (jobLock);
    if (status.running.load())
        return false;
    config = &c;
    db = &d;
    bassReq = std::move (req);
    bassPool = shots;
    setText ("Generating bass loops", "Preparing…");
    return enqueue (Kind::Bass);
}

bool JobWorker::startSplice (ConfigStore& c, LibraryDatabase& d, SpliceRequest req)
{
    const juce::ScopedLock sl (jobLock);
    if (status.running.load())
        return false;
    config = &c;
    db = &d;
    spliceReq = std::move (req);
    setText ("Generating riff variations", "Preparing…");
    return enqueue (Kind::Splice);
}

void JobWorker::run()
{
    DrumRequest drum;
    BassRequest bass;
    SpliceRequest splice;
    std::vector<LibraryItem> pool;
    ConfigStore* c = nullptr;
    LibraryDatabase* d = nullptr;
    Kind k = Kind::Idle;

    {
        const juce::ScopedLock sl (jobLock);
        k = kind.load();
        c = config;
        d = db;
        drum = drumReq;
        bass = bassReq;
        splice = spliceReq;
        pool = bassPool;
    }

    const auto cancel = [this] { return threadShouldExit() || status.cancelRequested.load(); };
    const auto progress = [this] (int done, int total, juce::String s)
    {
        status.done.store (done);
        status.total.store (juce::jmax (1, total));
        setText (s, juce::String (done) + " / " + juce::String (total));
        sendChangeMessage();
    };

    bool ok = false;
    juce::String err;

    if (d == nullptr || (k != Kind::Idle && c == nullptr))
    {
        err = "Internal error: generator is not ready.";
    }
    else
    {
        switch (k)
        {
            case Kind::Scan:
            {
                auto so = scanLibrary (*c, *d, cancel, progress);
                ok = so.ok;
                err = so.error;
                if (ok && (err.isEmpty() || err.startsWith ("Scan")))
                {
                    if (! err.startsWith ("Scan"))
                        err = "Indexed " + juce::String (so.added + so.kept) + " files"
                              + (so.removed > 0 ? (", removed " + juce::String (so.removed)) : juce::String());
                }
                break;
            }

            case Kind::Drums:
            {
                auto kicks = d->queryByRole (FolderRole::Kick, {});
                auto claps = d->queryByRole (FolderRole::Clap, {});
                if (claps.empty())
                    claps = d->queryByRole (FolderRole::Snare, {});
                auto hats  = d->queryByRole (FolderRole::Hat, {});
                auto rides = d->queryByRole (FolderRole::Ride, {});
                auto go = generateDrumBatch (drum, *d, kicks, claps, hats, rides, cancel, progress);
                ok = go.ok;
                err = go.error.isNotEmpty() ? go.error
                                            : ("Wrote " + juce::String (go.filesWritten) + " files.");
                break;
            }

            case Kind::Bass:
            {
                auto go = generateBassBatch (bass, *d, pool, cancel, progress);
                ok = go.ok;
                err = go.error.isNotEmpty() ? go.error
                                            : ("Wrote " + juce::String (go.filesWritten) + " files.");
                break;
            }

            case Kind::Splice:
            {
                auto go = generateSpliceBatch (splice, *d, cancel, progress);
                ok = go.ok;
                err = go.error.isNotEmpty() ? go.error
                                            : ("Wrote " + juce::String (go.filesWritten) + " files.");
                break;
            }

            case Kind::Idle:
            default:
                err = "No job.";
                break;
        }
    }

    lastOk.store (ok);
    setText (ok ? "Done" : "Failed", err);
    finishedKind.store (k);
    status.running.store (false);
    kind.store (Kind::Idle);
    sendChangeMessage();
}

} // namespace foundry
