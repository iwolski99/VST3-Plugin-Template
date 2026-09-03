#pragma once

#include "ConfigStore.h"
#include "LibraryDatabase.h"

namespace foundry
{

struct ScanOutcome
{
    bool ok = false;
    juce::String error;
    int added = 0;
    int removed = 0;
    int kept = 0;
};

ScanOutcome scanLibrary (ConfigStore& config, LibraryDatabase& db,
                         std::function<bool()> cancel,
                         std::function<void (int done, int total, juce::String status)> progress);

} // namespace foundry
