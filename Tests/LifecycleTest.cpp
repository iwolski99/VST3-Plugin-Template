/*  Constructs, runs and destroys the processor and its editor the way a host
    does - repeatedly, in a few different orders - so a teardown bug surfaces
    here rather than as "the DAW hangs when I remove the plugin".

    Build with -DFOUNDRY_BUILD_TESTS=ON, then run FoundryLifecycleTest. */

#include "../Source/PluginEditor.h"
#include <iostream>

namespace
{
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
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    for (int i = 0; i < 8; ++i)
        runOnce (i % 2 == 0);

    std::cout << "LifecycleTest: construct/run/destroy cycles clean.\n";
    return 0;
}
