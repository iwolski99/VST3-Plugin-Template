#pragma once

#include "KnobComponent.h"
#include "../DSP/ToneRenderer.h"

class BinauralBeatsAudioProcessor;

namespace pt
{

//==============================================================================
class ExportTile : public juce::Component,
                   public juce::SettableTooltipClient
{
public:
    ExportTile (juce::String titleText, juce::String hintText);

    void setTheme (const Theme& t);
    void setActive (bool shouldBeActive);

    std::function<void()> onDragOut;
    std::function<void()> onSaveClick;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { hot = true;  repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hot = false; repaint(); }

private:
    juce::String title, hint;
    Theme theme { getTheme() };
    bool active = true, hot = false, dragStarted = false;
};

//==============================================================================
class MainPanel : public juce::Component,
                  private juce::Timer,
                  private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit MainPanel (BinauralBeatsAudioProcessor& processor);
    ~MainPanel() override;

    void setTheme (const Theme& t);

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void timerCallback() override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    void syncFrequencies (const juce::String& sourceId);
    void setParamValue (const juce::String& id, float actual);
    void updateBeatCaption();
    void updatePlayButton();
    void togglePlayback();
    void dragExport (ExportKind kind);
    void saveExport (ExportKind kind);
    void saveBoth();
    void showStatus (const juce::String& text, bool isError);

    BinauralBeatsAudioProcessor& proc;
    juce::AudioProcessorValueTreeState& apvts;

    ModeSwitch modeSwitch;
    ValueField leftField, rightField, beatField;
    ValueField durationField, fadeInField, fadeOutField;

    juce::TextButton playButton { "PLAY" };
    juce::Label timeLabel;
    juce::Label statusLabel;

    ExportTile binauralTile { "BINAURAL.WAV", "Stereo \xc2\xb7 drag out" };
    ExportTile toneTile     { "TONE.WAV",     "Mono \xc2\xb7 drag out" };
    juce::TextButton exportBinauralButton { "SAVE BINAURAL" };
    juce::TextButton exportToneButton     { "SAVE TONE" };
    juce::TextButton exportBothButton     { "SAVE BOTH" };

    std::unique_ptr<juce::FileChooser> chooser;

    Theme theme;
    bool syncing = false;
    float progress = 0.0f;
};

} // namespace pt
