/*  Constructs, runs and destroys the processor and its editor the way a host
    does - repeatedly, and in a few different orders - so a teardown bug
    surfaces here as a crash or (under -DTEMPLATE_SANITIZE=ON) a located
    ASan/UBSan report, rather than as "the DAW hangs when I remove the
    plugin" reported by an actual user months later.

    Build with -DTEMPLATE_BUILD_TESTS=ON, then run the TemplateLifecycleTest
    binary. */

#include "../Source/PluginEditor.h"
#include <iostream>

namespace
{
    void runOnce (bool openEditorFirst)
    {
        auto proc = std::make_unique<PluginTemplateAudioProcessor>();
        proc->prepareToPlay (48000.0, 512);

        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        buffer.clear();

        if (openEditorFirst)
        {
            std::unique_ptr<juce::AudioProcessorEditor> editor (proc->createEditor());
            editor->setSize (640, 420);
            proc->processBlock (buffer, midi);
            editor.reset();               // editor torn down before the processor
            proc->processBlock (buffer, midi);
        }
        else
        {
            proc->processBlock (buffer, midi);
            std::unique_ptr<juce::AudioProcessorEditor> editor (proc->createEditor());
            editor->setSize (640, 420);
            proc->processBlock (buffer, midi);
            // editor and processor torn down together, editor first (RAII order)
        }

        proc->releaseResources();
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    for (int i = 0; i < 20; ++i)
    {
        runOnce (i % 2 == 0);
        // Nothing to assert on directly - a use-after-free or double-free in
        // teardown either crashes outright or is caught by the sanitizer
        // build. Reaching this line 20 times over is the pass condition.
    }

    std::cout << "LifecycleTest: 20 construct/run/destroy cycles clean.\n";
    return 0;
}
