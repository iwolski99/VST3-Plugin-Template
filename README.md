# Foundry

A local VST3 **instrument** for Riddim and Dubstep loop generation. Foundry
indexes your own one-shots, generates drum loops and bass riffs from them,
recombines those riffs in the Riff Splicer, and plays the results back on the
spot - from the PLAY button, from the host transport, or from a MIDI note.

Because it is an instrument, load Foundry on an **instrument / generator**
track, not an effect slot. That also keeps hosts from suspending it: an effect
on a track with no incoming audio gets put to sleep by FL Studio's smart
disable and by Ableton's device-chain sleep, which is exactly the state in
which a sample player appears to do nothing when you press play.

No cloud. No subscription. Your sounds in. Ideas out.

This lives on a feature branch of the VST3 plugin template repository so the
original template on `main` stays untouched.

## Clone and build (Windows)

You need **CMake 3.22+**, **Visual Studio 2022** (Desktop C++ workload), and
an internet connection on the first configure so JUCE can be downloaded.

```bat
git clone -b claude/foundry-vst-bugs-pll8g9 --single-branch https://github.com/iwolski99/VST3-Plugin-Template.git Foundry
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
git clone -b claude/foundry-vst-bugs-pll8g9 --single-branch https://github.com/iwolski99/VST3-Plugin-Template.git Foundry
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
6. Generate drums or bass, then press **PLAY**, hit the host transport, or send
   Foundry a note.

Generated files are ordinary 24-bit WAV files on disk. Drag-and-drop from the
library list works in hosts that accept external file drags (including FL Studio).

## Playing loops

Foundry plays one loop at a time - whichever is currently loaded.

- **Clicking a row** in the Library browser loads that file and starts it.
- **PLAY** restarts the loaded loop from the top; **STOP** ends it. Finishing a
  generation batch arms its newest loop, so PLAY always has something to play.
- **SYNC** (on by default) starts and stops with the host transport and folds
  the loop into the timeline, so it stays locked to the bar wherever playback
  begins.
- **Any MIDI note** triggers the loop from the top and releasing it stops the
  loop. The note is a trigger only: Foundry plays finished loops, so the pitch
  you press never transposes the sample. Whichever key you hit in the piano
  roll, the loop plays back exactly as rendered.

The loaded loop and the SYNC setting are saved with the session.

## Tests

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DFOUNDRY_BUILD_TESTS=ON
cmake --build build --config Release --target FoundryEngineTest FoundryPlaybackTest FoundryLifecycleTest
```

- `FoundryEngineTest` - offline generation, database and rendering.
- `FoundryPlaybackTest` - drives the processor the way a host does and checks
  that audio actually comes out of every trigger path.
- `FoundryLifecycleTest` - repeated construct / run / destroy cycles.

Add `-DFOUNDRY_SANITIZE=ON` to build the tests with AddressSanitizer and UBSan.

## Format

64-bit VST3 instrument (`Instrument|Sampler`), MIDI in, stereo out, no audio
input. WAV sources and 24-bit WAV output. Host sample rate by default
(44.1 / 48 kHz overrides in Settings). All rendering happens off the audio
thread, and the loaded clip is re-decoded if the host changes sample rate.
