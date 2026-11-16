"""Modulation matrix, second half: slots 5 to 8, the new sources (Key Track, Random) and targets (Pan, Texture Level, Texture Position, Sample Level),
and each slot's Curve and Range. Also that projects saved before these existed load and render as before (headless, Pedalboard, synthetic audio)."""
import sys, numpy as np
from pathlib import Path
from scipy.signal import hilbert
ROOT = Path(__file__).resolve().parents[1]; sys.path.insert(0, str(ROOT / 'tools'))
from pedalboard import load_plugin
from state_tools import edit_state
PATH = str(ROOT / 'build/Relic_artefacts/Release/VST3/Relic.vst3'); ASSETS = str(ROOT / 'tools/test_assets'); SR = 44100
SINE, SINE2, RAMP = ASSETS + '/sine440.wav', ASSETS + '/sine660.wav', ASSETS + '/ramp.wav'
rows = []
def check(name, ok, note=''): rows.append(bool(ok)); print(f"  [{'PASS' if ok else 'FAIL'}] {name} {note}", flush=True)
SRC = dict(lfo1=1, lfo2=2, vel=3, wheel=4, env1=5, env2=6, xx=7, xy=8, m1=9, m2=10, key=11, rand=12)
DST = dict(cut=1, reso=2, pitch=3, level=4, pw=5, pan=6, texlvl=7, texpos=8, samplvl=9)
BASE = dict(waveform=0, synth_mix=1.0, filter_on=0, master=1.0, osc_level=0.3, voices=4, unison_voices=1, attack_ms=1, decay_ms=0, sustain=1.0, release_ms=20, vib_on=0, porta_on=0,
            lfo1_shape=0, lfo1_rate_hz=4.0, lfo1_sync=0, chop_on=0)
def slot(n, src, dst, amt, curve=None, rng=None):
    d = {f'mod{n}_src': SRC[src], f'mod{n}_dst': DST[dst], f'mod{n}_amt': amt}
    if curve is not None: d[f'mod{n}_curve'] = curve
    if rng is not None: d[f'mod{n}_range'] = rng
    return d
def plugin(params=None, wav=None, tex=None, drop=None):
    P = load_plugin(PATH); P.raw_state = edit_state(P.raw_state, {**BASE, **(params or {})}, wav, {'samplePathT': tex} if tex else None, drop); P.reset(); return P
def render(P, note=57, hold=2.2, dur=2.6, ch=False):
    y = P([(bytes([0x90, note, 100]), 0.0), (bytes([0x80, note, 0]), hold)], duration=dur, sample_rate=SR, num_channels=2)
    return y if ch else y.mean(axis=0)
def rms(x, a=0.4, b=1.8): s = x[..., int(a * SR):int(b * SR)]; return float(np.sqrt(np.mean(s ** 2)))
def amp(x, f, a=0.4, b=1.6):
    s = x[int(a * SR):int(b * SR)]; w = np.hanning(len(s)); X = np.abs(np.fft.rfft(s * w, 1 << 17)); fr = np.fft.rfftfreq(1 << 17, 1 / SR)
    return float(2 * X[(fr > f - 12) & (fr < f + 12)].max() / w.sum())
def inst_dev(x, f0):
    ph = np.unwrap(np.angle(hilbert(x))); f = np.diff(ph) * SR / (2 * np.pi); k = int(0.02 * SR); f = np.convolve(f, np.ones(k) / k, 'same'); return 1200 * np.log2(f[int(0.3 * SR):int(2.0 * SR)] / f0)
F0 = 440 * 2 ** ((57 - 69) / 12)
NEW = [f'mod{n}_{k}' for n in range(1, 9) for k in ('curve', 'range')] + [f'mod{n}_{k}' for n in range(5, 9) for k in ('src', 'dst', 'amt')]

# ---- defaults, saved state, old projects
P0 = load_plugin(PATH)
check('defaults: slots 5-8 empty, Curve centred, Range Full', all(str(getattr(P0, f'mod_{n}_source')) == 'None' and str(getattr(P0, f'mod_{n}_destination')) == 'None' and abs(getattr(P0, f'mod_{n}_amount')) < 1e-6
      and abs(getattr(P0, f'mod_{n}_curve')) < 1e-6 and str(getattr(P0, f'mod_{n}_range')) == 'Full' for n in range(5, 9)))
Pa = plugin({**slot(7, 'key', 'pan', -0.6, 0.4, 2), **slot(2, 'lfo2', 'texpos', 0.3, -0.7, 1)}); Pb = load_plugin(PATH); Pb.raw_state = Pa.raw_state
check('slot 7 and slot 2 settings (source, target, amount, curve, range) survive a save and restore',
      str(Pb.mod_7_source) == 'Key Track' and str(Pb.mod_7_destination) == 'Pan' and abs(Pb.mod_7_amount + 0.6) < 1e-3 and abs(Pb.mod_7_curve - 0.4) < 1e-3 and str(Pb.mod_7_range) == 'Negative'
      and str(Pb.mod_2_source) == 'LFO 2' and str(Pb.mod_2_destination) == 'Texture Position' and abs(Pb.mod_2_curve + 0.7) < 1e-3 and str(Pb.mod_2_range) == 'Positive')
legacy = {**slot(1, 'lfo1', 'cut', 0.5), **slot(2, 'lfo1', 'pitch', 0.03), **slot(3, 'm1', 'level', 0.4), 'macro_1': 0.7, 'filter_on': 1, 'cutoff_hz': 1500.0}
check('a project saved before these controls (their parameters removed) renders the same as with the defaults', np.allclose(render(plugin(legacy, drop=NEW)), render(plugin(legacy)), atol=1e-6))

# ---- slots 5 to 8 behave like slots 1 to 4
devs = []
for n in (1, 5, 8):
    d = inst_dev(render(plugin(slot(n, 'lfo1', 'pitch', 0.05))), F0); devs.append((d.max(), d.min()))
check('slots 1, 5 and 8 each give the same pitch modulation (+/-60 cents)', all(54 < a < 66 and -66 < b < -54 for a, b in devs), str([(round(a), round(b)) for a, b in devs]))
x = render(plugin({**slot(5, 'lfo1', 'pitch', 0.05), **slot(6, 'm1', 'level', -0.5), **slot(7, 'm2', 'level', 0.5), 'macro_1': 1.0, 'macro_2': 1.0}))
x0 = render(plugin({**slot(5, 'lfo1', 'pitch', 0.05)}))
check('slots 5-7 together: pitch from slot 5 is kept and the two level slots cancel', abs(rms(x) / rms(x0) - 1.0) < 0.03 and 54 < inst_dev(x, F0).max() < 66)
# ---- sources
r = {n: rms(render(plugin(slot(1, 'key', 'level', 1.0)), note=n)) for n in (36, 60, 84)}
check('Key Track to Level at +1: note 60 unchanged, note 84 doubles it, note 36 is silent', r[36] < 0.02 * r[60] and abs(r[84] / r[60] - 2.0) < 0.1, f'({r[36]:.4f} {r[60]:.3f} {r[84]:.3f})')
P = plugin(slot(1, 'rand', 'level', 0.5)); ev = []
for k in range(8): ev += [(bytes([0x90, 60, 100]), 1.0 * k), (bytes([0x80, 60, 0]), 1.0 * k + 0.7)]
yy = P(ev, duration=8.2, sample_rate=SR, num_channels=2).mean(axis=0); lv = [rms(yy, k + 0.2, k + 0.6) for k in range(8)]
lv = np.array(lv)
check('Random to Level: each note gets its own level, between 0.5x and 1.5x', lv.std() / lv.mean() > 0.05 and lv.max() / np.median(lv) < 1.6 and lv.min() / np.median(lv) > 0.4, f'(spread {lv.min():.3f}..{lv.max():.3f})')
Pr = plugin(slot(1, 'rand', 'level', 0.5)); w = [rms(Pr([(bytes([0x90, 60, 100]), 0.0), (bytes([0x80, 60, 0]), 1.2)], duration=1.5, sample_rate=SR, num_channels=2).mean(axis=0), a, a + 0.2) for a in (0.2, 0.5, 0.8)]
check('Random is steady within a note', max(w) / min(w) < 1.03)
# ---- targets
y = render(plugin({**slot(1, 'm1', 'pan', 1.0), 'macro_1': 1.0}), ch=True); y0 = render(plugin({}), ch=True)
check('Pan to full right: the left channel goes silent and the right gets louder', rms(y[0]) < 0.02 * rms(y[1]) and rms(y[1]) > 1.2 * rms(y0[1]), f'(L {rms(y[0]):.4f}, R {rms(y[1]):.3f})')
y = render(plugin({**slot(1, 'm1', 'pan', -1.0), 'macro_1': 1.0}), ch=True); check('Pan to full left: the right channel goes silent', rms(y[1]) < 0.02 * rms(y[0]))
TEX = dict(synth_mix=0.0, sample_gain=0.0, sample_root=69, sample_octave=0, sample_normalize=0, tex_level=0.8, tex_spread=0.0, tex_size_ms=120.0, tex_density=40.0, tex_jitter_ct=0.0, tex_normalize=0, tex_indep=1, tex_root=69, tex_pos=0.25)
ref = amp(render(plugin(TEX, wav=SINE, tex=SINE2), note=69), 660)
off = amp(render(plugin({**TEX, **slot(1, 'm1', 'texlvl', -1.0), 'macro_1': 1.0}, wav=SINE, tex=SINE2), note=69), 660)
half = amp(render(plugin({**TEX, **slot(1, 'm1', 'texlvl', -0.5), 'macro_1': 1.0}, wav=SINE, tex=SINE2), note=69), 660)
check('Texture Level: -1 silences the texture, -0.5 halves it', off < 0.03 * ref and abs(half / ref - 0.5) < 0.08, f'({off / ref:.3f}, {half / ref:.2f})')
a0 = rms(render(plugin({**TEX, 'tex_pos': 0.0}, wav=SINE, tex=RAMP), note=69), 0.4, 1.2); a1 = rms(render(plugin({**TEX, 'tex_pos': 0.0, **slot(1, 'm1', 'texpos', 0.8), 'macro_1': 1.0}, wav=SINE, tex=RAMP), note=69), 0.4, 1.2)
aref = rms(render(plugin({**TEX, 'tex_pos': 0.4}, wav=SINE, tex=RAMP), note=69), 0.4, 1.2)
check('Texture Position: +0.8 on a 0.5-range target moves the grains 0.4 up the rising ramp (as position 0.4 does)', a1 > 3 * a0 and abs(a1 / aref - 1.0) < 0.15, f'(pos 0: {a0:.3f}, modulated: {a1:.3f}, pos 0.4: {aref:.3f})')
SM = dict(synth_mix=0.0, sample_root=69, sample_octave=0, sample_normalize=0, sample_gain=0.5, tex_level=0.8, tex_spread=0.0, tex_size_ms=120.0, tex_density=40.0, tex_jitter_ct=0.0, tex_normalize=0, tex_indep=1, tex_root=69)
b = render(plugin(SM, wav=SINE, tex=SINE2), note=69); m = render(plugin({**SM, **slot(1, 'm1', 'samplvl', -1.0), 'macro_1': 1.0}, wav=SINE, tex=SINE2), note=69)
check('Sample Level: -1 silences sample A (440 Hz) and leaves the texture (660 Hz)', amp(m, 440) < 0.03 * amp(b, 440) and amp(m, 660) > 0.9 * amp(b, 660), f'(A {amp(m,440)/amp(b,440):.3f}, T {amp(m,660)/amp(b,660):.2f})')
# ---- range and curve
ref_d = inst_dev(render(plugin(slot(1, 'lfo1', 'pitch', 0.05))), F0)
dp = inst_dev(render(plugin(slot(1, 'lfo1', 'pitch', 0.05, rng=1))), F0); dn = inst_dev(render(plugin(slot(1, 'lfo1', 'pitch', 0.05, rng=2))), F0)
check('Range Positive keeps only the upward half of the LFO (about 0 to +60 cents)', dp.max() > 50 and dp.min() > -6, f'({dp.min():.0f}..{dp.max():.0f})')
check('Range Negative keeps only the downward half (about -60 to 0 cents)', dn.min() < -50 and dn.max() < 6, f'({dn.min():.0f}..{dn.max():.0f})')
base = rms(render(plugin({**slot(1, 'm1', 'level', 1.0), 'macro_1': 0.5})))
cu = rms(render(plugin({**slot(1, 'm1', 'level', 1.0, 1.0), 'macro_1': 0.5}))); cd = rms(render(plugin({**slot(1, 'm1', 'level', 1.0, -1.0), 'macro_1': 0.5}))); cz = rms(render(plugin({'macro_1': 0.5})))
check('Curve on a half-way source: centre = 1.5x, +1 (slow start) = 1.06x, -1 (fast start) = 1.84x', abs(base / cz - 1.5) < 0.04 and abs(cu / cz - 1.0625) < 0.04 and abs(cd / cz - 1.84) < 0.06, f'({base/cz:.2f}, {cu/cz:.2f}, {cd/cz:.2f})')
dc = inst_dev(render(plugin(slot(1, 'lfo1', 'pitch', 0.05, 1.0))), F0)
check('Curve keeps the sign and the peaks of a two-sided source, but spends less time at the extremes', 50 < dc.max() < 70 and -70 < dc.min() < -50 and np.mean(np.abs(dc)) < 0.75 * np.mean(np.abs(ref_d)), f'(mean |dev| {np.mean(np.abs(dc)):.0f} vs {np.mean(np.abs(ref_d)):.0f})')
# ---- stress
P = plugin({**slot(1, 'lfo1', 'cut', 0.6, 0.5), **slot(2, 'rand', 'pan', 0.8), **slot(3, 'key', 'pitch', 0.02), **slot(4, 'lfo2', 'reso', 0.5), **slot(5, 'm1', 'texpos', 0.5, -0.5), **slot(6, 'm2', 'samplvl', 0.4),
           **slot(7, 'vel', 'texlvl', 0.5), **slot(8, 'wheel', 'level', -0.5, 0.9, 1), 'filter_on': 1, 'macro_1': 0.8, 'macro_2': 0.3, 'voices': 16}, wav=SINE, tex=SINE2)
ev = []
for i, n in enumerate((36, 43, 52, 59, 66, 74, 81, 88)): ev += [(bytes([0x90, n, 110]), 0.03 * i), (bytes([0x80, n, 0]), 1.2)]
y = P(ev, duration=1.8, sample_rate=SR, num_channels=2)
check('all eight slots active with every new target, ten voices: finite and never above full scale', np.isfinite(y).all() and np.abs(y).max() <= 1.0 + 1e-6, f'(peak {np.abs(y).max():.3f})')
print(f"\nRESULT: {sum(rows)} of {len(rows)} modulation slot checks passed"); sys.exit(0 if all(rows) else 1)
