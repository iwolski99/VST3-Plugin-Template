/*  Renders the Foundry editor to PNG files with a small synthetic library and
    a riff loaded - used to check the layout without a DAW (run under Xvfb).

        FoundryScreenshot <output-dir>

    Build with -DFOUNDRY_BUILD_TESTS=ON. */

#include "../Source/PluginEditor.h"
#include <iostream>

namespace
{
    void writeShot (const juce::File& f, double freq, double seconds, double decay)
    {
        const double sr = 44100.0;
        const int n = juce::roundToInt (sr * seconds);
        juce::AudioBuffer<float> b (1, n);
        juce::Random rng ((juce::int64) (freq * 1000));
        for (int i = 0; i < n; ++i)
        {
            const double t = i / sr;
            const double env = std::exp (-t * decay);
            const double tone = std::sin (juce::MathConstants<double>::twoPi * freq * t * (1.0 + 0.6 * std::exp (-t * 30.0)));
            const double noise = (rng.nextFloat() * 2.0 - 1.0) * std::exp (-t * 60.0) * 0.4;
            b.setSample (0, i, (float) (0.8 * env * tone + noise));
        }
        foundry::AudioIO io;
        juce::String err;
        io.writeWav (f, b, sr, 24, err);
    }

    void snap (FoundryAudioProcessorEditor& ed, const juce::File& file)
    {
        // A first paint requests the visible thumbnails; then let the
        // background thread and timers catch up before the real snapshot.
        ed.createComponentSnapshot (ed.getLocalBounds(), true, 1.0f);
        for (int i = 0; i < 40; ++i)
        {
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        }
        auto img = ed.createComponentSnapshot (ed.getLocalBounds(), true, 1.0f);
        file.deleteFile();
        juce::FileOutputStream os (file);
        juce::PNGImageFormat png;
        png.writeImageToStream (img, os);
        std::cout << "wrote " << file.getFullPathName() << "\n";
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "shots");
    out.createDirectory();

    auto root = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("FoundryShots");
    root.deleteRecursively();
    root.createDirectory();

    auto proc = std::make_unique<FoundryAudioProcessor>();
    proc->prepareToPlay (48000.0, 512);

    auto& cfg = proc->getConfig();
    cfg.setRoot (root);
    juce::String err;
    cfg.createRecommendedStructure (err);
    proc->reopenDatabase (err);

    const auto kicks = cfg.folder (foundry::FolderRole::Kick);
    const auto bass  = cfg.folder (foundry::FolderRole::Bass);
    const char* kickNames[] = { "Kick_Punchy_01", "Kick_Sub_Boom", "Kick_Riddim_Hard", "Kick_Clicky" };
    const char* bassNames[] = { "Bass_Growl_A", "Bass_Wobble_LFO_1-4", "Bass_Reese_Dark", "Bass_Yoi_Vowel",
                                "Bass_Metallic_Screech", "Bass_Sub_Sine_F", "Bass_Neuro_Grind_02", "Bass_Riddim_Saw_Stack",
                                "Bass_Formant_Talk", "Bass_Gritty_Square", "Bass_Long_Tail_Pluck", "Bass_FM_Bell_Hit" };
    for (int i = 0; i < 4; ++i)
        writeShot (kicks.getChildFile (juce::String (kickNames[i]) + ".wav"), 50.0 + i * 6, 0.5, 8.0 + i * 3);
    for (int i = 0; i < 12; ++i)
        writeShot (bass.getChildFile (juce::String (bassNames[i]) + ".wav"), 40.0 + i * 7, 0.4 + 0.15 * (i % 4), 1.5 + i * 0.6);
    bass.getChildFile ("Growls").createDirectory();
    writeShot (bass.getChildFile ("Growls").getChildFile ("Growl_Deep.wav"), 45.0, 1.0, 2.0);
    cfg.folder (foundry::FolderRole::Snare);
    writeShot (cfg.folder (foundry::FolderRole::Clap).getChildFile ("Clap_Tight.wav"), 900.0, 0.3, 20.0);

    // One of your own folders, a saved pattern, and a generated bass loop with its pattern.
    auto mine = root.getChildFile ("Field Recordings");
    mine.createDirectory();
    writeShot (mine.getChildFile ("Metal_Clank.wav"), 700.0, 0.5, 6.0);
    cfg.get().userFolders.add (mine.getFullPathName());

    {
        foundry::PatternFile pf;
        pf.kind = foundry::PatternKind::Drums;
        pf.grid = foundry::defaultDrumPattern (foundry::Genre::Riddim, 2);
        pf.save (cfg.patternDir (false).getChildFile (juce::String ("Riddim Bounce") + foundry::kPatternExtension));
        pf.grid = foundry::defaultDrumPattern (foundry::Genre::Dubstep, 1);
        pf.save (cfg.patternDir (false).getChildFile (juce::String ("Halftime Basic") + foundry::kPatternExtension));
        pf.kind = foundry::PatternKind::Bass;
        pf.grid.setBars (1);
        pf.grid.clear();
        for (auto [r, st] : { std::pair<int, int> { 0, 0 }, { 0, 3 }, { 1, 6 }, { 1, 8 }, { 2, 11 }, { 0, 12 }, { 3, 14 } })
            pf.grid.setStep (r, st, true);
        pf.save (cfg.patternDir (true).getChildFile (juce::String ("Wobble Call") + foundry::kPatternExtension));
    }

    auto scan = foundry::scanLibrary (cfg, proc->getLibrary(), nullptr, nullptr);
    if (! scan.ok)
        std::cerr << "scan: " << scan.error << "\n";

    // Favourite a couple so the Favourites folder has something in it.
    for (auto& it : proc->getLibrary().queryUnderFolder (bass.getFullPathName(), "Growl", 0))
        proc->getLibrary().setFavorite (it.id, true, err);

    // A riff: six slots filled, one rest, one locked, varied lengths and offsets.
    foundry::RiffSketch sk;
    const int pick[] = { 0, 2, -1, 1, 4, 6, 0, 7 };
    for (int i = 0; i < foundry::kRiffSlots; ++i)
    {
        if (pick[i] < 0)
            continue;
        auto& s = sk.slots[(size_t) i];
        s.path = bass.getChildFile (juce::String (bassNames[pick[i]]) + ".wav").getFullPathName();
        s.lengthSteps = (i == 3) ? 2 : (i == 5 ? 8 : 4);
        s.offsetSeconds = (i == 4) ? 0.06 : 0.0;
    }
    sk.slots[0].locked = true;
    sk.slots[6].path.clear();          // slot 6 (1/2) rings through 7; 3 is silent
    sk.slots[6].gain = 0.6f;
    proc->setSketch (sk, false);

    {
        std::unique_ptr<juce::AudioProcessorEditor> base (proc->createEditor());
        auto& ed = dynamic_cast<FoundryAudioProcessorEditor&> (*base);
        ed.setSize (FoundryAudioProcessorEditor::kBaseWidth, FoundryAudioProcessorEditor::kBaseHeight);

        ed.getBrowser().showFolder (bass);
        ed.getBrowser().selectAll();
        snap (ed, out.getChildFile ("01-riffmaker-multiselect.png"));

        ed.getBrowser().navigateTo (foundry::BrowserModel::root());
        snap (ed, out.getChildFile ("02-browser-root.png"));

        ed.applyThemeByName ("Purple");
        ed.getBrowser().showFolder (cfg.patternDir (true));
        ed.openPattern (cfg.patternDir (true).getChildFile (juce::String ("Wobble Call") + foundry::kPatternExtension));
        snap (ed, out.getChildFile ("03-purple-bass-pattern.png"));

        ed.applyThemeByName ("Green");
        ed.getBrowser().showFolder (cfg.patternDir (false));
        ed.openPattern (cfg.patternDir (false).getChildFile (juce::String ("Riddim Bounce") + foundry::kPatternExtension));
        snap (ed, out.getChildFile ("04-green-drum-pattern.png"));

        ed.applyThemeByName ("Blue");
        ed.showTab (FoundryAudioProcessorEditor::riffMakerTab);
        ed.setSize (1500, 980);
        ed.getBrowser().showFolder (bass);
        snap (ed, out.getChildFile ("05-blue-resized.png"));

        ed.applyThemeByName ("Red");
        ed.setSize (1000, 740);
        snap (ed, out.getChildFile ("06-red-minimum-size.png"));
        ed.applyThemeByName ("Orange");
    }

    proc->releaseResources();
    proc.reset();
    root.deleteRecursively();
    return 0;
}
