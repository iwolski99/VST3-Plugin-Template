#pragma once

#include "ConfigStore.h"
#include "Generators.h"
#include "LibraryDatabase.h"
#include "LibraryScan.h"

namespace foundry
{

class JobWorker : public juce::Thread,
                  public juce::ChangeBroadcaster
{
public:
    enum class Kind { Idle, Scan, Drums, Bass, Splice };

    JobWorker();
    ~JobWorker() override;

    bool isBusy() const noexcept { return status.running.load(); }
    void requestCancel() { status.cancelRequested.store (true); }

    Kind currentKind() const noexcept { return kind.load(); }

    /** The kind of the job that finished most recently. */
    Kind lastKind() const noexcept { return finishedKind.load(); }

    /** True when the last finished job rendered new loops into the library. */
    bool lastJobGenerated() const noexcept
    {
        const auto k = finishedKind.load();
        return k == Kind::Drums || k == Kind::Bass || k == Kind::Splice;
    }

    int  progressDone() const noexcept { return status.done.load(); }
    int  progressTotal() const noexcept { return juce::jmax (1, status.total.load()); }

    juce::String title() const;
    juce::String lastMessage() const;
    bool lastSucceeded() const noexcept { return lastOk.load(); }

    bool startScan (ConfigStore&, LibraryDatabase&);
    bool startDrums (ConfigStore&, LibraryDatabase&, DrumRequest);
    bool startBass (ConfigStore&, LibraryDatabase&, BassRequest,
                    const std::vector<LibraryItem>& shots);
    bool startSplice (ConfigStore&, LibraryDatabase&, SpliceRequest);

    void run() override;

private:
    struct Status
    {
        std::atomic<bool> running { false };
        std::atomic<bool> cancelRequested { false };
        std::atomic<int>  done { 0 };
        std::atomic<int>  total { 1 };
        juce::String title, message;
        mutable juce::CriticalSection textLock;
    };

    void setText (juce::String title, juce::String message);
    bool enqueue (Kind k);

    Status status;
    std::atomic<Kind> kind { Kind::Idle };
    std::atomic<Kind> finishedKind { Kind::Idle };
    std::atomic<bool> lastOk { true };

    ConfigStore* config = nullptr;
    LibraryDatabase* db = nullptr;
    DrumRequest drumReq;
    BassRequest bassReq;
    SpliceRequest spliceReq;
    std::vector<LibraryItem> bassPool;

    juce::CriticalSection jobLock;
};

} // namespace foundry
