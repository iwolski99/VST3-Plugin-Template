#pragma once

#include "../Common.h"

namespace pt
{

//==============================================================================
/** Parameter IDs. Never renamed once shipped - a saved project or preset
    references these strings, not the enum position, so renaming one silently
    breaks every file written before the rename. Add new ones at the bottom. */
namespace pid
{
    inline constexpr const char* leftFreq  = "leftFreq";
    inline constexpr const char* rightFreq = "rightFreq";
    inline constexpr const char* beatFreq  = "beatFreq";
    inline constexpr const char* duration  = "duration";
    inline constexpr const char* fadeIn    = "fadeIn";
    inline constexpr const char* fadeOut   = "fadeOut";
    inline constexpr const char* mode      = "mode";
    inline constexpr const char* mix       = "mix";
    inline constexpr const char* output    = "output";
    inline constexpr const char* bypass    = "bypass";
}

//==============================================================================
/** Non-automatable editor state stored alongside the parameter tree. */
namespace stateID
{
    inline constexpr const char* zoom       = "uiZoom";
    inline constexpr const char* presetName = "presetName";
}

} // namespace pt
