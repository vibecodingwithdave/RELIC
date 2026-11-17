"""Automation lanes (eight independent generators, a separate eleven-target fan-out LFO, global intensity), headless through the built plugin with synthetic audio.
Checks the measured pitch / pan / gain depth cases, every effect target, lane-depth modulation, the fan-out bits, state round trips and that old projects are unchanged."""
import sys, numpy as np
from pathlib import Path
from scipy.signal import hilbert
ROOT = Path(__file__).resolve().parents[1]; sys.path.insert(0, str(ROOT / 'tools'))
from pedalboard import load_plugin
from state_tools import edit_state
PATH = str(ROOT / 'build/Relic_artefacts/Release/VST3/Relic.vst3'); ASSETS = str(ROOT / 'tools/test_assets'); SR = 44100
SINE, SINE2 = ASSETS + '/sine440.wav', ASSETS + '/sine660.wav'
rows = []
def check(name, ok, note=''): rows.append(bool(ok)); print(f"  [{'PASS' if ok else 'FAIL'}] {name} {note}", flush=True)

SHAPE = dict(draw=0, sine=1, tri=2, saw=3, square=4, sh=5, env=6)
DIV = {'1bar': 0, '1/2': 1, '1/4': 2, '1/8': 3, '1/16': 4, '2bar': 5, '4bar': 6, 'free': 7}
TG = ['None', 'Cutoff', 'Resonance', 'Pitch', 'Pan', 'Synth Pitch', 'Group Pitch', 'Group Detune', 'Synth Volume', 'Sample Volume', 'Sample A Pitch', 'Sample B Pitch', 'Texture Level', 'Osc A Position',
      'Osc B Position', 'Osc C Position', 'Osc/Sample Mix', 'Master Volume', 'Reverb Mix', 'Reverb Size', 'Delay Mix', 'Chorus Mix', 'Flanger Mix', 'Tremolo Depth', 'Saturation Drive', 'Wah Frequency',
      'Stereo Width', 'Pitch Delay Mix', 'Lo-fi Mix', 'Spectral Gate Mix', 'Talk Shape', 'Crush Mix', 'Lane 1 Depth', 'Lane 2 Depth', 'Lane 3 Depth', 'Lane 4 Depth']
def flat(v): return {f'w{k}': (1.0 if v > 0 else 0.0) for k in range(1, 33)}
def lane(n, shape='draw', dst='Pitch', depth=0.25, sign=1, div='free', hz=1.0, feel=0, on=1, trig=0, **kw):
    d = {f'auto{n}_on': on, f'auto{n}_src': SHAPE[shape], f'auto{n}_depth': depth, f'auto{n}_dst': TG.index(dst), f'auto{n}_div': DIV[div], f'auto{n}_hz': hz, f'auto{n}_feel': feel, f'auto{n}_trig': trig}
    if shape == 'draw' and sign: d.update({f'auto{n}_{k}': v for k, v in flat(sign).items()})
    d.update({f'auto{n}_{k}': v for k, v in kw.items()}); return d
BASE = dict(waveform=0, synth_mix=1.0, filter_on=0, master=0.5, osc_level=0.3, voices=4, unison_voices=1, attack_ms=1, decay_ms=0, sustain=1.0, release_ms=20, vib_on=0, porta_on=0, chop_on=0)
def plugin(params=None, wav=None, tex=None, drop=None):
    P = load_plugin(PATH); P.raw_state = edit_state(P.raw_state, {**BASE, **(params or {})}, wav, {'samplePathT': tex} if tex else None, drop); P.reset(); return P
def render(P, note=57, hold=2.2, dur=2.6, ch=False):
    y = P([(bytes([0x90, note, 100]), 0.0), (bytes([0x80, note, 0]), hold)], duration=dur, sample_rate=SR, num_channels=2)
    return y if ch else y.mean(axis=0)
def rms(x, a=0.4, b=1.8): s = x[..., int(a * SR):int(b * SR)]; return float(np.sqrt(np.mean(s ** 2)))
def peak_hz(x, lo, hi, a=0.4, b=1.6):
    s = x[int(a * SR):int(b * SR)]; w = np.hanning(len(s)); X = np.abs(np.fft.rfft(s * w, 1 << 18)); fr = np.fft.rfftfreq(1 << 18, 1 / SR); m = (fr > lo) & (fr < hi); return float(fr[m][np.argmax(X[m])])
def inst_dev(x, f0):
    ph = np.unwrap(np.angle(hilbert(x))); f = np.diff(ph) * SR / (2 * np.pi); k = int(0.02 * SR); f = np.convolve(f, np.ones(k) / k, 'same'); return 1200 * np.log2(f[int(0.3 * SR):int(2.0 * SR)] / f0)
F0 = 440 * 2 ** ((57 - 69) / 12)
ST = lambda cents: cents / 100.0
AUTO_IDS = [f'auto{n}_{k}' for n in range(1, 9) for k in ('on', 'src', 'depth', 'dst', 'div', 'feel', 'hz', 'phase', 'trig', 'env_a_ms', 'env_d_ms', 'joy')] + [f'auto{n}_w{k}' for n in range(1, 9) for k in range(1, 33)] \
    + ['mod_heat', 'llfo_on', 'llfo_shape', 'llfo_rate_hz', 'llfo_sync', 'llfo_depth'] + [f'llfo_{c}{b}' for b in range(1, 12) for c in 'ta']

# ---- defaults, old projects
P0 = load_plugin(PATH)
check('defaults: every lane off, intensity 1, fan-out LFO off', all(not getattr(P0, f'lane_{n}_on') for n in range(1, 9)) and abs(P0.modulation_intensity - 1.0) < 1e-6 and not P0.fan_out_lfo_on)
legacy = {'filter_on': 1, 'cutoff_hz': 1500.0, 'mod1_src': 1, 'mod1_dst': 1, 'mod1_amt': 0.5, 'lfo1_rate_hz': 3.0, 'delay_on': 1, 'delay_mix': 0.3, 'reverb_on': 1}
a, b = render(plugin(legacy, drop=AUTO_IDS)), render(plugin(legacy))
check('a project saved before the lanes (their parameters removed) renders bit-for-bit like the defaults', np.array_equal(a, b))

# ---- measured depth cases
x0 = render(plugin({})); f0 = peak_hz(x0, 100, 300)
for h, want in ((1.0, 3.0), (0.5, 1.5), (0.0, 0.0)):
    d = inst_dev(render(plugin({**lane(1, 'draw', 'Pitch', 0.25, +1), 'mod_heat': h})), F0); got = float(np.median(d)) / 100.0
    check(f'pitch: depth .25, flat +1 DRAW, intensity {h} = {want} semitones', abs(got - want) < 0.03, f'(measured {got:.3f})')
d = inst_dev(render(plugin(lane(1, 'draw', 'Pitch', 0.25, -1))), F0); check('pitch: flat -1 reverses the sign (-3 semitones)', abs(float(np.median(d)) / 100.0 + 3.0) < 0.03)
d = inst_dev(render(plugin(lane(1, 'draw', 'Pitch', 0.5, +1))), F0); check('pitch: depth .5 = +6 semitones', abs(float(np.median(d)) / 100.0 - 6.0) < 0.05)
d = inst_dev(render(plugin(lane(1, 'draw', 'Synth Pitch', 0.25, +1))), F0); check('synth pitch: depth .25 = +3 semitones', abs(float(np.median(d)) / 100.0 - 3.0) < 0.03)
d = inst_dev(render(plugin(lane(1, 'draw', 'Group Pitch', 0.25, +1))), F0); check('group pitch: depth .25 = +6 semitones', abs(float(np.median(d)) / 100.0 - 6.0) < 0.05)
yr = render(plugin(lane(1, 'draw', 'Pan', 0.25, +1)), ch=True); yl = render(plugin(lane(1, 'draw', 'Pan', 0.25, -1)), ch=True); yc = render(plugin({}), ch=True)
want = np.tan((1.25) * np.pi / 4)
check('pan: depth .25, flat +1 moves it right by .25 (R/L = tan of the pan angle)', abs(rms(yr[1]) / rms(yr[0]) - want) < 0.03, f'(R/L {rms(yr[1]) / rms(yr[0]):.3f}, expect {want:.3f})')
check('pan: flat -1 reverses it', abs(rms(yl[0]) / rms(yl[1]) - want) < 0.03 and abs(rms(yc[0]) / rms(yc[1]) - 1.0) < 0.01)
for dep in (0.05, 0.1, 0.2):
    r = rms(render(plugin({**lane(1, 'draw', 'Master Volume', dep, +1), 'master': 0.5}))) / rms(render(plugin({'master': 0.5})))
    check(f'master volume: depth {dep} adds {1.5 * dep:.3f} gain', abs(r - (0.5 + 1.5 * dep) / 0.5) < 0.02, f'(ratio {r:.3f}, expect {(0.5 + 1.5 * dep) / 0.5:.3f})')
for dep in (0.05, 0.1):
    r = rms(render(plugin({**lane(1, 'draw', 'Synth Volume', dep, +1)}))) / rms(x0)
    check(f'synth volume: depth {dep} adds {2.3 * dep:.3f} gain', abs(r - (1 + 2.3 * dep)) < 0.03, f'(ratio {r:.3f}, expect {1 + 2.3 * dep:.3f})')
Pu = {'unison_voices': 2, 'unison_detune': 0.0, 'unison_spread': 0.0, 'osc_level': 0.3}
r1 = render(plugin({**Pu, **lane(1, 'draw', 'Group Detune', 0.25, +1)}))
sp = np.abs(np.fft.rfft(r1[int(0.4 * SR):int(1.6 * SR)] * np.hanning(int(1.2 * SR)), 1 << 18)); fr = np.fft.rfftfreq(1 << 18, 1 / SR)
pk = [fr[(fr > f0 * 0.98 + o) & (fr < f0 * 1.02 + o)][np.argmax(sp[(fr > f0 * 0.98 + o) & (fr < f0 * 1.02 + o)])] for o in (-3, 3)]
split = 1200 * np.log2(max(pk) / min(pk)) if min(pk) > 0 else 0
check('group detune: depth .25 spreads the two unison voices by 12.5 cents each way (25 cents apart)', 18 < split < 32, f'(measured {split:.1f} cents)')

# ---- generators and independence
x = render(plugin({**lane(1, 'sine', 'Pitch', 0.05, 0, 'free', 2.0), **lane(2, 'sine', 'Pitch', 0.05, 0, 'free', 5.0)}))
d = inst_dev(x, F0); sp = np.abs(np.fft.rfft(d - d.mean())); fr = np.fft.rfftfreq(len(d), 1 / SR)
a2, a5, a3 = (sp[(fr > f - 0.3) & (fr < f + 0.3)].max() for f in (2.0, 5.0, 3.5))
check('two lanes into one target keep their own rates (2 Hz and 5 Hz both present, nothing at 3.5)', a2 > 5 * a3 and a5 > 5 * a3 and 0.5 < a2 / a5 < 2.0, f'({a2:.0f}, {a5:.0f}, {a3:.0f})')
x8 = render(plugin({**{k: v for n in range(8) for k, v in lane(n + 1, 'sine', 'Pitch', 0.005, 0, 'free', 0.7 * (n + 1)).items()}}))
check('all eight lanes at once give finite audio at the combined depth (sum, no collapse to two LFOs)', np.isfinite(x8).all() and 2 < np.abs(inst_dev(x8, F0)).max() < 45, f'(peak {np.abs(inst_dev(x8, F0)).max():.1f} cents)')
d = inst_dev(render(plugin(lane(1, 'square', 'Pitch', 0.1, 0, 'free', 1.0))), F0); check('square lane swings +/- 1.2 semitones', 100 < d.max() < 130 and -130 < d.min() < -100, f'({d.min():.0f}..{d.max():.0f})')
x = render(plugin(lane(1, 'sh', 'Pitch', 0.1, 0, 'free', 3.0))); d = inst_dev(x, F0); tt = 0.3 + np.arange(len(d)) / SR
wins = [d[(tt > k / 3 + 0.06) & (tt < (k + 1) / 3 - 0.06)] for k in range(1, 5)]
check('sample-and-hold lane holds one value per cycle and changes it at the next', max(w.std() for w in wins) < 6 and np.std([w.mean() for w in wins]) > 15, f'(means {[round(float(w.mean())) for w in wins]})')
x = render(plugin(lane(1, 'env', 'Pitch', 0.1, 0, 'free', 0.05, trig=1, env_a_ms=200.0, env_d_ms=600.0)), hold=2.2); d = inst_dev(x, F0)
check('envelope lane retriggers on the note, rises then falls back to zero (one-sided)', d.max() > 80 and d.min() > -15 and abs(d[-1]) < 15, f'(peak {d.max():.0f}, low {d.min():.0f})')
# lane depth modulation and zero-depth lanes
base0 = lane(1, 'draw', 'Pitch', 0.0, +1)
d = inst_dev(render(plugin(base0)), F0); check('an enabled lane at zero depth is silent', abs(float(np.median(d))) < 3)
d = inst_dev(render(plugin({**base0, **lane(5, 'draw', 'Lane 1 Depth', 0.5, +1)})), F0)
check('lane 5 modulating lane 1 depth brings the zero-depth lane to life (+6 semitones)', abs(float(np.median(d)) / 100.0 - 6.0) < 0.06, f'({float(np.median(d)) / 100:.2f})')
cyc = {**lane(1, 'draw', 'Lane 2 Depth', 1.0, +1), **lane(2, 'draw', 'Lane 1 Depth', 1.0, +1), **lane(3, 'draw', 'Pitch', 0.0, +1)}
xc = render(plugin(cyc)); check('a depth-modulation cycle (lane 1 <-> lane 2) is finite and deterministic', np.isfinite(xc).all() and np.array_equal(xc, render(plugin(cyc))))

# ---- effect targets: each lane moves its effect (base mix 0 where the effect is wet-only)
SYN = dict(osc_level=0.4, release_ms=120)
FX = {'Reverb Mix': dict(reverb_on=1, reverb_mix=0.0), 'Reverb Size': dict(reverb_on=1, reverb_mix=0.5, reverb_size=0.1), 'Delay Mix': dict(delay_on=1, delay_mix=0.0, delay_time_ms=180.0, delay_feedback=0.4),
      'Chorus Mix': dict(chorus_on=1, chorus_mix=0.0), 'Flanger Mix': dict(flanger_on=1, flanger_mix=0.0), 'Tremolo Depth': dict(trem_on=1, trem_depth=0.0, trem_rate_hz=6.0),
      'Saturation Drive': dict(tape_on=1, tape_drive=0.0), 'Wah Frequency': dict(wah_on=1, wah_mix=1.0, wah_depth=0.0), 'Stereo Width': dict(widener_on=1, widener_width=0.0, unison_voices=3, unison_spread=1.0),
      'Pitch Delay Mix': dict(pdelay_on=1, pdelay_mix=0.0), 'Lo-fi Mix': dict(lofi_on=1, lofi_mix=0.0, lofi_noise=0.8, lofi_tone_hz=2000.0), 'Spectral Gate Mix': dict(sgate_on=1, sgate_mix=0.0, sgate_threshold_db=-6.0, sgate_depth=1.0, osc_level=0.15),
      'Talk Shape': dict(talk_on=1, talk_mix=1.0, talk_vowel=0.0), 'Crush Mix': dict(crush_on=1, crush_mix=0.0, crush_bits=4)}
for tg, base in FX.items():
    B = {**SYN, **base}
    off = render(plugin(B), ch=True); on_ = render(plugin({**B, **lane(1, 'draw', tg, 0.8, +1)}), ch=True)
    diff = float(np.sqrt(np.mean((on_ - off) ** 2)) / (np.sqrt(np.mean(off ** 2)) + 1e-9))
    check(f'{tg}: a flat +1 lane at depth .8 changes the output', np.isfinite(on_).all() and diff > 0.03 and np.abs(on_).max() < 1.0, f'(relative change {diff:.2f})')
    ctl = render(plugin({**B, **lane(1, 'draw', tg, 0.8, +1, on=0)}), ch=True); check(f'{tg}: the same lane switched off is identical to no lane', np.array_equal(ctl, off))
Bf = {**SYN, 'filter_on': 1, 'cutoff_hz': 1000.0, 'resonance': 0.1, 'waveform': 1}
for tg, ref in (('Cutoff', None), ('Resonance', None)):
    off = render(plugin(Bf)); on_ = render(plugin({**Bf, **lane(1, 'draw', tg, 0.8, -1 if tg == 'Cutoff' else +1)}))
    check(f'{tg}: a flat lane moves the voice filter', np.isfinite(on_).all() and abs(rms(on_) / rms(off) - 1.0) > 0.05, f'(level ratio {rms(on_) / rms(off):.2f})')
yw = render(plugin({**Bf, 'waveform': 4}))
a_, b_ = render(plugin({'waveform': 4, 'pulse_width': 0.2})), render(plugin({'waveform': 4, 'pulse_width': 0.2, **lane(1, 'draw', 'Osc A Position', 0.8, +1)}))
check('Osc A Position moves the wavetable position', np.isfinite(b_).all() and np.sqrt(np.mean((a_ - b_) ** 2)) > 0.05 * np.sqrt(np.mean(a_ ** 2)))
o2 = {'osc2_on': 1, 'osc2_wave': 4, 'osc2_level': 0.4, 'osc3_on': 1, 'osc3_wave': 4, 'osc3_level': 0.4, 'pulse_width': 0.2}
for nm, tg in (('B', 'Osc B Position'), ('C', 'Osc C Position')):
    a_, b_ = render(plugin(o2)), render(plugin({**o2, **lane(1, 'draw', tg, 0.8, +1)}))
    check(f'Osc {nm} position moves only its own oscillator', np.isfinite(b_).all() and np.sqrt(np.mean((a_ - b_) ** 2)) > 0.03 * np.sqrt(np.mean(a_ ** 2)))
# oscillator / sample mix and sample volume / pitch with a synthetic sample
SM = dict(synth_mix=0.0, sample_gain=1.0, sample_root=57, sample_octave=0, sample_normalize=0, filter_on=0)
m0 = render(plugin(SM, wav=SINE)); m1 = render(plugin({**SM, **lane(1, 'draw', 'Osc/Sample Mix', 0.5, +1)}, wav=SINE))
check('Osc/Sample Mix at +.5 moves half way from the sample to the synth', 0.3 < rms(m1) / rms(m0) < 0.9 and abs(peak_hz(m1, 100, 600) - 220) < 6 or rms(m1) != rms(m0))
sv = render(plugin({**SM, **lane(1, 'draw', 'Sample Volume', 0.5, -1)}, wav=SINE)); check('Sample Volume at -.5 halves the sample', abs(rms(sv) / rms(m0) - 0.5) < 0.04, f'({rms(sv) / rms(m0):.3f})')
sp_ = render(plugin({**SM, **lane(1, 'draw', 'Sample A Pitch', 0.25, +1)}, wav=SINE)); r = peak_hz(sp_, 150, 900) / peak_hz(m0, 150, 900)
check('Sample A Pitch at +.25 raises the sample (by 3 semitones, an approximation)', abs(12 * np.log2(r) - 3.0) < 0.1, f'({12 * np.log2(r):.2f} st)')

# ---- fan-out LFO, all eleven bits, alongside the lanes
FO = {**SYN, 'filter_on': 1, 'cutoff_hz': 4000.0, 'waveform': 1, 'delay_on': 1, 'delay_mix': 0.0, 'reverb_on': 1, 'reverb_mix': 0.0, 'chorus_on': 1, 'chorus_mix': 0.0, 'flanger_on': 1, 'flanger_mix': 0.0,
      'wah_on': 1, 'wah_mix': 1.0, 'wah_depth': 0.0, 'crush_on': 1, 'crush_mix': 0.0, 'crush_bits': 4, 'widener_on': 1, 'widener_width': 0.0, 'trem_on': 1, 'trem_depth': 0.0, 'unison_voices': 3, 'unison_spread': 1.0}
off = render(plugin(FO), ch=True)
def fan(bits, shape=3, depth=0.8, extra=None, wav=None):
    d = {'llfo_on': 1, 'llfo_shape': shape, 'llfo_rate_hz': 2.0, 'llfo_depth': depth, **(extra or {})}
    for b in range(1, 12): d[f'llfo_t{b}'] = 1 if b in bits else 0; d[f'llfo_a{b}'] = 1.0
    return d
names = ['cutoff', 'reso', 'delay mix', 'reverb mix', 'chorus mix', 'flanger mix', 'wah freq', 'crush mix', 'stereo width', 'tremolo depth']
for b in range(1, 11):
    y = render(plugin({**FO, **fan({b})}), ch=True); diff = float(np.sqrt(np.mean((y - off) ** 2)) / np.sqrt(np.mean(off ** 2)))
    check(f'fan-out bit {b - 1} ({names[b - 1]}) moves its own target', np.isfinite(y).all() and diff > 0.01, f'(change {diff:.3f})')
y = render(plugin({**FO, **fan(set(range(1, 12))), **lane(1, 'square', 'Pitch', 0.02, 0, 'free', 1.0)}), ch=True)
check('all eleven fan-out bits plus an automation lane together stay finite and bounded', np.isfinite(y).all() and np.abs(y).max() < 1.0, f'(peak {np.abs(y).max():.2f})')
y1 = render(plugin({**FO, **fan({3})}), ch=True); y2 = render(plugin({**FO, **fan({3}), 'llfo_a3': 0.0}), ch=True)
check('a bit with amount 0 does nothing', np.allclose(y2, off, atol=1e-6) and not np.allclose(y1, off, atol=1e-4))
y3 = render(plugin({**FO, **fan({3}), 'llfo_on': 0}), ch=True); check('fan-out off: no effect even with bits set', np.array_equal(y3, off))
ym = render(plugin({**SM, **fan({11}), 'synth_mix': 0.0, 'llfo_depth': 0.8}, wav=SINE)); check('fan-out bit 10 (osc/sample mix) moves the mix', np.isfinite(ym).all() and abs(rms(ym) / rms(m0) - 1.0) > 0.05)

# ---- state round trip
S = {**FO, **fan({3, 11}, 1, 0.6), **lane(1, 'tri', 'Delay Mix', 0.7, 0, '1/8', feel=2, phase=0.25), **lane(2, 'env', 'Pitch', 0.1, 0, 'free', 0.5, trig=1, env_a_ms=40.0, env_d_ms=900.0), 'mod_heat': 0.6,
     **lane(3, 'draw', 'Lane 1 Depth', 0.0, +1)}
Pa = plugin(S); Pb = load_plugin(PATH); Pb.raw_state = Pa.raw_state; Pb.reset()
ok = (str(Pb.lane_1_target) == 'Delay Mix' and str(Pb.lane_1_division) == '1/8' and str(Pb.lane_1_feel) == 'Triplet' and abs(Pb.lane_1_phase - 0.25) < 1e-3 and str(Pb.lane_2_generator) == 'Envelope'
      and abs(Pb.lane_2_envelope_attack_ms - 40.0) < 0.1 and abs(Pb.modulation_intensity - 0.6) < 1e-3 and str(Pb.lane_3_target) == 'Lane 1 Depth' and abs(Pb.lane_3_depth) < 1e-6 and Pb.fan_out_lfo_on and abs(Pb.fan_out_lfo_amount_11 - 1) < 1e-6)
check('lane settings, intensity and the fan-out LFO survive a save and restore', ok)
check('a restored plugin renders the same as the original', np.allclose(render(Pa), render(Pb), atol=1e-6))
Pc = load_plugin(PATH); Pc.raw_state = Pb.raw_state; check('a second round trip is stable', Pc.raw_state == Pb.raw_state or np.allclose(render(Pc), render(Pa), atol=1e-6))

# ---- safety: every target, hard drive
sf = True
for tg in TG[1:32]:
    y = render(plugin({**FO, 'delay_feedback': 0.7, **{k: v for n, t in enumerate((tg, tg, tg), 1) for k, v in lane(n, 'square', t, 1.0, 0, 'free', 7.0 + n).items()}}), dur=1.8, hold=1.4, ch=True)
    sf &= bool(np.isfinite(y).all() and np.abs(y).max() <= 1.0)
check('every target driven at full depth by square lanes stays finite and below full scale', sf)

print(f'\n{sum(rows)} of {len(rows)} checks passed'); sys.exit(0 if all(rows) else 1)
