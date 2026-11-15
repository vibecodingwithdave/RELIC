"""Verify LFOs, the modulation matrix, vibrato, portamento, mod wheel and pitch bend (headless, Pedalboard)."""
import numpy as np
from scipy.signal import hilbert
from pedalboard import load_plugin
P = load_plugin(str(__import__('pathlib').Path(__file__).resolve().parents[1] / 'build/Relic_artefacts/Release/VST3/Relic.vst3')); SR = 44100; rows = []
def passed(n, ok, note=''): rows.append(bool(ok)); print(f"  [{'PASS' if ok else 'FAIL'}] {n} {note}")
BASE = dict(waveform='Sine', filter_on=False, tremolo_on=False, delay_on=False, comp_on=False, crush_on=False, pan_on=False, chorus_on=False, flanger_on=False, reverb_on=False,
            synth_mix=1.0, master=1.0, osc_level=0.3, voices=4, unison_voices=1, attack_ms=1, decay_ms=0, sustain=1.0, release_ms=20, vib_on=False, porta_on=False,
            lfo1_shape='Sine', lfo1_rate_hz=4.0, lfo1_sync='Free', lfo2_shape='Sine', lfo2_rate_hz=0.5, lfo2_sync='Free')
for k in (1, 2, 3, 4): BASE.update({f'mod{k}_src': 'None', f'mod{k}_dst': 'None', f'mod{k}_amt': 0.0})
import re
def pn(k):   # our short ids -> the names Pedalboard derives from the parameter display names
    k = re.sub(r'^(lfo|mod)(\d)_', r'\1_\2_', k); k = k.replace('_src', '_source').replace('_dst', '_destination').replace('_amt', '_amount')
    return k.replace('vib_', 'vibrato_').replace('porta_', 'portamento_')
def render(events, dur, **kw):
    for k, v in {**BASE, **kw}.items(): setattr(P, pn(k), v)
    P.reset(); return P(events, duration=dur, sample_rate=SR, num_channels=2).mean(axis=0)
def note(n=57, vel=100, a=0.0, b=None, dur=3.0): return [(bytes([0x90, n, vel]), a), (bytes([0x80, n, 0]), b if b is not None else dur - 0.1)]
def inst_freq(x, smooth=0.02):
    ph = np.unwrap(np.angle(hilbert(x))); f = np.diff(ph) * SR / (2 * np.pi); k = int(smooth * SR); f = np.convolve(f, np.ones(k) / k, 'same'); return f
def brightness(x, a, b):
    s = x[int(a * SR):int(b * SR)]; X = np.abs(np.fft.rfft(s * np.hanning(len(s)))); fr = np.fft.rfftfreq(len(s), 1 / SR); return float(X[fr > 1500].sum() / (X.sum() + 1e-12))
def windows(x, w=0.1, a=0.2, b=2.8): return [brightness(x, t, t + w) for t in np.arange(a, b, w)]
def rms_w(x, w=0.05, a=0.2, b=2.8): return [float(np.sqrt(np.mean(x[int(t * SR):int((t + w) * SR)] ** 2))) for t in np.arange(a, b, w)]

print('== pitch')
f0 = 440 * 2 ** ((57 - 69) / 12)
x = render(note(), 3.0, vib_on=True, vib_rate_hz=5.0, vib_depth_ct=50.0); f = inst_freq(x)[int(0.3 * SR):int(2.7 * SR)]
dev = 1200 * np.log2(f / f0); passed('vibrato 50 cents: peak deviation 50 cents (45 to 55)', 45 < dev.max() < 55 and -55 < dev.min() < -45, '(%.1f / %.1f)' % (dev.max(), dev.min()))
zc = np.sum(np.diff(np.sign(dev - dev.mean())) != 0) / 2 / 2.4; passed('vibrato rate 5 Hz', abs(zc - 5.0) < 0.3, '(%.2f Hz)' % zc)
passed('vibrato off: pitch is steady', np.ptp(1200 * np.log2(inst_freq(render(note(), 3.0))[int(0.3 * SR):int(2.7 * SR)] / f0)) < 1.0)
x = render(note(), 3.0, lfo1_rate_hz=4.0, mod1_src='LFO 1', mod1_dst='Pitch', mod1_amt=0.05); dev = 1200 * np.log2(inst_freq(x)[int(0.3 * SR):int(2.7 * SR)] / f0)
passed('matrix LFO 1 to pitch at 5%: +/-60 cents', 54 < dev.max() < 66 and -66 < dev.min() < -54, '(%.1f / %.1f)' % (dev.max(), dev.min()))
x = render(note(), 3.0, lfo1_rate_hz=0.1, lfo1_sync='1/4', mod1_src='LFO 1', mod1_dst='Pitch', mod1_amt=0.05); f = inst_freq(x)[int(0.3 * SR):int(2.7 * SR)]; dev = 1200 * np.log2(f / f0)
zc = np.sum(np.diff(np.sign(dev - dev.mean())) != 0) / 2 / 2.4; passed('LFO sync 1/4 at 120 BPM runs at 2 Hz', abs(zc - 2.0) < 0.25, '(%.2f Hz)' % zc)
x = render([(bytes([0xE0, 127, 127]), 0.0)] + note(), 2.0); f = inst_freq(x)[int(0.3 * SR):int(1.5 * SR)].mean(); passed('pitch bend full up = +2 semitones', abs(1200 * np.log2(f / f0) - 200) < 3, '(%.1f cents)' % (1200 * np.log2(f / f0)))
x = render(note(57, 100, 0.0, 0.5) + note(69, 100, 0.6, 2.4), 3.0, porta_on=True, porta_ms=300.0); f = inst_freq(x, 0.01)
f_early, f_late = f[int(0.62 * SR)], f[int(2.0 * SR)]; f_mid = f[int(0.75 * SR)]
passed('portamento: starts near the old note, glides, lands on the new one', f_early < 260 and 260 < f_mid < 430 and abs(f_late - 440) < 3, '(%.0f -> %.0f -> %.0f Hz)' % (f_early, f_mid, f_late))
x = render(note(57, 100, 0.0, 0.5) + note(69, 100, 0.6, 2.4), 3.0, porta_on=False); f = inst_freq(x, 0.01); passed('portamento off: jumps straight to the new pitch', abs(f[int(0.65 * SR)] - 440) < 5)

print('== matrix')
SAW = dict(waveform='Saw', filter_on=True, filter_type='LP', cutoff_hz=1000.0, resonance=0.1)
base = windows(render(note(), 3.0, **SAW)); passed('no modulation: brightness is steady', max(base) / min(base) < 1.1, '(%.2f)' % (max(base) / min(base)))
x = render(note(), 3.0, lfo1_rate_hz=1.0, mod1_src='LFO 1', mod1_dst='Cutoff', mod1_amt=0.5, **SAW); w = windows(x)
passed('LFO to cutoff: the sound sweeps bright and dark', max(w) / min(w) > 3.0, '(%.1fx)' % (max(w) / min(w)))
x = render(note(), 3.0, lfo1_rate_hz=1.0, mod1_src='LFO 1', mod1_dst='Cutoff', mod1_amt=-0.5, **SAW); w2 = windows(x)
passed('negative amount inverts the sweep', np.corrcoef(w, w2)[0, 1] < -0.5, '(corr %.2f)' % np.corrcoef(w, w2)[0, 1])
lo = windows(render(note(vel=40), 3.0, mod1_src='Velocity', mod1_dst='Cutoff', mod1_amt=0.6, **SAW)); hi = windows(render(note(vel=127), 3.0, mod1_src='Velocity', mod1_dst='Cutoff', mod1_amt=0.6, **SAW))
passed('velocity to cutoff: harder is brighter', np.mean(hi) > 1.5 * np.mean(lo), '(%.3f vs %.3f)' % (np.mean(hi), np.mean(lo)))
cc = lambda v: [(bytes([0xB0, 1, v]), 0.0)]
lo = windows(render(cc(0) + note(), 3.0, mod1_src='Mod Wheel', mod1_dst='Cutoff', mod1_amt=0.6, **SAW)); hi = windows(render(cc(127) + note(), 3.0, mod1_src='Mod Wheel', mod1_dst='Cutoff', mod1_amt=0.6, **SAW))
passed('mod wheel to cutoff: wheel up is brighter', np.mean(hi) > 1.5 * np.mean(lo), '(%.3f vs %.3f)' % (np.mean(hi), np.mean(lo)))
x = render(note(), 3.0, lfo1_shape='Square', lfo1_rate_hz=2.0, mod1_src='LFO 1', mod1_dst='Level', mod1_amt=1.0); r = rms_w(x, 0.05, 0.3, 2.6)
passed('LFO square to level: alternates between silent and double', min(r) < 0.03 * max(r), '(min %.3f, max %.3f)' % (min(r), max(r)))
h2 = lambda x: float(np.abs(np.fft.rfft(x[int(0.3 * SR):int(2.7 * SR)] * np.hanning(int(2.4 * SR))))[int(2 * 220 * 2.4):int(2 * 220 * 2.4) + 3].max())
sq = dict(waveform='Square', pulse_width=0.5); st = render(note(), 3.0, **sq); md = render(note(), 3.0, lfo1_shape='Square', lfo1_rate_hz=3.0, mod1_src='LFO 1', mod1_dst='Pulse Width', mod1_amt=0.5, **sq)
passed('LFO to pulse width adds even harmonics', h2(md) > 20 * h2(st), '(%.3f vs %.4f)' % (h2(md), h2(st)))
x = render(note(), 3.0, lfo1_rate_hz=1.0, mod1_src='LFO 1', mod1_dst='Resonance', mod1_amt=1.0, **SAW); w = windows(x)
passed('LFO to resonance changes the sound', max(w) / min(w) > 1.2, '(%.2fx)' % (max(w) / min(w)))
x = render(note(), 3.0, lfo1_shape='Random', lfo1_rate_hz=6.0, mod1_src='LFO 1', mod1_dst='Pitch', mod1_amt=0.08); dev = 1200 * np.log2(inst_freq(x, 0.005)[int(0.3 * SR):int(2.7 * SR)] / f0)
passed('random LFO: steps between values, stays within range', np.ptp(dev) > 30 and dev.max() < 105 and dev.min() > -105, '(%.0f to %.0f cents)' % (dev.min(), dev.max()))
x = render(note(), 3.0, lfo1_rate_hz=1.0, mod1_src='LFO 1', mod1_dst='Cutoff', mod1_amt=0.5, mod2_src='LFO 1', mod2_dst='Cutoff', mod2_amt=-0.5, **SAW); w = windows(x)
passed('two slots on one destination add up (cancel here)', max(w) / min(w) < 1.15, '(%.2fx)' % (max(w) / min(w)))
x = render(note(), 3.0, lfo1_rate_hz=2.0, mod1_src='LFO 1', mod1_dst='Pitch', mod1_amt=0.3, vib_on=True, vib_depth_ct=100.0, voices=16, unison_voices=7, unison_detune=30.0, osc_level=0.5)
passed('everything modulated at once: finite, under full scale', bool(np.isfinite(x).all()) and float(np.abs(x).max()) < 1.0, '(peak %.2f)' % float(np.abs(x).max()))
print(f"\nRESULT: {sum(rows)} of {len(rows)} modulation checks passed")
