/*  Constructs, runs and destroys the processor and its editor the way a host
    does - repeatedly, in a few different orders - so a teardown bug surfaces
    here rather than as "the DAW hangs when I remove the plugin".

    Build with -DFOUNDRY_BUILD_TESTS=ON, then run FoundryLifecycleTest. */

#include "../Source/PluginEditor.h"
#include <iostream>

namespace
{
    bool gFailed = false;

    void runOnce (bool openEditorFirst)
    {
        auto proc = std::make_unique<FoundryAudioProcessor>();
        proc->prepareToPlay (48000.0, 512);

        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        buffer.clear();

        const bool canOpenEditor = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() != nullptr;

        if (openEditorFirst && canOpenEditor)
        {
            std::unique_ptr<juce::AudioProcessorEditor> editor (proc->createEditor());
            editor->setSize (FoundryAudioProcessorEditor::kBaseWidth,
                             FoundryAudioProcessorEditor::kBaseHeight);
            proc->processBlock (buffer, midi);
            editor.reset();
            proc->processBlock (buffer, midi);
        }
        else
        {
            proc->processBlock (buffer, midi);

            if (canOpenEditor)
            {
                std::unique_ptr<juce::AudioProcessorEditor> editor (proc->createEditor());
                editor->setSize (FoundryAudioProcessorEditor::kBaseWidth,
                                 FoundryAudioProcessorEditor::kBaseHeight);
                proc->processBlock (buffer, midi);
            }
        }

        proc->releaseResources();
    }

    /** Click a slot, press Delete: it goes back to silence. */
    void deleteKeySilencesSlot()
    {
        if (juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() == nullptr)
            return;

        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("FoundryLifecycleTone");
        dir.createDirectory();
        auto tone = dir.getChildFile ("tone.wav");
        {
            juce::AudioBuffer<float> b (1, 4800);
            for (int i = 0; i < b.getNumSamples(); ++i)
                b.setSample (0, i, 0.5f * std::sin ((float) i * 0.05f));
            foundry::AudioIO io;
            juce::String err;
            io.writeWav (tone, b, 48000.0, 24, err);
        }

        auto proc = std::make_unique<FoundryAudioProcessor>();
        proc->prepareToPlay (48000.0, 512);
        juce::String err;
        proc->assignSample (5, tone, err);

        std::unique_ptr<juce::AudioProcessorEditor> base (proc->createEditor());
        auto& ed = dynamic_cast<FoundryAudioProcessorEditor&> (*base);
        auto& panel = ed.getRiffPanel();

        panel.selectSlot (5);
        const bool handled = panel.keyPressed (juce::KeyPress (juce::KeyPress::deleteKey));
        const bool ok = handled && proc->getSketch().slots[5].isEmpty();
        std::cout << (ok ? "LifecycleTest: Delete key silences the selected slot.\n"
                         : "LifecycleTest: FAIL - Delete key did not silence the slot.\n");
        if (! ok)
            gFailed = true;

        base.reset();
        proc->releaseResources();
        dir.deleteRecursively();
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    deleteKeySilencesSlot();

    for (int i = 0; i < 8; ++i)
        runOnce (i % 2 == 0);

    std::cout << "LifecycleTest: construct/run/destroy cycles clean.\n";
    return gFailed ? 1 : 0;
}
