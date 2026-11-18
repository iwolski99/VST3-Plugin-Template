#pragma once

#include "LibraryDatabase.h"
#include <set>

namespace foundry
{

/** Waveform thumbnails for the sample browser.

    Tiles ask for a thumbnail every time they paint; a miss queues the file for
    a background thread (newest requests first, so whatever is on screen right
    now wins) and the tile draws a placeholder until it arrives. Results are
    kept in memory and in the library database, so a folder you have opened
    once draws instantly next time, even after restarting the DAW. */
class ThumbnailCache : private juce::Thread
{
public:
    static constexpr int kBins = 48;

    struct Thumb
    {
        std::vector<juce::uint8> peaks;     // kBins values, 0..255
        double seconds = 0.0;
        bool failed = false;                // unreadable file: draw a broken tile, don't retry
    };

    explicit ThumbnailCache (LibraryDatabase& db);
    ~ThumbnailCache() override;

    /** Returns the thumbnail if ready, otherwise queues it and returns null. */
    std::shared_ptr<const Thumb> request (const juce::String& path);

    /** Bumped every time a thumbnail finishes, so the UI knows to repaint. */
    int generation() const noexcept { return finished.load(); }

    void forget (const juce::String& path);

private:
    void run() override;
    std::shared_ptr<Thumb> compute (const juce::String& path);

    LibraryDatabase& database;
    juce::AudioFormatManager formats;

    juce::CriticalSection lock;
    std::map<juce::String, std::shared_ptr<const Thumb>> done;
    std::vector<juce::String> queue;        // back = most recent request
    std::set<juce::String> queued;
    juce::WaitableEvent wake;
    std::atomic<int> finished { 0 };

    static constexpr size_t kMaxQueue = 600;
    static constexpr size_t kMaxCached = 6000;
};

} // namespace foundry
