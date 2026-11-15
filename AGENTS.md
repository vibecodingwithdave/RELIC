# Relic: working notes for a coding agent

Relic is a synth plugin (AU, VST3, Standalone) built with JUCE 8.0.4 and CMake. Author brand: "McBABE! Labs". Licensed under the GNU AGPLv3 (see LICENSE). Everything is committed in this repo (`git log` shows the history). Read `README.md` (features), `VERIFICATION.md` (what was measured and what was not) and `SPEC.md`.

## Ground rules (set by the owner)
- Never copy code, samples, wavetables or artwork from any other product. Relic's code is original. Matching another instrument's behaviour means measuring it and writing my own implementation.
- Imported presets are files the user supplies at run time in `~/Library/Relic/Presets/Imported`; none ship with this repository.
- Say plainly what was verified and what was not. Nothing here has been checked by ear in Logic.

## Build, install, validate
```sh
cmake -S . -B build && cmake --build build --config Release -j8     # re-run the configure step after adding preset files (the preset list is globbed at configure time)
A=build/Relic_artefacts/Release
rm -rf ~/Library/Audio/Plug-Ins/Components/Relic.component ~/Applications/Relic.app
cp -R $A/AU/Relic.component ~/Library/Audio/Plug-Ins/Components/ && cp -R $A/Standalone/Relic.app ~/Applications/
codesign --force --deep -s - ~/Library/Audio/Plug-Ins/Components/Relic.component
auval -v aumu Rfs1 Rfgd                                              # must end with AU VALIDATION SUCCEEDED
```
Factory presets: edit `tools/make_factory_presets.py`, run it, then re-run the CMake configure step and rebuild. The installer stamps by content hash, so changed presets reach `~/Library/Relic/Presets/Factory` on the next plugin load.

## Tests (headless: Pedalboard loads build/.../VST3/Relic.vst3)
```sh
tools/setup_env.sh          # once: creates .venv with pedalboard, numpy, scipy
tools/run_all_tests.sh      # every suite; all were passing at the last commit
```
Each `tools/verify_*.py` prints PASS/FAIL lines and a RESULT line. `dsp_*_test.cpp` are standalone harnesses (no JUCE) compiled by their Python wrappers with `clang++ -std=c++17`.

Gotchas that cost time before:
- Pedalboard derives parameter names from display names, lowercased with underscores ("Mod 1 Source" becomes `mod_1_source`, "Arp Division" becomes `arp_division`). Setting an unknown name is silently ignored, so guard with `assert name in P.parameters`.
- State leaks between tests: set every parameter a test depends on, and use a fresh `load_plugin` when it matters.
- The test host plays a C4 chord of its own before each call; do not use plugin-level tests for anything stateful (that is why voice leading is tested on the chord engine, `tools/dsp_lead_test.cpp`).
- macOS: `sed -i ''`, no `timeout` command, `grep -c` exits 1 on zero matches (do not chain it with `&&`).
- UI checks: add a temporary `RELIC_SNAPSHOT` env hook near the end of the `RelicEditor` constructor that renders `createComponentSnapshot` to a PNG and quits, run the standalone with the variable set, view the PNG, then remove the hook (`grep -c RELIC_SNAPSHOT Source/PluginEditor.cpp` must be 0). Delete `~/Library/Application Support/Relic.settings` first or the standalone restores old state. `tools/verify_craft.py` needs a similar temporary hook that dumps 60 CRAFT rolls to JSON; it skips itself when no dump exists.
- Do not leave debug prints or hooks in committed code.

## Where things live
- `Source/PluginProcessor.*`: parameters (`createLayout`), voices, signal chain, CRAFT/CHAOS, undo history, effects A/B.
- `Source/NoteEngine.h`: chord expansion, arpeggiator (orders, Waterfall, grouping, shapes, human), voice leading.
- `Source/Dsp/`: oscillator and wavetable bank, effects (`Effects.h`, `Effects2.h`), modulation LFOs.
- `Source/PluginEditor.*`: the whole UI, drawn at a 1484 x 1060 design size; bitmap font; all text shrinks to fit its box.
- `Assets/`: skin art (the pictures on the left are `Assets/skin/scene_*.jpg`, listed in `kScenes` in `Source/PluginEditor.cpp`), `Assets/presets/` (generated).
- `tools/`: tests and the preset generator.

## Local measurement notes
Behaviour measurements of reference instruments are kept in a `private/` folder next to this repository's files. It is ignored by git and not published.

## What is left
- Arp: nothing planned is missing. GO/STOP, the rhythm lane, 2-HAND, grouping in the plain modes and Relic's own Chance/Spray are implemented (the reference's `arp_chance`/`arp_spray` have no audible effect and no UI control; see VERIFICATION.md); the lane has now been measured against a reference instrument (v1.5.1) and corrected (including ties, velocity floor, STOP hit counts and Waterfall gate curve) (see VERIFICATION.md).
- Chords: SCALE-CUSTOM mode (own chord per degree), 8 custom shapes, Chord Split, chord banks, DYNAMIC extension.
- Sound: physical models beyond the string and bell, a stem splitter (needs a trained model), more matrix slots (the panel fits four), mid/side filter modes.
- Content: a factory sample library, importer mapping for arp, chord and the newer effects.
- Everything above is unheard in Logic: that check belongs to the owner.
