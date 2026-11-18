# Foundry

A local VST3 **instrument** for building riffs out of your own samples.

Foundry's main page is the **Riff Maker**: eight slots on a beat grid, each one
playing just the start of a sample. Drag sounds in from the sample browser,
slide where each one starts, choose how long each one lasts, lock the keepers
and roll the dice on the rest. When it slaps, **RENDER** saves it as a WAV in
*My Riffs*, or grab **DRAG TO DAW** and drop it straight onto your playlist.

The original batch tools (Riddim / Dubstep drum loops, bass riffs and the riff
splicer) are still here, on the **Generator** tab.

No cloud. No subscription. Your sounds in. Ideas out.

## The Riff Maker

```
 ┌ PLAY STOP SYNC │ 1/4 grid │ 145 BPM │ DICE UNDO REDO CLEAR ─────── RENDER  DRAG TO DAW ┐
 │ [1 sample] [2 sample] [3 empty ] [4 sample]                                             │
 │ [5 sample] [6 sample] [7 sample] [8 sample]                                             │
 ├ Library › Drums › Kicks                        [search]  TILES LIST  RESCAN ─────────────┤
 │ ▢ ▢ ▢ ▢ ▢ ▢ ▢ ▢   sample browser: tiles with waveforms and names                       │
 └ preview · volume · zoom ────────────────────────────────────────────────────────────────┘
```

- **Slots sit on a fixed grid.** Slot 1 starts on beat 1, slot 2 on beat 2, and
  so on: eight slots on the default 1/4 grid make a 2-bar riff. Switch the
  grid to 1/16, 1/8 or 1/2 for faster or slower riffs.
- **Each slot plays only the first slice of its sample**, a quarter note by
  default. Change it with the ◂ ▸ arrows (1/16 up to 1 bar). A slice longer
  than the grid is cut off when the next slot starts, so the riff behaves like
  a bass line. If the next slot is empty, the slice rings on into it.
- **Slide the start position** by dragging the waveform left or right, the way
  you slip-edit a clip in FL Studio's playlist: the slot stays on the grid and
  the audio moves underneath it. Hold Shift for fine moves; double-click the
  waveform to reset. The strip under the waveform shows where in the file the
  slot is playing from.
- **Volume per slot**: drag the dB value up or down (double-click resets).
- **Lock** a slot (padlock) and the dice and CLEAR leave it alone.
- **DICE** fills every unlocked slot with a random sample from whatever folder
  is open in the browser, or from your search results. Open *Drums › Kicks*
  and roll for kick riffs, or open *Bass* and roll for bass riffs.
- **Rearrange** by dragging a slot by its title bar onto another slot (they
  swap). The right-click menu has copy-to-slot, lengths, reset and more.
- **Undo / redo** every edit (also Ctrl/Cmd+Z, Ctrl/Cmd+Shift+Z).
- **Hear it**: click a slot's waveform to play just that slot; PLAY loops the
  whole riff; **SYNC** plays it along with the DAW's transport, locked to the
  bar (it picks up mid-riff if you start playback in the middle); any MIDI
  note plays it from the top while held.

### Getting the riff into your song

- **RENDER** writes one seamless loop, e.g. `Riff_0007_145BPM.wav`, into your
  *My Riffs* folder at the current tempo. Nothing changed since the last
  render? Foundry reuses that file instead of making a duplicate.
- **DRAG TO DAW**: drag the handle out of the plugin window and onto the
  playlist. Hover just outside the plugin for a moment and the DAW takes over
  the drag. The riff is rendered (and saved in *My Riffs*) as it leaves.
- Every render also saves an editable `.foundryriff` file next to the WAV. In
  the browser, **double-click a riff** (or right-click → *Open in Riff Maker*)
  to bring the exact slots back and keep working on it. Samples that were
  moved are found again by filename.
- The riff you're working on is saved with your DAW project.

## The sample browser

A game-engine-style asset dock along the bottom: square tiles with a waveform
thumbnail and the sample's **name under every tile**, plus a **LIST** view
when you'd rather see more names at once.

- Curated folders at the top, mapped from Settings: **Favourites**,
  **Drums** (Kicks, Snares, Claps, Hats, Rides), **Bass**, **My Riffs** and
  **Generated**. Inside those you browse your real sub-folders, with a
  clickable breadcrumb and an up button.
- **Click** a sample to hear it. **Drag** it onto a slot. **Double-click** to
  drop it into the next empty slot. Right-click for favourites, *Load into
  slot*, *Send to Splicer*, *Lock in Drum Generator* and *Show in Explorer*.
- Drag a tile **out of the plugin** to drop the file into the DAW.
- You can also drag audio files from Explorer/Finder straight onto a slot.
  Drop several and they fill that slot and the ones after it.
- **Search** is instant and never touches the disk: it queries the library
  index, so a huge sample collection doesn't stall the DAW. It searches the
  open folder and everything below it (or the whole library from the top).
- Thumbnails are drawn in the background and cached in the library database,
  so a folder you've opened once draws instantly next time.
- WAV, AIFF, FLAC and Ogg files are indexed. Renders are always 24-bit WAV.
- New files show up after **RESCAN** (or *Refresh Library* in Settings).

## The Generator tab

The original Foundry tools, unchanged in what they do:

- **Drum Loops**: a 4-row step pattern (kick, clap, hat, ride) rendered into
  1–50 loops using random or locked samples, as full loops and/or stems.
- **Bass Riffs**: sequences 2–4 of your bass one-shots into rhythmic phrases.
- **Splicer**: slices 2–4 bass loops and recombines them into new variations.

When a batch finishes, the browser opens its output folder so the new loops
are right there to preview, drag into the DAW, or drop into a Riff Maker slot.

## Clone and build (Windows)

You need **CMake 3.22+**, **Visual Studio 2022** (Desktop C++ workload), and
an internet connection on the first configure so JUCE can be downloaded.

```bat
git clone https://github.com/iwolski99/VST3-Plugin-Template.git Foundry
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
rescan plugins in FL Studio. Load Foundry on an **instrument / generator**
channel, not an effect slot.

### Manual Windows build

```bat
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Foundry_VST3
```

## macOS / Linux

```bash
git clone https://github.com/iwolski99/VST3-Plugin-Template.git Foundry
cd Foundry
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target Foundry_VST3 -j
```

Linux packages:

```bash
sudo apt install libx11-dev libxext-dev libxrandr-dev libxinerama-dev \
                 libxcursor-dev libfreetype6-dev libfontconfig1-dev libasound2-dev
```

A **Standalone** target (`Foundry_Standalone`) builds alongside the VST3. In
the standalone app, type the tempo into the BPM box on the Riff Maker; inside
a DAW the tempo always comes from the host.

| OS | Installed VST3 path |
|---|---|
| Windows | `C:\Program Files\Common Files\VST3\` |
| macOS | `~/Library/Audio/Plug-Ins/VST3/` |
| Linux | `~/.vst3/` |

## First launch

1. Open **Settings** and choose a root library folder.
2. Click **Create Recommended Structure**, or map your existing Kick / Snare /
   Clap / Hat / Ride / Bass folders. *My Riffs* defaults to `<root>/Riffs`.
3. Put samples in those folders and press **RESCAN** in the browser.
4. Open **Bass** (or any folder), drag samples into the slots, and press PLAY.

## Tests

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DFOUNDRY_BUILD_TESTS=ON
cmake --build build --config Release --target FoundryEngineTest FoundryPlaybackTest FoundryLifecycleTest
```

- `FoundryEngineTest`: offline generation, database, and the Riff Maker
  engine: grid timing, slice lengths, start offsets, ring-through and cut-off,
  seamless loop renders, browser folders / search / favourites, thumbnails.
- `FoundryPlaybackTest`: drives the processor the way a host does: preview,
  MIDI, host transport (including starting mid-riff), sample-rate changes,
  undo/redo, dice, render and reopen, session save/restore.
- `FoundryLifecycleTest`: repeated construct / run / destroy cycles.
- `FoundryScreenshot <dir>`: renders the editor to PNGs with a demo library,
  for checking the layout (run under `xvfb-run` on a headless Linux box).

Add `-DFOUNDRY_SANITIZE=ON` to build the tests with AddressSanitizer and UBSan.

## Format

64-bit VST3 instrument (`Instrument|Sampler`), MIDI in, stereo out, no audio
input. All rendering happens off the audio thread; the riff player never
locks, allocates or frees memory while it plays, and slot samples are
re-prepared if the host changes sample rate.
