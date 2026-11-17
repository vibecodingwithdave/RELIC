# Installing Relic

Relic is a free polyphonic synthesizer. It comes as a plugin (VST3, and AU on Mac) for your music software, and as a standalone app you can open on its own.

> **Status:** release downloads are not published yet, so the Windows and Mac steps below describe the planned downloads. Until they exist, see "Building it yourself" at the bottom of the page.

## What you get

- **The plugin** for your music software (VST3 on Windows and Mac, AU on Mac for Logic and GarageBand).
- **The standalone app**, which makes sound by itself with a computer keyboard or a MIDI keyboard.
- **The factory presets.** They are built into Relic and appear in the preset browser the first time it starts. You do not install them separately.

Relic does not ship with sample sounds. The sampler and the sample-based layers play audio files you load yourself. You can use your own recordings, or any sounds you are licensed to use.

## Windows

1. Once a release exists, download `Relic-Setup-<version>.exe` from the Releases page: https://github.com/vibecodingwithdave/RELIC/releases
2. Run it. It asks for administrator permission because the plugin goes into the shared VST3 folder.
3. Choose what to install: the plugin, the standalone app, or both.
4. Open your music software and rescan plugins (in Ardour: Window > Plugin Manager > Scan; other programs have a similar option).
5. Or start **Relic** from the Start menu for the standalone app.

Windows may show a "Windows protected your PC" screen because the installer is not code-signed yet. Choose **More info**, then **Run anyway**, only if you downloaded it from the link above.

To uninstall, use Windows Settings > Apps. Your presets and settings stay in your user folder.

## Mac

There is no Mac download yet; for now build it yourself (see the bottom of this page). When a download exists, these are the places the files go:

1. Put the files in these folders:
   - VST3: `/Library/Audio/Plug-Ins/VST3/`
   - AU (Logic, GarageBand): `/Library/Audio/Plug-Ins/Components/`
   - Standalone app: `/Applications/`
2. Restart your music software and rescan plugins. In Logic, Relic appears under Instruments > AU Instruments.
3. If macOS says the app "cannot be opened because the developer cannot be verified", right-click it, choose **Open**, then **Open** again.

## Your first sound

1. Open Relic (standalone, or load it on an instrument track).
2. Pick a preset in the browser. Try the categories along the top: Pads, Keys, Leads, Bass.
3. Play with a MIDI keyboard or your computer keyboard.
4. Move a knob to hear what it does. Resting the mouse on a control for a moment shows a short tip.
5. CRAFT builds a new sound for a style you choose; CHAOS rolls something wilder; UNDO steps back.

## Where things are saved

| What | Windows | Mac |
|---|---|---|
| Your presets | `%APPDATA%\Relic\Presets` | `~/Library/Relic/Presets` |

Factory presets are copied into a `Factory` folder there on first run. Save your own with SAVE AS.

## Troubleshooting

- **The plugin does not show up.** Rescan plugins; on Mac make sure the file is in one of the folders above; restart the music software.
- **No sound in the standalone app.** Check the audio output device in the app's settings and that your keyboard or MIDI device is selected.
- **Logic rejects the AU.** Run a rescan from Logic's Plug-in Manager and check it is not marked as failed.
- **Something else.** Open an issue: https://github.com/vibecodingwithdave/RELIC/issues

## Building it yourself

If no download is available for your system yet, you can build Relic. You need CMake and a C++ compiler (Xcode command line tools on Mac, Visual Studio on Windows).

```bash
git clone --recurse-submodules https://github.com/vibecodingwithdave/RELIC.git
cd RELIC
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target Relic_VST3 Relic_Standalone -j 8
```

On Mac add `Relic_AU` for Logic. The results are in `build/Relic_artefacts/Release/`. Copy the plugin into the folders listed above.

## License

Relic is free software under the GNU Affero General Public License, version 3. The source is at https://github.com/vibecodingwithdave/RELIC.
