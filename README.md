# Foundry

A local VST3 loop-generation and sample-workflow plugin for Riddim and Dubstep.
Foundry indexes your own one-shots, generates drum loops and bass riffs from them,
recombines those riffs in the Riff Splicer, and lets you audition and drag the
results straight into a DAW.

No cloud. No subscription. Your sounds in. Ideas out.

This lives on the `cursor/foundry-vst-1f99` branch of the VST3 plugin template
repository so the original template on `main` stays untouched.

## Clone and build (Windows)

You need **CMake 3.22+**, **Visual Studio 2022** (Desktop C++ workload), and
an internet connection on the first configure so JUCE can be downloaded.

```bat
git clone -b cursor/foundry-vst-1f99 --single-branch https://github.com/iwolski99/VST3-Plugin-Template.git Foundry
cd Foundry
build_windows.bat
```

Run `build_windows.bat` **as Administrator** so it can copy `Foundry.vst3`
into the default Windows VST3 folder:

```text
C:\Program Files\Common Files\VST3\Foundry.vst3
```

If the copy is denied, the built plugin is still at:

```text
build\Foundry_artefacts\Release\VST3\Foundry.vst3
```

Copy that folder into `C:\Program Files\Common Files\VST3\` yourself, then
rescan plugins in FL Studio.

### Manual Windows build

```bat
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Foundry_VST3
```

## macOS / Linux

```bash
git clone -b cursor/foundry-vst-1f99 --single-branch https://github.com/iwolski99/VST3-Plugin-Template.git Foundry
cd Foundry
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target Foundry_VST3 -j
```

Linux packages:

```bash
sudo apt install libx11-dev libxext-dev libxrandr-dev libxinerama-dev \
                 libxcursor-dev libfreetype6-dev libfontconfig1-dev libasound2-dev
```

A **Standalone** target (`Foundry_Standalone`) builds alongside the VST3 for
auditioning without a DAW.

| OS | Installed VST3 path |
|---|---|
| Windows | `C:\Program Files\Common Files\VST3\` |
| macOS | `~/Library/Audio/Plug-Ins/VST3/` |
| Linux | `~/.vst3/` |

## First launch

1. Open **Settings**.
2. Choose a root library folder.
3. Click **Create Recommended Structure** (or map your existing Kick / Clap / Hat / Ride / Bass folders).
4. Drop WAV one-shots into those folders.
5. **Refresh Library**.
6. Generate drums or bass, audition from the Library tab, drag into the DAW.

Generated files are ordinary 24-bit WAV files on disk. Drag-and-drop from the
library list works in hosts that accept external file drags (including FL Studio).

## Tests

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DFOUNDRY_BUILD_TESTS=ON
cmake --build build --config Release --target FoundryEngineTest FoundryLifecycleTest
```

## Format

64-bit VST3. WAV sources and 24-bit WAV output. Host sample rate by default
(44.1 / 48 kHz overrides in Settings). All rendering happens off the audio thread.
