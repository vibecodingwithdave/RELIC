"""Drawn modulation curves (headless, Pedalboard): a slot's response can follow nine drawn points instead of the Curve bend."""
import sys, subprocess, tempfile, numpy as np
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]; sys.path.insert(0, str(ROOT / 'tools'))
from pedalboard import load_plugin
from state_tools import edit_state
PATH = str(ROOT / 'build/Relic_artefacts/Release/VST3/Relic.vst3'); SR = 44100; rows = []
def check(name, ok, note=''): rows.append(bool(ok)); print(f"  [{'PASS' if ok else 'FAIL'}] {name} {note}", flush=True)
with tempfile.TemporaryDirectory() as tmp:
    b = str(Path(tmp) / 'mc'); subprocess.run(['clang++', '-std=c++17', '-O1', str(ROOT / 'tools/dsp_mod_curve_test.cpp'), '-o', b], check=True)
    r = subprocess.run([b], capture_output=True, text=True); print(r.stdout.strip()); check('DSP harness: the curve function is exact', r.returncode == 0)
BASE = dict(waveform=0, synth_mix=1.0, filter_on=0, master=1.0, osc_level=0.3, voices=4, unison_voices=1, attack_ms=1, decay_ms=0, sustain=1.0, release_ms=20, vib_on=0, porta_on=0, lfo1_shape=0, lfo1_rate_hz=4.0, lfo1_sync=0, chop_on=0)
M1_LEVEL = dict(mod1_src=9, mod1_dst=4, mod1_amt=1.0)            # Macro 1 -> Level: the level is 1 + (curve of the macro)
def cv(*pts): return {f'mod1_cv{j + 1}': v for j, v in enumerate(pts)}
def plugin(params=None, drop=None):
    P = load_plugin(PATH); P.raw_state = edit_state(P.raw_state, {**BASE, **(params or {})}, None, None, drop); P.reset(); return P
def render(params=None, drop=None, note=57):
    return plugin(params, drop)([(bytes([0x90, note, 100]), 0.0), (bytes([0x80, note, 0]), 2.2)], duration=2.6, sample_rate=SR, num_channels=2).mean(axis=0)
def rms(x): s = x[int(0.4 * SR):int(1.8 * SR)]; return float(np.sqrt(np.mean(s ** 2)))
zero = rms(render({}))
def level(macro, mode=1, **pts): return rms(render({**M1_LEVEL, 'macro_1': macro, 'mod1_cvmode': mode, **pts})) / zero
NEW = [f'mod{n}_{k}' for n in range(1, 9) for k in ['cvmode'] + [f'cv{j}' for j in range(1, 10)]]
P0 = load_plugin(PATH)
check('defaults: drawn curve off, the points are a straight line', not bool(P0.mod_1_drawn_curve) and all(abs(getattr(P0, f'mod_1_curve_point_{j}') - (j - 1) / 8) < 1e-3 for j in range(1, 10)))
print('== THE RESPONSE FOLLOWS THE POINTS ==')
check('drawn mode with the default points equals the straight line (macro 0.5 -> 1.5x)', abs(level(0.5) - 1.5) < 0.03, f'({level(0.5):.3f})')
flat = cv(*([0.2] * 9)); check('all points at 0.2: the macro at 0.5 gives 1.2x', abs(level(0.5, **flat) - 1.2) < 0.03, f'({level(0.5, **flat):.3f})')
check('and at 0.9 it still gives 1.2x (a flat response)', abs(level(0.9, **flat) - 1.2) < 0.03)
hill = cv(0, 0.1, 0.2, 0.3, 0.8, 0.3, 0.2, 0.1, 0)
check('a hill: the macro at 0.5 (the peak point) gives 1.8x, at 0.25 gives 1.2x', abs(level(0.5, **hill) - 1.8) < 0.05 and abs(level(0.25, **hill) - 1.2) < 0.05, f'({level(0.5, **hill):.3f}, {level(0.25, **hill):.3f})')
check('the macro at 0 gives the first point and at 1 the last (hill: 1.0x and 1.0x)', abs(level(0.0, **hill) - 1.0) < 0.03 and abs(level(1.0, **hill) - 1.0) < 0.03)
check('between two points the curve is smooth (macro 0.5625 lies between the points of 0.5 and 0.625)', 1.3 < level(0.5625, **hill) < 1.8)
print('== MODE SWITCH ==')
a = rms(render({**M1_LEVEL, 'macro_1': 0.5, 'mod1_cvmode': 0, **hill})) ; b = rms(render({**M1_LEVEL, 'macro_1': 0.5, 'mod1_cvmode': 0}))
check('with the drawn curve off the points are ignored', abs(a - b) < 1e-6)
c = rms(render({**M1_LEVEL, 'macro_1': 0.5, 'mod1_cvmode': 0, 'mod1_curve': 1.0})); d = rms(render({**M1_LEVEL, 'macro_1': 0.5, 'mod1_cvmode': 1, 'mod1_curve': 1.0}))
check('with the drawn curve on, the Curve bend is ignored (default points -> 1.5x, not 1.06x)', abs(c / zero - 1.0625) < 0.04 and abs(d / zero - 1.5) < 0.04, f'({c / zero:.2f}, {d / zero:.2f})')
check('without any of the new settings the render is the plain slot', np.array_equal(render({**M1_LEVEL, 'macro_1': 0.5}), render({**M1_LEVEL, 'macro_1': 0.5}, drop=NEW)))
print('== SIGN IS KEPT, OTHER SLOTS ARE SEPARATE ==')
def inst_dev(x, f0=440.0):
    n = len(x); X = np.fft.fft(x); h = np.zeros(n); h[0] = 1; h[1:n // 2] = 2; h[n // 2] = 1; z = np.fft.ifft(X * h)
    f = np.diff(np.unwrap(np.angle(z))) * SR / (2 * np.pi); k = int(0.01 * SR); f = np.convolve(f, np.ones(k) / k, 'same'); return 1200 * np.log2(np.maximum(f, 1) / f0)
LP = dict(mod1_src=1, mod1_dst=3, mod1_amt=0.05)           # LFO 1 -> Pitch: +-60 ct at full swing
ref = inst_dev(render(LP, note=69))[int(0.3 * SR):int(2.0 * SR)]
half = inst_dev(render({**LP, 'mod1_cvmode': 1, **cv(0, *([0.5] * 8))}, note=69))[int(0.3 * SR):int(2.0 * SR)]
check('an LFO through a flat 0.5 curve swings both ways by half as much (sign kept)', 45 < ref.max() < 75 and 22 < half.max() < 38 and -38 < half.min() < -22, f'(plain {ref.min():.0f}..{ref.max():.0f}, drawn {half.min():.0f}..{half.max():.0f} ct)')
two = render({**M1_LEVEL, 'macro_1': 0.5, 'mod1_cvmode': 1, **flat, 'mod2_src': 9, 'mod2_dst': 4, 'mod2_amt': 0.0})
check('slot 2 has its own points and mode (slot 1 drawn does not touch slot 2)', abs(rms(render({'mod2_src': 9, 'mod2_dst': 4, 'mod2_amt': 1.0, 'macro_1': 0.5, 'mod1_cvmode': 1, **flat})) / zero - 1.5) < 0.04)
s8 = rms(render({'mod8_src': 9, 'mod8_dst': 4, 'mod8_amt': 1.0, 'macro_1': 0.5, 'mod8_cvmode': 1, **{f'mod8_cv{j}': 0.2 for j in range(1, 10)}})) / zero
check('slot 8 works the same way (flat 0.2 -> 1.2x)', abs(s8 - 1.2) < 0.03, f'({s8:.3f})')
print('== STATE ==')
Pa = plugin({'mod3_cvmode': 1, **{f'mod3_cv{j}': (j * 0.1) for j in range(1, 10)}}); Pb = load_plugin(PATH); Pb.raw_state = Pa.raw_state
check('the mode and all nine points survive a save and restore', bool(Pb.mod_3_drawn_curve) and all(abs(getattr(Pb, f'mod_3_curve_point_{j}') - j * 0.1) < 1e-3 for j in range(1, 10)))
print(f'{sum(rows)}/{len(rows)} passed'); sys.exit(0 if all(rows) else 1)
