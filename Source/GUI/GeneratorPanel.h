#pragma once

#include "BassPanel.h"
#include "DrumPanel.h"
#include "SplicerPanel.h"
#include "TabBar.h"

namespace pt
{

/** Foundry's original batch tools - drum loops, bass riffs, the splicer -
    gathered on one tab. They work exactly as before; results land in the
    Generated folder of the browser, where any of them can be dropped into a
    Riff Maker slot. */
class GeneratorPanel : public juce::Component
{
public:
    enum Page { drums = 0, bass, splicer };

    explicit GeneratorPanel (FoundryAudioProcessor& p);

    void setTheme (const Theme& t);
    void showPage (int page);
    int  getPage() const noexcept { return page; }

    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;

    DrumPanel drumPanel;
    BassPanel bassPanel;
    SplicerPanel splicerPanel;

private:
    TabBar tabs { { "DRUM LOOPS", "BASS RIFFS", "SPLICER" } };
    int page = drums;
    Theme theme { getTheme() };
};

} // namespace pt
