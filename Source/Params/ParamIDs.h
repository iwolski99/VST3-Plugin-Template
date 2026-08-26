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
    inline constexpr const char* input   = "input";
    inline constexpr const char* drive   = "drive";
    inline constexpr const char* tone    = "tone";
    inline constexpr const char* mix     = "mix";
    inline constexpr const char* output  = "output";
    inline constexpr const char* bypass  = "bypass";
}

//==============================================================================
/** Non-automatable editor state stored alongside the parameter tree. */
namespace stateID
{
    inline constexpr const char* zoom       = "uiZoom";
    inline constexpr const char* presetName = "presetName";
}

} // namespace pt
