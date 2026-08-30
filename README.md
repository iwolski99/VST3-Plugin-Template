# BinauralBeats

A JUCE VST3 (and standalone) generator for **binaural beats** and **pure tones**,
built on the plugin template in this repo. This lives on its own branch so
`main` stays the untouched template.

Headphones are required for binaural beats: the left ear hears one sine, the
right ear hears another, and the brain supplies the difference as a beat.

## What it does

- Type a **left frequency**, **right frequency**, and **beat frequency**. The
  three stay linked: changing left or beat moves the right ear; changing right
  updates the beat (`|right − left|`).
- Set a **duration** (seconds, or `mm:ss`) plus **fade in** and **fade out**.
  Preview playback and exported files both use that envelope.
- Switch **Pure Tone** (mono sine on the left/carrier frequency), **Binaural**
  (stereo L/R), or **Both** (preview mix; export writes two files).
- **Drag** a WAV tile onto a DAW track or the desktop, or use the **Save**
  buttons. Pure-tone exports are mono; binaural exports are stereo. Files are
  44.1 kHz, 16-bit PCM.

Factory presets cover Delta / Theta / Alpha / Beta / Gamma sessions plus
432 / 440 / 528 Hz tones.

## Building

You need **CMake 3.22+** and a C++17 compiler. JUCE is downloaded on first
configure.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DTEMPLATE_BUILD_TESTS=ON
cmake --build build --config Release --target BinauralBeats_VST3 BinauralBeats_Standalone TemplateDspTest TemplateLifecycleTest -j
```

On Linux, install the usual JUCE packages first:

```bash
sudo apt install libx11-dev libxext-dev libxrandr-dev libxinerama-dev \
                 libxcursor-dev libfreetype6-dev libfontconfig1-dev libasound2-dev
```

Windows: `build_windows.bat`, or the Visual Studio generator in the command
above with `--target BinauralBeats_VST3`.

The VST3 installs after a Release build (`COPY_PLUGIN_AFTER_BUILD` is on):

| OS | Path |
|---|---|
| Windows | `C:\Program Files\Common Files\VST3\` |
| macOS | `~/Library/Audio/Plug-Ins/VST3/` |
| Linux | `~/.vst3/` |

Standalone: `build/BinauralBeats_artefacts/Release/Standalone/BinauralBeats`

## Tests

```bash
./build/TemplateDspTest_artefacts/Release/TemplateDspTest
./build/TemplateLifecycleTest_artefacts/Release/TemplateLifecycleTest
```

The DSP test checks fade-in/out, stereo difference on binaural vs identity on
pure tone, playback stopping at duration, and a WAV round-trip. The lifecycle
test constructs and tears down the processor and editor 20 times.

## Using it

1. Drop **BinauralBeats** on a stereo track (or open the standalone).
2. Pick a mode. For a 10 Hz alpha beat, left `200`, beat `10` → right becomes
   `210`.
3. Set duration and fades. Press **Play** (or spacebar) to preview.
4. Drag **BINAURAL.WAV** / **TONE.WAV** into the DAW, or Save Binaural / Save
   Tone / Save Both.

Mix in the footer blends the generator against the track input (default 100 %
wet). Output is the level trim.

## Layout

```
Source/
  PluginProcessor.*      Generator + timed playback + WAV export
  PluginEditor.*         Zoom, theme, drag-and-drop container
  DSP/ToneRenderer.h     Sine + raised-cosine envelope (header-only)
  DSP/WavExporter.h      Block-wise 16-bit WAV writer
  Params/                Left/Right/Beat, duration, fades, mode, mix, output
  Presets/               Factory bank (brainwave sessions + tones)
  GUI/                   Template chrome + ValueField + export tiles
```
