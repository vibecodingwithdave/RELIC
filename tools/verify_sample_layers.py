"""Sampler layers (headless, Pedalboard, synthetic test audio): sample End and tuning for slot A, plus (as it grows) independent slot B controls and
per-slot envelopes. Checks playback boundaries, tuning, saved-state round trips and that projects saved before these controls existed still work."""
import sys, os, numpy as np
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]; sys.path.insert(0, str(ROOT / 'tools'))
from pedalboard import load_plugin
from state_tools import edit_state
PATH = str(ROOT / 'build/Relic_artefacts/Release/VST3/Relic.vst3'); ASSETS = str(ROOT / 'tools/test_assets'); SR = 44100
SINE = ASSETS + '/sine440.wav'; SINE2 = ASSETS + '/sine660.wav'; RAMP = ASSETS + '/ramp.wav'
rows = []
def check(name, ok, note=''): rows.append(bool(ok)); print(f"  [{'PASS' if ok else 'FAIL'}] {name} {note}", flush=True)
BASE = dict(synth_mix=0.0, sample_gain=1.0, sample_root=69, sample_octave=0, sample_loop=0, sample_reverse=0, sample_normalize=0, sample_start=0.0, loop_start=0.0, loop_end=1.0,
            loop_xfade=0, sample_morph=0.0, attack_ms=0.5, decay_ms=0.0, sustain=1.0, release_ms=30.0, filter_on=0, master=1.0, voices=4, unison_voices=1, chop_on=0, tex_level=0.0)
def plugin(params=None, wav=SINE, attrs=None, drop=None):
    P = load_plugin(PATH); P.raw_state = edit_state(P.raw_state, {**BASE, **(params or {})}, wav, attrs, drop); P.reset(); return P
def render(P, hold=2.4, note=69, dur=2.8):
    return P([(bytes([0x90, note, 127]), 0.0), (bytes([0x80, note, 0]), hold)], duration=dur, sample_rate=SR, num_channels=2).mean(axis=0)
def rms(x, a, b): s = x[int(a * SR):int(b * SR)]; return float(np.sqrt(np.mean(s ** 2)))
def freq(x, a=0.3, b=1.3):
    s = x[int(a * SR):int(b * SR)] * np.hanning(int((b - a) * SR)); X = np.abs(np.fft.rfft(s, 1 << 19)); return float(np.fft.rfftfreq(1 << 19, 1 / SR)[np.argmax(X)])
SILENT = 1e-3; LOUD = 0.05

# ---- defaults and saved state
P0 = load_plugin(PATH)
check('Sample End defaults to 1 (full length) and Sample Tune to 0', abs(P0.sample_end - 1.0) < 1e-6 and abs(P0.sample_tune_ct) < 1e-6)
P1 = plugin(dict(sample_end=0.5, sample_tune_ct=37.0)); st = P1.raw_state
P2 = load_plugin(PATH); P2.raw_state = st
check('End and Tune survive a save and restore into a new instance', abs(P2.sample_end - 0.5) < 1e-4 and abs(P2.sample_tune_ct - 37.0) < 0.1, f'(end {P2.sample_end:.3f}, tune {P2.sample_tune_ct:.1f})')
Pold = plugin({}, drop=['sample_end', 'sample_tune_ct'])
check('A project saved before these controls existed loads with End = 1 and Tune = 0', abs(Pold.sample_end - 1.0) < 1e-6 and abs(Pold.sample_tune_ct) < 1e-6)
xa = render(plugin({})); xb = render(Pold)
check('and renders identically to the defaults', np.allclose(xa, xb, atol=1e-6))

# ---- sample end
x = render(plugin(dict(sample_end=0.5)))     # the file is 2 s long, so End 0.5 stops at 1 s
check('End 0.5: the sample plays until about 1 s, then stops', rms(x, 0.2, 0.9) > LOUD and rms(x, 1.15, 2.2) < SILENT, f'(before {rms(x,0.2,0.9):.3f}, after {rms(x,1.15,2.2):.5f})')
x = render(plugin(dict(sample_start=0.25, sample_end=0.5)))
check('Start 0.25 / End 0.5: plays 0.5 s of the file (until about 0.5 s) then stops', rms(x, 0.1, 0.4) > LOUD and rms(x, 0.65, 2.2) < SILENT)
x = render(plugin({}))
check('End at its default: the whole 2 s file plays', rms(x, 1.5, 1.9) > LOUD and rms(x, 2.15, 2.35) < SILENT)
x = render(plugin(dict(sample_end=0.5, sample_loop=1, loop_start=0.0, loop_end=1.0)))
check('a loop stays inside the End point (keeps sounding well past 1 s)', rms(x, 1.3, 2.2) > LOUD)
x = render(plugin(dict(sample_end=0.1, sample_loop=1, loop_start=0.5, loop_end=1.0)))
check('a loop that starts after End falls back to looping the played region (still sounds, no crash)', np.isfinite(x).all() and rms(x, 0.5, 2.2) > LOUD)
# reverse
x = render(plugin(dict(sample_reverse=1, sample_end=0.5)))
check('reverse with End 0.5 plays from the End back to the start (about 1 s)', rms(x, 0.1, 0.9) > LOUD and rms(x, 1.15, 2.2) < SILENT)
x = render(plugin(dict(sample_reverse=1, sample_start=0.25, sample_end=0.75)))
check('reverse with Start and End set stops at Start (about 1 s of the 2 s file)', rms(x, 0.1, 0.9) > LOUD and rms(x, 1.15, 2.2) < SILENT)
x = render(plugin(dict(sample_reverse=1, sample_start=0.3)))
check('reverse with Start but End at its default behaves as before (Start ignored, plays back to the beginning)', rms(x, 1.6, 1.9) > LOUD)

# ---- tuning
f0, fp, fm = freq(render(plugin({}))), freq(render(plugin(dict(sample_tune_ct=100)))), freq(render(plugin(dict(sample_tune_ct=-100))))
check('Tune 0 leaves the pitch alone (440 Hz)', abs(f0 - 440.0) < 0.6, f'({f0:.2f} Hz)')
check('Tune +100 cents raises it by a semitone (466.2 Hz)', abs(fp - 440 * 2 ** (100 / 1200)) < 0.7, f'({fp:.2f} Hz)')
check('Tune -100 cents lowers it by a semitone (415.3 Hz)', abs(fm - 440 * 2 ** (-100 / 1200)) < 0.7, f'({fm:.2f} Hz)')
f50 = freq(render(plugin(dict(sample_tune_ct=50)))); check('Tune +50 cents sits halfway (452.9 Hz)', abs(f50 - 440 * 2 ** (50 / 1200)) < 0.7, f'({f50:.2f} Hz)')

print(f"\nRESULT: {sum(rows)} of {len(rows)} sample-layer checks passed")
sys.exit(0 if all(rows) else 1)
