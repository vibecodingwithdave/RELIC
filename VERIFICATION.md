# Verification: Relic milestone 1

Run with `tools/verify.py` (headless, Pedalboard). Result: **52 of 52 checks pass.**

## Bugs found by verification
- **Ping-pong delay decayed every two echoes** (levels -6, -12, -12, -18) because feedback was applied on only one leg of the loop. Fixed by applying feedback on both legs; levels are now -6, -12, -18, -24.

## Results against the spec

| Area | Result |
|---|---|
| Pitch | within 0.02 cents (saw, square, triangle at C2 and C4) |
| Saw | harmonics 2 to 5 within 0.4 dB of 1/k |
| Square | odd harmonics within 0.4 dB of 1/k; even harmonic at -71 dB |
| Triangle | odd harmonics within 0.5 dB of 1/k^2; even harmonic at -73 dB |
| Relative level vs sine (rms) | saw -3.3 dB, square 0.0, triangle -2.2 (reference -3.2, -0.1, -2.2) |
| Aliasing (saw, C6 and C7) | -159 dB and -135 dB re fundamental (spec: below -70; reference -81 to -93) |
| Filter LP, reso 0 | DC gain 0 dB; -3 dB point exactly at the cutoff (250, 1000, 4000 Hz); slope -12.0 to -12.6 dB/oct |
| Filter resonance peaks | +7.0 / +10.4 / +13.7 / +15.3 dB at reso 0.25 / 0.5 / 0.8 / 1.0 (reference +6.4 / +10.5 / +13.7 / +15.4) |
| HP, BP, Notch | same shape as the reference table at 250 to 4000 Hz |
| Tremolo | rate within 0.05 Hz; depth 0.5 gives min/max 0.48; depth 1.0 reaches 0 |
| Ping-pong delay | echo spacing 0.50 s; -6.03 dB per echo; alternates left and right |
| Compressor | worst error vs the formula 0.02 dB (three settings, four levels each) |
| Bitcrusher | 4-bit and 8-bit SNR within 0.5 dB of ideal; downsample-by-4 image at the predicted frequency |
| Auto-panner | rate within 0.01 Hz; depth 1.0 sweeps the full range |

## Differences from the reference (deliberate or unexplained)
- The reference's cutoff lands about 12% low at 8 kHz; Relic hits the set cutoff exactly (it pre-warps the filter).
- Relic aliases less than the reference at high notes.
- The compressor's detector time constants, the delay's wet/dry law and all level calibrations other than the oscillator shapes were chosen, not measured.

## Not yet verified
AU and Standalone builds compile but were not exercised; no listening test in a DAW; CPU use not measured.

## Sampler (`tools/verify_sampler.py`): 20 of 20 checks pass
- Pitch within 0.005 cents for notes at, above and below the root; root and octave controls shift correctly; a 44.1 kHz file at a 48 kHz host keeps its pitch.
- Gain exact (0.5 gives half); normalize raises a quiet sample to full scale.
- SAMPLE / Synth mix: 0 is sample only, 1 is synth only, 0.5 has both.
- Forward and reverse playback run the right way, start point lands on target, loop on keeps sounding, loop off ends the voice, and a loop region stays inside its range.
- A fresh plugin instance restores the sample from the saved state.

Not verified: very long samples, files that move after saving.

## Presets (`tools/verify_presets.py`)
- 484 preset files (16 factory, 468 imported from a user-supplied library) hold 17,807 parameter values; every id exists in the plugin.
- 56 presets rendered (all 16 factory, 40 imported at random): no non-finite output, none silent; peaks stay below full scale with the output safety stage (it is transparent below 0.9).

Not verified: how closely an imported preset sounds like its original (reverb, chorus, flanger, textures and LFOs are dropped); presets whose samples have moved.

## Chords and arpeggiator (tools/verify_arp_chords.py, 42 checks)
Chord engine: triads, 7th, 9th, add9, sus2, sus4, power; scales (major, minor, dorian); inversions 1 and 2; close, open, drop 2 and spread voicings; scale lock on and off; two keys at once; strum spacing (about 100 ms).
Arpeggiator at 120 BPM: up, down, up-down, down-up, 2 octaves, played order, chord mode, random (stays inside the held notes); rates 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32 land within 4 ms of exact; gate 50% and 25%; swing delays odd steps by 125 ms at 0.5; release stops within 20 ms; hold keeps playing and a new key replaces the held note.
All 42 pass. Host tempo is read from the DAW when it provides one; the headless host does not, so the tests run at the 120 BPM fallback.

## Chorus, flanger, reverb (tools/verify_modfx.py, 22 DSP + 10 plugin checks)
DSP harness (tools/dsp_modfx_test.cpp, no JUCE): flanger notch at 1 kHz for a 0.5 ms delay, +13.9 dB at 2 kHz with feedback 0.8 (theory 14.0), -5.1 dB with feedback -0.8 (theory -5.1), dry passes untouched at mix 0; chorus taps at 6.8, 12 and 17.2 ms, a single 12 ms tap at depth 0; reverb RT60 measured by backward integration at sizes 0.2, 0.5, 0.8: 0.55, 1.35, 3.29 s against 0.55, 1.34, 3.30 designed, damping shortens the decay, pre-delay holds the first echo back, left and right decorrelated, stable at the longest setting.
In the plugin: reverb rings on after the note ends and dies away, bigger size rings longer, effects off are bit-identical to dry, everything on stays under full scale.

## Modulation (tools/verify_mod.py, 19 checks)
Vibrato at 50 cents measures 49.2 cents peak at 5.00 Hz; LFO to pitch at 5% gives plus or minus 59.4 cents (60 expected); sync 1/4 at 120 BPM runs at 1.88 Hz (2 expected, measured over 2.4 s); pitch bend full up is +200.0 cents; portamento starts near the old note, glides through the middle, lands within 3 Hz of the new one, and off means an instant jump. Matrix: LFO to cutoff sweeps the brightness 64x, negative amounts invert it (correlation -0.92), velocity and mod wheel open the filter, LFO square to level alternates silent and double, LFO to pulse width creates even harmonics, LFO to resonance, random LFO stays inside its range, two slots on one target add, and everything at once stays finite and under full scale.
Also fixed: the factory preset installer never replaced older presets; its version stamp now follows the bundled files, so new presets reach your library on the next load.

## EQ, tape, widener, wah, lo-fi, OTT, lead mode (tools/verify_fx2.py, 38 DSP + 16 plugin checks)
EQ: flat at 0 dB; shelves and peak hit their set gain within 0.02 dB at the test frequencies (8.98 for 9, 8.00 for 8, 6.00 for 6). Tape: unity at drive 0, odd harmonics only at drive 1 (third 20% of fundamental, second at zero), tone rolls 10 kHz off by -15 dB, wow wanders 21 cents. Widener: exact mono, unity and double-side. Wah: static at depth 0, 11.7x sweep at depth 1, envelope mode opens with playing level. Lo-fi: -6.2 dB at the tone frequency, 60 Hz high-pass, dry path delayed 6 ms to match, silent with noise 0. OTT: flat sum at depth 0, 40 dB squeezed under 22 dB at depth 1, lift capped at +12.5 dB. Lead mode in the plugin: overlap takes over without an envelope dip, releasing the top note returns to the held one, glide slides.
LEAD and TAPE are my own interpretation of two buttons whose behavior I did not measure in the reference; the OTT thresholds are tuned to the measured shape (about +12.5 dB cap, wide range compression), not copied.

## Chop sampler (tools/verify_chop.py, 27 checks)
Synthetic tone-burst samples (written by the script) cover: even slicing of 4, 8 and 16 slices (each pad plays its own slice and stops at its end); onset slicing on irregularly spaced hits (pad N starts on hit N); grid slicing at the 120 BPM fallback (1/4 and 1/8); silent pads past the slice count; notes outside C1 to D#2 and chop off playing the sample pitched; pitch plus and minus 12; reverse (swell, not decay); loop while held; early release; sample gain; overlapping pads; chop with LEAD on. Not verified: onset slicing on real drum loops (only synthetic bursts), grid slicing at a real host tempo, and by ear in Logic.

## Randomizers and undo (checked with a temporary self-test in the standalone, since removed)
21 checks, all passed: CRAFT and CHAOS keep the protected controls (master, voices, mix, chops, chords, arp, lead, glide, vibrato, sample gain); CRAFT leaves the mod matrix and LFOs alone and stays in musical ranges (cutoff 500 Hz to 16 kHz, sustain at least 0.25, resonance at most 0.5); CHAOS does change the mod matrix and never lands cutoff, sustain or level in a silent corner; LOCK FX leaves every effect untouched; undo restores every parameter exactly, redo restores the rolled state, a new edit clears redo, and the history ends cleanly at 100 steps. Not verified: pressing the buttons with the mouse, and by ear.

## Loop crossfade (tools/verify_loopfade.py, 8 checks)
A 440 Hz sine looped over 4.25 cycles clicks with a 0.50 jump between samples; with 5, 10 or 20 ms fades the biggest jump falls to 0.04. A fade longer than half the loop is limited, non-looped playback is unchanged, and looped chop slices lose their click too (0.39 down to 0.03). Not verified: by ear on real material (an equal-power fade can dip slightly on correlated audio), and the + / - keyboard buttons were built but not clicked.

## Oscillators 2 and 3 (tools/verify_osc23.py, 15 checks)
Off by default and silent; octave, semitone and fine tune land on the expected frequencies (110, 220, 329.6, 440, 226.4 Hz); level scales amplitude; square adds odd harmonics and sine adds none; each oscillator works alone and all three together; they follow the keyboard, keep playing with oscillator 1 at zero, and go through the filter. The tab layout was checked in a rendered snapshot. Not verified: by ear, and unison on oscillators 2 and 3 does not exist (by design). CRAFT and CHAOS now also roll these oscillators.

## Modulation envelopes (tools/verify_env.py, 12 checks)
Env 1 to pitch (amount 0.1 = 1.2 semitones): decay falls from about 100 ct through 60 ct to zero, sustain 0.5 holds 60 ct, a 400 ms attack rises slowly, release falls back after the key comes up, and a new note restarts the envelope. Env 2 has its own settings, two envelopes in two slots add, an unrouted envelope does nothing, a negative amount bends down. Env to cutoff is bright then dull (0.24 against 0.02 brightness), a slow attack opens the filter slowly, env to level swells in when negative. The ENV tab layout was checked in a rendered snapshot. Not verified: by ear, and the envelope curves are linear segments like the amp envelope.

## Batch: FX 4, filter models, sources, XY, split (final run, all suites passed)
- FX 3 (tools/verify_fx3.py): 19 DSP + 10 plugin checks. Talk box formants land at the vowel positions, the spectral gate passes a tone and removes the weaker harmonics, pitch delay echoes shift by the set interval (the shifter is granular, so pitch is within a few percent, not exact), tape repeats are darker, wash is stable.
- Filter (tools/verify_filter2.py, 15): SP-12 is 26 dB steeper at 4.4 kHz, OBX adds a 3rd harmonic to a hot sine, MPC60 is 2.6 dB down at 12.5 kHz, all four models stay bounded at high resonance; source routing filters only the chosen layer; key sync starts the second note from the LFO start. The models are my own sketches, not measured copies of any hardware.
- Sources (tools/verify_sources.py, 27): wavetable position sweeps spectra and is band-limited, matrix Pulse Width moves it; pluck tracks pitch (110 to 880 Hz) and decays; bell partials decay at different rates; slot B morph 0, 0.5 and 1; texture off is silent, on follows the key, jitter smears pitch, sparse grains pulse.
- XY, macros, split (tools/verify_batch4.py, 12): the four sources drive the matrix at the expected amounts; split sends low keys to the sample and high keys to the synth. EFFECTS A/B checked with a temporary self-test (7 checks, removed). Not verified by hand: clicking any of the new buttons, the sidebar menus, the XY pad drag, and none of it by ear in Logic.
- Bug found and fixed on the way: restoring saved state forced Synth Mix to the sample side whenever a sample loaded, so a reloaded project lost its mix setting.
- Not built: stem splitter (needs a trained model), eight matrix slots (the panel fits four), mid/side filter modes, bowed or blown physical models.

## UI review and CRAFT rework (tools/verify_craft.py)
- Every tab was rendered at 2x and inspected. Fixed: knob values just under a whole number printed as "99999" (a rounding quirk in the formatter), effect pad labels sat on the bottom border, Master, Glide and filter buttons were clipped by their panel frames, and button or knob text that was too long ran over the edge (all text now shrinks to fit its box). Effect panels now start their knob rows at the same height with roomier spacing.
- CRAFT used to roll random waveforms (including Pluck and Bell), random extra-oscillator intervals and any of the 17 effects at random, and extra oscillators ignored the main level, so layers were far louder than their parent. Now: styled patches, consonant layers only, four safe effects. Oscillators 2 and 3 are now scaled by the main oscillator level. The wavetable bank is levelled to about the same loudness as the saw.
- Checked on 60 rolls: only reverb, delay, chorus and tape ever switch on; layers are always unison, octave or fifth within 12 cents; mod depth at most 0.25; all pads, plucks and other styles appear; the rendered C minor chords never hard-clip (peaks 0.21 to 0.96) and sit within about 12 dB of each other (plucks are the quiet end because they decay); median brightness 1.4 kHz, none over 3.5 kHz. This is a measurement of safety and level, not of taste: whether the results sound good is for your ears.

## New factory presets (26, tools/check_new_presets.py and verify_presets.py)
Each new preset was rendered as a C major chord: none is silent, all stay under the limiter (peaks 0.21 to 0.94, rms between about -14 and -29 dBFS depending on how much they decay). The plucked-string model and the talk box needed more output gain to sit with the rest, so they were raised. The preset check now loads a fresh plugin per preset; before, one preset's settings leaked into the next and hid that Chord Stabs, Minor Strum Pad and Cathedral Bells were running into the limiter, so their oscillator level was lowered. The imported presets from your library were not changed, and two of them (hypercharge, DroneAtHome) peak at 0.999. Not verified: by ear.

## Arpeggiator measured against a reference instrument (tools/verify_arp_chords.py, 79 checks)
the reference was run headless as an ordinary licensed plugin: its oscillator set to a pure sine, effects off, held chords sent in, and the output analysed for note times and pitches (note recognition at each onset; templates and per-note energy). Measured and now matched in Relic: the five orders (pitch, as played, converge, diverge, random), the 14 rate divisions, swing scale (delay = swing x half a step), gate (note length = gate x step; 100% ties), HUMAN (timing scatter, level give, ghost notes), the 25 ms wait before the first step, and Waterfall: the 8-step phrase on chords of 2 to 7 notes and over several octaves, the four groupings (each group restarts the phrase and is cut to its length; the step locks to 1/16), and the shapes Spiral, Geyser, Twin, Tide and Climb. the reference's Chance, Spray, Dynamics and Shape Gen showed no effect on notes or timing in these tests, and 2-Hand changes timing in ways I did not decode, so those are not built. Not measured in that batch: GO/STOP, Hold behaviour, the rhythm lane, chord controls (VOICE, ROOT, extensions, FIXED/CUSTOM), grouping applied to the plain Up/Down modes. Note on a change: the arp rate setting is now stored as Arp Division (the old six-value rate setting is gone), and swing is 0 to 1; presets saved before this change lose their arp rate and swing.

## Chords measured against a reference instrument (tools/verify_arp_chords.py, tools/dsp_lead_test.cpp)
the reference's CHORD button is its Chord Lock setting. Measured with the same method as the arp (sine, effects off, spectral analysis of held chords) and matched in Relic: scale chords stack diatonic thirds from the scale and out-of-scale keys snap down to the scale note below (already how Relic worked); 9th, 11th and 13th keep stacking (Relic gains 11th and 13th); inversions lift the lowest 1, 2 or 3 tones an octave (already matched); FIXED mode with the reference's 24 shapes, checked on C4 and F#4 and ignoring the scale; voice leading. Voice leading is my reading of seven measured transitions on two progressions: of all inversions and octave placements, the one with the smallest total move from the previous chord, with the lowest note no more than 3 semitones under the key pressed; the 3-semitone figure sits between the cases measured (excluded at 5 under, allowed at 2 under) so it is approximate. The voice-leading checks run on the chord engine alone, because the test host plays a C4 chord of its own before every call and that pollutes a plugin-level test. Not built: the reference's SCALE-CUSTOM mode (your own chord per degree), custom shapes 1 to 8, the Chord Split setting, chord banks, and its DYNAMIC extension; the harmonic and melodic minor scales exist in Relic but not in the reference.

## Arp GO/STOP (2026-11-14, corrected one-cycle STOP)

The first implementation incorrectly treated STOP as an immediate mute. It is preserved in commit 7c312c6. The owner's reference help text specifies that GO loops while held and STOP plays one cycle per fresh chord, then waits. Relic now follows that behavior while keeping the saved/automatable `arp_run` parameter and older projects' GO default, including restoration over a stopped instance. CRAFT/CHAOS continue to preserve it; ARP bypass leaves ordinary chord playback alone.

### What was measured on a reference instrument

a local probe script loads the ordinary installed plugin, renders once before setting parameters, uses a helper for a sine with effects off, clears all discovery notes with ordinary MIDI, and delays the test gesture to 0.75 s. Each scenario runs inside one render, without parameter changes/discovery between gestures. Gate 0.3, Human/Swing 0, Pitch order, 1/8 at the reference's 120 BPM fallback; three-note chord C4 E4 G4. Only onset times, pitches and summary levels are printed; no reference audio is retained.

| STOP case | Observed steps/pitches, then silence while held |
|---|---|
| Up | 3: C4 E4 G4 |
| Down | 3: G4 E4 C4 |
| Up-Down | 4: C4 E4 G4 E4 |
| Waterfall | 8: C4 E4 G4 C4 E4 C4 G4 E4 |
| Up, two octaves | 6: C4 E4 G4 C5 E5 G5 |
| Waterfall, grouping 5-5-6 | 16, at approximately 1/16 spacing |

Adding C5 at 2.5 s, after the first pass finished, produced no extra steps. Releasing/repressing only G4 while C4/E4 remained held also produced no new pass. Adding C5 at 1.1 s, during the pass, extended that pass to C4 E4 G4 C5 without restarting it. Releasing all three keys at 2.5 s and pressing them again at 3.25 s produced a second C4 E4 G4 pass (six steps total). Tail RMS rounded to zero at seven decimal places for all these completed STOP scenarios. These measurements resolve all three requested questions.

### Implemented behavior and assumptions

STOP starts on a fresh physical gesture, plays the current cycle, then waits even with keys held. Up/Down, Played and Random use the octave-expanded pattern length; Up-Down/Down-Up use max(1, 2L-2); Waterfall uses 8 steps, or one 16-step bar with any grouping; Chord mode uses one simultaneous hit. Active note-set changes update the pass length. Completion stops new steps while preserving scheduled note-offs. The display's (STOP) label appears only after playback and pending note-offs finish, including with gate 100%.

All physical keys must be released to re-arm; adding/repressing one while another remains held cannot do so. With HOLD, physical release leaves the latch and the current pass intact; a fresh gesture replaces it and starts one new pass. Switching STOP to GO restarts held/latched notes at step zero and loops, using the existing host grid or 25 ms collection wait. Switching GO to STOP finishes its current pass (and does not start another if that pass already completed).

Assumptions/unmeasured reference behavior: the reference's arp_mode parameter exposes no Down-Up choice, so its cycle is the symmetric Up-Down cycle specified by the owner. Played/Random cycle lengths, Chord's single hit, all other Waterfall shapes/groupings, and HOLD interactions follow the requested rules and existing Relic latch behavior; only grouping 5-5-6 was measured here. GO-to-STOP finishing the current pass, exact transition timing, and partial key release during a pass were not measured on a reference instrument. STOP-to-GO restart at zero is the owner's explicit target. Editing chord settings while already waiting does not re-arm STOP; that is a design choice consistent with requiring a fresh gesture.

### Verification

- `tools/dsp_arp_run_test.cpp`: 44 direct engine checks pass, run by `verify_arp_chords.py`. Exact sequences/counts cover every mode, octaves, orders and all four groupings; complete note-offs, early release, tied final gates and waiting state; fresh/partial gestures; GO resumption/host sync; HOLD replacement, reset, bypass and the MIDI ceiling.
- `tools/verify_arp_run.py`: 22 checks pass, covering plugin audio for each mode/grouping, octave expansion, live additions, re-arming, continuous GO resumption, bypass, parameter/default and saved-state compatibility. Exact stateful event behavior is established by the engine harness; audio scenarios clear discovery notes and delay the input chord.
- AU, VST3 and Standalone compile. UI appearance/clicking and listening in Logic remain unverified; no snapshot/debug hooks are retained.
- Final full `tools/run_all_tests.sh` run completed with exit status 0: all 16 executed suites passed (87 chord/arp groups/checks, including the 44-check STOP harness; 22 GO/STOP plugin checks). CRAFT skipped for the missing temporary dump. All 99 preset renders were finite and audible, with no unknown parameter IDs. `git diff --check` passed.

## Arp rhythm lane: actual the reference measurements and corrections (2026-11-14)

The initial implementation in fb4c572 followed help text with unmeasured assumptions. Those assumptions are superseded by the measurements below. The owner supplied a saved lane from the reference instrument, which showed how the lane is actually stored (the earlier guesses at its structure did not configure the engine). The owner's sparse export produced three hits in a render whose no-lane control produced 17.

a local probe script --measure` configures synthetic lanes through that observed format, using the ordinary installed reference instrument (v1.5.1), a warm-up render before parameter setup, clean sine/effects off, Up/Pitch order, 1/8 at 120 BPM fallback, Human/Swing 0, MIDI velocity 100 and a 6 ms source release. Discovery notes are cleared; C4 E4 G4 starts at 0.75 s. Only numerical observations are retained. The user's preset and every reference code/audio/sample/wavetable/art asset remain outside this repository.

### Measured and corrected

- **Pitch progression:** H/R alternation yields C, silence, E, silence, G. Ties and leading ties do not consume pitches or retrigger; a leading tie is silent. This confirms the separate pitch cursor.
- **Ordinary ties:** with a 250 ms step and Gate 0.3, H/T/R supports sound for about 325 ms, and H/T/T/R for about 575 ms. With Gate 0.1, H/T/R supports about 275 ms. The final tied cell uses its gate point, rather than its end. The implementation now defers the note-off only through intermediate ties, then schedules the final gate.
- **Velocity:** measured output ratios for stored strengths 0/1/5/6/7/10/25/50/75/100 were approximately .060/.061/.060/.060/.070/.101/.250/.502/.754/1.000. The multiplier is linear with a **6% minimum**. Stored zero is a quiet hit and consumes a pitch; it is not a rest. Tie velocity does not retrigger or alter the pitch sequence. The hit bar/tooltip now reflect the floor.
- **STOP:** H/R still emits all three C E G hits, at approximately .75, 1.25 and 1.75 s, then stops. Up-Down H/R emits four hits; Waterfall H/R eight; grouped Waterfall H/R sixteen. Thus STOP counts emitted hits rather than clock cells. Rests/ties lengthen the pass instead of shortening the pitch sequence.
- **Final STOP ties:** H/T/T emits three hits at .75, 1.5 and 2.25 s, then holds the final tie to about 3.0 s. H/H/H/T ends its final tie at 1.75 s. H/T/R ends its last tie at 2.75 s. This final STOP tail uses whole tied cells, unlike ordinary looping ties. Waterfall likewise carries its eighth H/T/T hit through the final ties to 6.75 s.
- **Re-arm:** releasing the full chord and pressing again restarts C E G and lane phase, with one complete new pass.
- **Waterfall gate:** isolated hits followed by rests revealed a pre-existing mode-specific curve, present with the lane both on and off. Nine gate settings from 0 to 1 fit `stepLength * (0.125 + 5.875 * gate^2)` to roughly 3 ms of measured envelope support (which includes attack/release and envelope-detector uncertainty). Gate .3 gives about 163 ms at a 250 ms step; Gate .5 about 398 ms; Gate 1 about 1.5 s. Long Waterfall gates ring through rests; rests suppress new hits rather than cutting those tails. Relic now applies that measured curve and preserves those pending note-offs. Tied Waterfall hits extend by the tied cells plus the curved gate; final STOP ties end at the cell boundary.

### Remaining limits

Actual all-rest STOP audio is silent; its internal running/waiting flag cannot be inferred from silence. Relic leaves it armed until a hit, release or reset. Human/Swing and live edits were not measured against the reference in this pass. Existing engine safety choices remain: disabling a lane releases owned ties at the block boundary; an already-released note cannot be resurrected by editing a future tie; an all-tie pattern edited during a STOP tail is bounded to one lane length. GO-to-STOP transition details remain an assumption. Relic's existing 25 ms initial chord wait and plain-mode 100% gate overlap were not re-matched here. UI appearance/gestures and listening in Logic remain unverified.

### Current validation status

A fresh full `tools/run_all_tests.sh` run on the final source (after the Waterfall gate/rest correction) completed with exit status 0. All 17 executed suites reported only passing RESULT lines and no FAIL lines (including 87/87 chord/arp checks with the 44-check STOP harness, 22/22 GO/STOP plugin checks and 13/13 rhythm groups with the 44-check rhythm harness). CRAFT skipped for the missing temporary dump. All 99 preset renders were finite and audible, with no unknown parameter IDs. The final source builds AU/VST3/Standalone, and `git diff --check` passes. The local reference probe passed 42/42 measured checks on this source before the final run; it was not re-run after the full suite because no code changed in between. Nothing was checked by ear in Logic, and UI appearance/gestures remain unverified.

## Arp 2-HAND (tools/verify_arp_twohand.py: 8 checks, including the 23-check DSP harness `dsp_arp_twohand_test.cpp`)
the reference's help text says only that 2-HAND "adds a slow low voice". Local probes (a sine, effects off, chords held at 120 BPM) measured it:
- The arp line is unchanged. One extra note sounds every 4 arp steps starting with the first, at 1/4, 1/8 and 1/16 alike (so 0.5 s at 1/16, 1.0 s at 1/8, 2.0 s at 1/4).
- That note is a held chord tone one octave down. Tones are taken lowest to highest and cycle (C E G then C again; C E G B; a single key repeats; two keys alternate), the same in Up, Down, Up-Down, Waterfall and with 2 octaves.
- It stays at a steady level for about 0.95 of the 4 steps and then drops, and sits about 3 dB under the arp note.

Relic matches those numbers: low hit every 4 steps, tones ascending and cycling, hold 0.95 of the period, level x0.72. Checks: exact events for all of the above, a rest in the rhythm lane does not cut the low note, releasing the keys leaves nothing stuck, turning 2-HAND off lets the current low note finish, and an audio render through the VST3 shows C3, E3, G3 in successive periods with 2-HAND on and none with it off while the arp line keeps its level.

Not measured against a reference instrument, so Relic's choices are assumptions: how 2-HAND behaves with the rhythm lane (Relic counts every lane step, including rests and ties, and a rest does not cut the low note), grouping, Human and Swing, GO to STOP, tempo changes mid-note, and the exact hold and level to better than a few percent. Measured at 120 BPM only. Not listened to in Logic.

## Arp Chance and Spray (tools/verify_arp_chance.py: 8 checks, including the 17-check DSP harness `dsp_arp_chance_test.cpp`)
**the reference has nothing to match.** Its `arp_chance` and `arp_spray` parameters exist, but a local probe script found no effect from them: Up, Waterfall (engine and mode), STAB, legacy, shape generator, Spiral, generative, drift, dynamics, sync, groupings and 3 octaves were identical at Chance 0 / Spray 1 to Chance 1 / Spray 0 (the few differences were in contexts that vary run to run anyway, such as Random order). The CUSTOM engine produced no notes without a drawn grid, whose saved format is not known, and the reference ARP tab shows no Chance or Spray control. They look like unused or hidden parameters. The owner chose to define them in Relic.

Relic's definitions (own design, not measured):
- **Chance** (default 1): probability that a step plays. A dropped step is silent, but the pattern position still advances, so the phrase keeps its place and a STOP pass still ends after the same number of steps. Rhythm-lane rests stay silent; Chance only thins the hits.
- **Spray** (default 0): probability that a note that plays jumps one octave up or down (equal odds). At the ends of the MIDI range the jump flips direction instead of leaving 0..127.
- The 2-HAND low voice is neither dropped nor sprayed. Randomness uses the engine's fixed-seed generator, so the same settings give the same events.

Checks: exact events for defaults (plain arp unchanged), Chance 0 (silent, nothing stuck), Chance 0.25/0.5/0.75 (hit counts rise, each played note is the one due on its step), Spray 0.5 and 1 (octave jumps, both directions, range ends), both together, STOP, rhythm lane, 2-HAND; and audio through the VST3 (Chance 0 silent, Chance 0.5 quieter, Spray 1 leaves no energy on C4 E4 G4 and lands on the octaves). Not listened to in Logic. Old saved projects get Chance 1 / Spray 0 (and 2-HAND off) for the missing parameters.

## Arp grouping in the plain modes (tools/verify_arp_group.py: 6 checks, including the 17-check DSP harness `dsp_arp_group_test.cpp`)
Waterfall grouping was measured earlier on a reference instrument (each group restarts the phrase and is cut to its length; the step locks to 1/16). Its behaviour in Up, Down and Up-Down had never been measured. Three attempts this time (local probe scripts: onset times and pitches, a fixed step grid, spectral-rise note events) gave inconsistent results: even the ungrouped Up baseline read wrongly in places, and the plugin went silent after repeated parameter changes in one session. A rough reading of the first attempt hinted that groups in the plain modes may repeat or reverse notes rather than simply restart the pattern, but that could not be confirmed, so **nothing here is matched to the reference**.

Relic's design, by extension of the measured Waterfall rule:
- With a grouping set, the plain modes (Up, Down, Up-Down, Down-Up, Played, Random) cut the line into the group lengths and restart the pattern at the top of every group; Up on C E G with 5-5-6 plays C E G C E, C E G C E, C E G C E G, then repeats the 16-step bar. A group is cut at its length, so with several octaves it never reaches past it.
- A grouped pass is one 16-step bar (STOP plays exactly 16 steps, then waits). The step keeps the chosen rate (Waterfall's lock to 1/16 is unchanged and applies to Waterfall only). Chord mode ignores grouping, and no grouping is the old behaviour.
- Rests in the rhythm lane do not skip notes inside a group.

Checks: exact sequences for Up, Down, Up-Down and Down-Up in all four groupings, 2 octaves, the bar repeating, rate unchanged, STOP, rests, Waterfall and Chord unchanged; and the grouped sequence heard through the VST3 (Up and Down 4-4-4-4, Up 5-5-6) against plain Up. Not listened to in Logic.

