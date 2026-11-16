"""Drawn LFO waveforms (headless, Pedalboard): the Custom shape follows 32 drawn points on a pitch route, runs at the LFO rate, leaves the old shapes alone, and old projects load unchanged."""
import sys, subprocess, tempfile, numpy as np
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]; sys.path.insert(0, str(ROOT / 'tools'))
from pedalboard import load_plugin
from state_tools import edit_state
PATH = str(ROOT / 'build/Relic_artefacts/Release/VST3/Relic.vst3'); SR = 44100; rows = []
def check(name, ok, note=''): rows.append(bool(ok)); print(f"  [{'PASS' if ok else 'FAIL'}] {name} {note}", flush=True)
with tempfile.TemporaryDirectory() as tmp:
    b = str(Path(tmp) / 'lc'); subprocess.run(['clang++', '-std=c++17', '-O1', str(ROOT / 'tools/dsp_lfo_custom_test.cpp'), '-o', b], check=True)
    r = subprocess.run([b], capture_output=True, text=True); print(r.stdout, end=''); check('DSP harness (curve maths)', r.returncode == 0)
BASE = dict(waveform=0, osc_level=0.4, unison_voices=1, synth_mix=1, filter_on=0, master=1.0, attack_ms=1, decay_ms=0, sustain=1, release_ms=60, voices=4, chord_on=0, arp_on=0, chop_on=0,
            osc2_on=0, osc3_on=0, vib_on=0, lfo1_rate_hz=2.0, lfo1_sync=0, mod1_src=1, mod1_dst=3, mod1_amt=0.1)
def table(f): return {f'lfo1_w{k + 1}': float(np.clip(f(k / 32.0), 0, 1)) for k in range(32)}
def render(params=None, drop=None, dur=2.6):
    P = load_plugin(PATH); P.raw_state = edit_state(P.raw_state, {**BASE, **(params or {})}, None, None, drop); P.reset()
    return P([(bytes([0x90, 69, 110]), 0.0), (bytes([0x80, 69, 0]), dur - 0.1)], duration=dur, sample_rate=SR, num_channels=2).mean(axis=0)
def dev(x, f0=440.0):
    n = len(x); X = np.fft.fft(x); h = np.zeros(n); h[0] = 1; h[1:n // 2] = 2; h[n // 2] = 1; z = np.fft.ifft(X * h)
    f = np.diff(np.unwrap(np.angle(z))) * SR / (2 * np.pi); k = int(0.01 * SR); f = np.convolve(f, np.ones(k) / k, 'same'); return 1200 * np.log2(np.maximum(f, 1) / f0)
def at(d, t): return float(d[int(t * SR)])
CUSTOM = 5
print('== SHAPE ==')
sine = dev(render({'lfo1_shape': 0})); cust = dev(render({'lfo1_shape': CUSTOM}))
check('default drawn points are a sine: Custom follows the Sine shape (within 8 ct of 120)', np.max(np.abs(sine[2000:-4000] - cust[2000:-4000])) < 8, f'({np.max(np.abs(sine[2000:-4000] - cust[2000:-4000])):.1f} ct)')
sq = dev(render({'lfo1_shape': CUSTOM, **table(lambda x: 1.0 if x < 0.5 else 0.0)}))
hi, lo = np.mean([at(sq, 0.12), at(sq, 0.62)]), np.mean([at(sq, 0.37), at(sq, 0.87)])
check('a drawn square: first half of each 0.5 s cycle up, second half down (about +120 / -120 ct)', hi > 90 and lo < -90 and 150 < hi - lo < 280, f'({hi:.0f} / {lo:.0f} ct)')
saw = dev(render({'lfo1_shape': CUSTOM, **table(lambda x: x)}))
v = [at(saw, t) for t in (0.1, 0.2, 0.3, 0.4)]
check('a drawn rising ramp: pitch climbs steadily through the cycle', v[0] < v[1] < v[2] < v[3] and v[3] - v[0] > 80, f'({", ".join(f"{x:.0f}" for x in v)} ct)')
flat = dev(render({'lfo1_shape': CUSTOM, **table(lambda x: 0.5)})); check('a flat line at the middle gives no modulation', np.ptp(flat[3000:-4000]) < 6, f'({np.ptp(flat[3000:-4000]):.1f} ct)')
up = dev(render({'lfo1_shape': CUSTOM, **table(lambda x: 1.0)})); check('a flat line at the top holds the pitch up (+120 ct)', 100 < np.mean(up[3000:-4000]) < 140, f'({np.mean(up[3000:-4000]):.0f} ct)')
print('== RATE ==')
sq4 = dev(render({'lfo1_shape': CUSTOM, 'lfo1_rate_hz': 4.0, **table(lambda x: 1.0 if x < 0.5 else 0.0)})); s = sq4[3000:int(2.2 * SR)] > 0
edges = np.flatnonzero(np.diff(s.astype(int)) == 1); per = np.median(np.diff(edges)) / SR if len(edges) > 2 else 0
check('the drawn cycle repeats at the LFO rate (4 Hz -> 0.25 s)', abs(per - 0.25) < 0.02, f'({per:.3f} s)')
print('== OTHER SHAPES AND OLD PROJECTS ==')
a = render({'lfo1_shape': 3}); b = render({'lfo1_shape': 3, **table(lambda x: x)}); check('the old shapes ignore the drawn points (bit-identical)', np.array_equal(a, b))
Pn = load_plugin(PATH); Pn.raw_state = edit_state(Pn.raw_state, {'lfo1_shape': CUSTOM, **table(lambda x: x)}); Pm = load_plugin(PATH); Pm.raw_state = Pn.raw_state
check('drawn points and Custom survive a save and restore', all(abs(getattr(Pm, f'lfo_1_draw_{k + 1}') - k / 32.0) < 0.01 for k in (0, 8, 16, 31)) and Pm.lfo_1_shape == 'Custom')
a = render({'lfo1_shape': 0}); b = render({'lfo1_shape': 0}, drop=[f'lfo1_w{k + 1}' for k in range(32)] + [f'lfo2_w{k + 1}' for k in range(32)]); check('a project without the drawn points renders the same as before', np.array_equal(a, b))
a = render({'lfo1_shape': CUSTOM}); b = render({'lfo1_shape': CUSTOM}, drop=[f'lfo1_w{k + 1}' for k in range(32)]); check('missing points default to a sine (Custom still works)', np.array_equal(a, b))
print(f'{sum(rows)}/{len(rows)} passed'); sys.exit(0 if all(rows) else 1)
