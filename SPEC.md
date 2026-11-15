# Relic: clean-room spec (milestone 1)

An original polyphonic synth plugin, specified from **measured behavior** of a reference instrument. No code, samples, wavetables or artwork from that instrument are used. Each requirement below is a behavior with a number that a test can check.

## Oscillator
- Shapes: sine, saw, square (variable pulse width 0.05 to 0.95), triangle.
- Harmonic content matches the mathematical series: saw 1/k (all harmonics), square odd harmonics at 1/k, triangle odd harmonics at 1/k^2.
- **Band-limited:** aliasing fold-back at least 70 dB below the fundamental at C6 and C7 (the reference measures -81 to -93 dB).
- Pitch accurate to within 1 cent.
- Relative level between shapes (reference, peak relative to sine): saw 0.85, square 0.71, triangle 0.95.
- Unison: 1 to 7 voices, detune 0 to 50 cents, stereo spread 0 to 1.

## Filter (one 2-pole multimode filter per voice)
- Types: low-pass, high-pass, band-pass, notch. All 12 dB/octave (band-pass 6 dB/octave per side).
- Low-pass at reso 0: unity gain at DC, -3 dB at the cutoff, slope about 12 dB/octave.
- Resonance: Q = 0.707 + 5.2 x reso. Reference peaks: +6.4 dB at 0.25, +10.5 at 0.5, +13.7 at 0.8, +15.4 at 1.0. No gain compensation (DC gain stays 0 dB).
- Cutoff 20 Hz to 20 kHz, set in Hz.

## Envelope
- Amplitude ADSR: attack, decay, release 0 to 5000 ms; sustain 0 to 1.

## Effects (chain order: crusher, panner, tremolo, compressor, delay)
- **Tremolo:** gain = 1 - depth x (0.5 + 0.5 x sin). Rate exact (0.1 to 20 Hz). Depth 1.0 reaches silence.
- **Delay:** time 1 to 2000 ms, feedback 0 to 0.95. Ping-pong mode: each echo is feedback x the previous one (-6.02 dB at 0.5), alternating left and right.
- **Compressor:** hard threshold; gain reduction = (1 - 1/ratio) x (level - threshold); detector reads RMS + 2 dB; attack and release in ms; makeup gain exact.
- **Bitcrusher:** true uniform quantization at the stated bits; sample-and-hold downsampling with no anti-alias filter.
- **Auto-panner:** rate in Hz exact; depth 1.0 sweeps hard left to hard right; sine, triangle, square shapes.

## Not in milestone 1
Sampler, chopper, stem splitter, reverb, chorus, flanger, OTT, lo-fi, wah, talk box, arpeggiator, chord engine, modulation matrix, preset browser, custom interface. Presets: an importer for the user's licensed presets comes after the engine matches.

## Verification
Render with Pedalboard (headless) and compare with the reference numbers above using `tools/` scripts. Differences found are listed in `VERIFICATION.md`.
