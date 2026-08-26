# VST3 Plugin Template

A JUCE/C++ starting point for building VST3 effect plugins, extracted from the
GUI framework and build scaffolding used by [PrismReverbVST](https://github.com/iwolski99/PrismReverbVST).
The point of this repo isn't to save you from writing CMake - it's to stop
every new plugin's first playable build from looking like a stock JUCE demo.

## Why this exists

The usual first pass at a new plugin looks rough for a small, boring, entirely
fixable set of reasons: flat vector knobs instead of something with a body and
a highlight, inconsistent corner radii between panels and buttons, text that's
either dead-centred with no hierarchy or randomly tracked, zipper noise on
every parameter because nothing is smoothed, and a preset system (if there is
one) bolted on as an afterthought. None of that is about drawing skill - it's
about not having a system in place before you start turning knobs into pixels.

This template starts you with that system already built and already used
consistently:

- **A photographed knob, not a flat disc.** `Source/GUI/LookAndFeel.cpp`
  rotates one PNG (`Assets/knobs/knob_body_master.png`) with an
  `AffineTransform` instead of drawing vector arcs, plus a recessed collar,
  an LED-tick value arc, and a static specular sheen that doesn't rotate with
  the knob (a fixed light source is what makes a rotated bitmap read as
  "turning under a lamp" instead of "spinning in place").
- **One shared geometry.** Every panel and control corner comes from exactly
  two constants (`ui::panelCorner`, `ui::controlCorner`) in `LookAndFeel.h`.
  Inconsistent radii are the thing that reads as "amateur" from across the
  room even when nobody can point at why.
- **A theme, not scattered hex codes.** `Source/GUI/Theme.h` is the plugin's
  entire colour language - eight colours. Every component reads through it;
  none hard-code a colour. Reskinning the whole plugin is a one-file edit.
- **Text that centres correctly under letter-spacing**, a shadow/bloom/pill
  primitive set shared by every "lit" element, and a preset browser + save
  panel + A/B compare that are fully wired, not stubbed.
- **Every gain and mix control is smoothed** (`PluginProcessor.cpp`) rather
  than read raw each block - the zipper noise that produces otherwise is one
  of the fastest ways a fine-sounding plugin reads as unfinished.
- **A UI zoom selector that remembers your choice**, because nobody's first
  plugin has one and everybody's fifth plugin wishes it did.

None of this is reverb-specific - it came out of stripping PrismReverbVST down
to the parts that had nothing to do with reverb.

## What's actually in here

```
Source/
  PluginProcessor.*      Minimal processor: Input/Drive/Tone/Mix/Output/Bypass
  PluginEditor.*          Wires the GUI pieces together, owns zoom + theme
  DSP/ExampleEffect.h     Placeholder tilt-EQ + soft-clip effect - replace this
  Params/                 Parameter IDs + the APVTS layout
  Presets/                Factory presets + on-disk user preset manager
  GUI/
    Theme.*               The whole colour language, one file
    LookAndFeel.*          Panel/knob/button/combo drawing primitives
    KnobComponent.*        Knob, Combo, Toggle, ModeSwitch - APVTS-bound controls
    HeaderBar.*             Wordmark + title + Bypass
    FooterBar.*             Preset browser controls, A/B, Mix/Output, Zoom
    MainPanel.*             The one content panel - add your own controls here
    PresetBrowser.*         Full-window preset grid with category sidebar
    SavePresetPanel.*       In-editor preset naming prompt
Tests/
  DspSmokeTest.cpp        Feeds noise/silence through every Drive x Tone corner,
                          fails on any NaN/Inf/wild sample
  LifecycleTest.cpp       Construct/run/destroy the processor + editor 20x,
                          in different orders - catches teardown bugs
Assets/knobs/             The knob bitmap
scripts/rename_plugin.py  Renames the template into your new plugin
```

## Quick start

```bash
git clone <this-repo> MyNewPlugin
cd MyNewPlugin
python3 scripts/rename_plugin.py "MyNewPlugin" \
    --company "Your Company" \
    --website "https://example.com" \
    --bundle-id "com.yourcompany.mynewplugin" \
    --manufacturer-code "Ycmp" \
    --plugin-code "Mnpl" \
    --description "What it does, one line"
```

Then, in order:

1. **Pick your colours.** Edit the eight hex values in `Source/GUI/Theme.cpp`.
   Everything else updates itself.
2. **Replace the DSP.** Gut `Source/DSP/ExampleEffect.h` and write your real
   effect. Nothing else in the template knows or cares what happens inside
   `process()`.
3. **Update the parameters.** Add/remove entries in `Source/Params/ParamIDs.h`
   and `ParameterLayout.cpp` to match what your new DSP actually reads, then
   add/remove `Knob`/`Combo`/`Toggle` members in `Source/GUI/MainPanel.h/.cpp`
   to control them. `FooterBar` already exposes `Mix` and `Output` - keep,
   rename or remove those to match.
4. **Update the factory presets** in `Source/Presets/FactoryPresets.cpp` -
   they currently reference the placeholder Drive/Tone/Mix parameters.
5. **Draw your own header mark.** `HeaderBar::paint()` has a placeholder
   triangular glyph where your logo goes.
6. Build it (see below) and listen. Iterate on `Theme.cpp` and the DSP; you
   should not need to touch `LookAndFeel.cpp` for a normal plugin.

## Building

You need **CMake 3.22+** and a C++17 compiler. JUCE is downloaded
automatically the first time you configure.

### Windows (Visual Studio 2022)

```bat
build_windows.bat
```

...or by hand:

```bat
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target PluginTemplate_VST3
```

(After renaming, the target is `<YourPluginName>_VST3`.)

### macOS / Linux

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target PluginTemplate_VST3 -j
```

Linux also needs the usual JUCE development packages:

```bash
sudo apt install libx11-dev libxext-dev libxrandr-dev libxinerama-dev \
                 libxcursor-dev libfreetype6-dev libfontconfig1-dev libasound2-dev
```

A **Standalone** target builds alongside the VST3 (`--target
PluginTemplate_Standalone`), useful for auditioning without a DAW.

`COPY_PLUGIN_AFTER_BUILD` is on, so a Release build installs the plugin for
you. Manually, the VST3 lives in `build/PluginTemplate_artefacts/Release/VST3/`
and installs to:

| OS | Path |
|---|---|
| Windows | `C:\Program Files\Common Files\VST3\` |
| macOS | `~/Library/Audio/Plug-Ins/VST3/` |
| Linux | `~/.vst3/` |

### Tests

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DTEMPLATE_BUILD_TESTS=ON
cmake --build build --config Release --target TemplateDspTest TemplateLifecycleTest
./build/TemplateDspTest_artefacts/Release/TemplateDspTest
./build/TemplateLifecycleTest_artefacts/Release/TemplateLifecycleTest
```

Add `-DTEMPLATE_SANITIZE=ON` (non-MSVC) to run both under ASan+UBSan.

> **Note:** this template was assembled and reviewed for consistency, but has
> not been build-tested in the environment it was written in (no C++
> toolchain / JUCE fetch available there). Run a build immediately after
> cloning and before you start layering your own DSP on top, so any issue is
> obviously yours and not the template's.

## Design notes worth keeping in mind

- **Everything draws through `PluginTemplateLookAndFeel`.** If you catch
  yourself writing a `fillRoundedRectangle` with a hand-picked corner value
  inside a new component, stop - add a primitive to the look-and-feel instead
  so the next component you write gets it for free too.
- **Components only ever read a `Theme`, never a literal `juce::Colour`.**
  This is what makes reskinning (or adding a second theme/skin later) a
  one-file change instead of a grep-and-pray.
- **Smooth anything that changes level or blends signals.** `Drive` and
  `Tone` are read once per block in this template because they shape a static
  curve rather than a level - if you add a parameter that scales gain
  directly, smooth it the way `Input`/`Output`/`Mix`/`Bypass` are.
- **The `pt::` namespace is the reusable part.** It's short on purpose so it
  reads fine in every file; the rename script deliberately leaves it alone.
  If you want your own personal framework namespace across every plugin you
  build from this template, rename it once, by hand, and keep it that way
  everywhere going forward.

## License

No license file is included - add one (MIT, GPL, or otherwise) before you
share or ship a plugin built from this template.
