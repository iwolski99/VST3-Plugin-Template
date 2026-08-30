#include "FactoryPresets.h"

namespace pt
{

const std::vector<FactoryPreset>& getFactoryPresets()
{
    static const std::vector<FactoryPreset> presets
    {
        { "Init", "Special", {} },

        { "Delta Sleep", "Delta",
          { { pid::leftFreq, 100.0f }, { pid::rightFreq, 102.0f }, { pid::beatFreq, 2.0f },
            { pid::duration, 1200.0f }, { pid::fadeIn, 8.0f }, { pid::fadeOut, 8.0f },
            { pid::mode, 1.0f } } },

        { "Theta Meditation", "Theta",
          { { pid::leftFreq, 140.0f }, { pid::rightFreq, 146.0f }, { pid::beatFreq, 6.0f },
            { pid::duration, 600.0f }, { pid::fadeIn, 4.0f }, { pid::fadeOut, 4.0f },
            { pid::mode, 1.0f } } },

        { "Alpha Relax", "Alpha",
          { { pid::leftFreq, 200.0f }, { pid::rightFreq, 210.0f }, { pid::beatFreq, 10.0f },
            { pid::duration, 300.0f }, { pid::fadeIn, 3.0f }, { pid::fadeOut, 3.0f },
            { pid::mode, 1.0f } } },

        { "Beta Focus", "Beta",
          { { pid::leftFreq, 220.0f }, { pid::rightFreq, 238.0f }, { pid::beatFreq, 18.0f },
            { pid::duration, 180.0f }, { pid::fadeIn, 2.0f }, { pid::fadeOut, 2.0f },
            { pid::mode, 1.0f } } },

        { "Gamma Insight", "Gamma",
          { { pid::leftFreq, 250.0f }, { pid::rightFreq, 290.0f }, { pid::beatFreq, 40.0f },
            { pid::duration, 120.0f }, { pid::fadeIn, 1.5f }, { pid::fadeOut, 1.5f },
            { pid::mode, 1.0f } } },

        { "Pure 432", "Tones",
          { { pid::leftFreq, 432.0f }, { pid::rightFreq, 432.0f }, { pid::beatFreq, 0.0f },
            { pid::duration, 60.0f }, { pid::mode, 0.0f } } },

        { "Pure 440", "Tones",
          { { pid::leftFreq, 440.0f }, { pid::rightFreq, 440.0f }, { pid::beatFreq, 0.0f },
            { pid::duration, 60.0f }, { pid::mode, 0.0f } } },

        { "Pure 528", "Tones",
          { { pid::leftFreq, 528.0f }, { pid::rightFreq, 528.0f }, { pid::beatFreq, 0.0f },
            { pid::duration, 60.0f }, { pid::mode, 0.0f } } },

        { "Both Preview", "Special",
          { { pid::leftFreq, 200.0f }, { pid::rightFreq, 210.0f }, { pid::beatFreq, 10.0f },
            { pid::duration, 30.0f }, { pid::fadeIn, 2.0f }, { pid::fadeOut, 2.0f },
            { pid::mode, 2.0f } } },
    };

    return presets;
}

} // namespace pt
