#include "FactoryPresets.h"

namespace pt
{

// A handful of presets covering the corners of the parameter space, so a
// freshly-built plugin already demonstrates that it does something rather
// than opening on flat defaults with an empty preset list. Replace these with
// your own once the DSP is real.
const std::vector<FactoryPreset>& getFactoryPresets()
{
    static const std::vector<FactoryPreset> presets
    {
        { "Init", "Special", {} },

        { "Gentle Warmth", "Mix Bus",
          { { pid::drive, 12.0f }, { pid::tone, 8.0f }, { pid::mix, 40.0f } } },

        { "Driven", "Guitar",
          { { pid::drive, 65.0f }, { pid::tone, -15.0f }, { pid::mix, 100.0f } } },

        { "Vocal Air", "Vocals",
          { { pid::drive, 8.0f }, { pid::tone, 35.0f }, { pid::mix, 25.0f } } },

        { "Parallel Grit", "Drums",
          { { pid::drive, 80.0f }, { pid::tone, -30.0f }, { pid::mix, 35.0f } } },
    };

    return presets;
}

} // namespace pt
