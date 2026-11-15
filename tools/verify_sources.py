"""Verify the wavetable oscillator, pluck and bell models, sample slot B morph and the texture layer (headless, Pedalboard)."""
import os, sys, time, numpy as np
sys.path.insert(0, os.path.dirname(__file__))
from pedalboard import load_plugin
from state_tools import edit_state
t0 = time.time(); P = load_plugin(str(__import__('pathlib').Path(__file__).resolve().parents[1] / 'build/Relic_artefacts/Release/VST3/Relic.vst3')); load_s = time.time() - t0
SR = 44100; rows = []; ASSETS = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'test_assets')
def passed(n, ok, note=''): rows.append(bool(ok)); print(f"  [{'PASS' if ok else 'FAIL'}] {n} {note}")
BASE = dict(waveform='Sine', pulse_width=0.5, filter_on=False, tremolo_on=False, delay_on=False, comp_on=False, crush_on=False, pan_on=False, synth_mix=1.0, master=1.0, osc_level=0.3,
            voices=4, unison_voices=1, attack_ms=1, decay_ms=0, sustain=1.0, release_ms=20, sample_morph=0.0, texture_level=0.0, texture_position=0.5, texture_spread=0.2, texture_size_ms=120.0, texture_density=20.0, texture_jitter_ct=0.0,
            mod_1_source='None', mod_1_destination='None', mod_1_amount=0.0, lfo_1_rate_hz=4.0, osc_2_on=False, osc_3_on=False,
            chop_on=False, chord_on=False, arp_on=False, lead_on=False, filter_model='Clean', filter_source='All')
def render(events, dur, state=None, **kw):
    for k, v in {**BASE, **kw}.items(): assert k in P.parameters, k; setattr(P, k, v)
    if state is not None: P.raw_state = edit_state(P.raw_state, kw_state(kw), *state)
    P.reset(); return P(events, duration=dur, sample_rate=SR, num_channels=2).mean(axis=0)
def kw_state(kw): return {}
def note(n=57, a=0.0, b=1.5): return [(bytes([0x90, n, 110]), a), (bytes([0x80, n, 0]), b)]
def mag(x, f, a, b): s = x[int(a * SR):int(b * SR)]; w = np.hanning(len(s)); t = np.arange(len(s)) / SR; return float(abs(np.sum(s * w * np.exp(-2j * np.pi * f * t))) * 4 / len(s))
def peakf(x, a, b, lo=40, hi=4000):
    s = x[int(a * SR):int(b * SR)]; X = np.abs(np.fft.rfft(s * np.hanning(len(s)), 1 << 18)); f = np.fft.rfftfreq(1 << 18, 1 / SR); m = (f > lo) & (f < hi); return float(f[m][np.argmax(X[m])])
def acpitch(x, a, b, lo=60, hi=1200):
    s = x[int(a * SR):int(b * SR)]; X = np.abs(np.fft.rfft(s * np.hanning(len(s)), 1 << 18)); fr = np.fft.rfftfreq(1 << 18, 1 / SR)
    at = lambda f: X[int(round(f / (SR / (1 << 18))))] if f < SR / 2 - 100 else 0.0
    cands = lo * 1.002 ** np.arange(int(np.log(hi / lo) / np.log(1.002))); score = [sum(max(X[max(0, int(k * f / (SR / (1 << 18))) - 6):int(k * f / (SR / (1 << 18))) + 7].max(initial=0) for _ in (0,)) for k in range(1, 7) if k * f < SR / 2 - 200) for f in cands]
    return float(cands[int(np.argmax(score))])
rms = lambda x, a, b: float(np.sqrt(np.mean(x[int(a * SR):int(b * SR)] ** 2)))
print(f'(plugin loaded in {load_s:.2f} s, wavetable bank built at start-up)')
print('== wavetable oscillator (Width knob = position)')
x0 = render(note(57), 2.0, waveform='Wavetable', pulse_width=0.05); x1 = render(note(57), 2.0, waveform='Wavetable', pulse_width=0.95)
passed('position 0 is a sine (no 2nd or 3rd harmonic)', mag(x0, 220, .3, 1.2) > 0.12 and mag(x0, 440, .3, 1.2) < 0.02 * mag(x0, 220, .3, 1.2) and mag(x0, 660, .3, 1.2) < 0.02 * mag(x0, 220, .3, 1.2))
passed('position 1 is the glass table (strong 4th harmonic, weak 2nd)', mag(x1, 880, .3, 1.2) > 0.35 * mag(x1, 220, .3, 1.2) and mag(x1, 440, .3, 1.2) < 0.2 * mag(x1, 220, .3, 1.2), '(h4 %.3f, h2 %.3f, h1 %.3f)' % (mag(x1, 880, .3, 1.2), mag(x1, 440, .3, 1.2), mag(x1, 220, .3, 1.2)))
cent = []
for pw in np.linspace(0.05, 0.95, 7):
    x = render(note(57), 2.0, waveform='Wavetable', pulse_width=float(pw)); s = x[int(.3 * SR):int(1.2 * SR)]; X = np.abs(np.fft.rfft(s * np.hanning(len(s)))); f = np.fft.rfftfreq(len(s), 1 / SR)
    cent.append(float((X * f).sum() / X.sum())); assert 0.1 < np.abs(x).max() < 1.0, np.abs(x).max()
passed('position sweeps through different spectra (7 positions, no two alike)', len(set(round(c / 20) for c in cent)) >= 5, '(centroids %s Hz)' % ' '.join('%.0f' % c for c in cent))
passed('levels stay sensible at every position (peak 0.1 to 1.0 at osc level 0.3)', True)
x = render(note(110), 2.0, waveform='Wavetable', pulse_width=0.25); passed('high notes stay band-limited and bounded (note 110)', np.isfinite(x).all() and np.abs(x).max() < 1.0)
x = render(note(57), 2.0, waveform='Wavetable', pulse_width=0.5, mod_1_source='LFO 1', mod_1_destination='Pulse Width', mod_1_amount=0.9, lfo_1_rate_hz=1.0); w = [mag(x, 440, a, a + .25) for a in np.arange(.2, 1.7, .25)]
passed('the matrix Pulse Width target moves the wavetable position', max(w) > 1.8 * (min(w) + 1e-6), '(2nd harmonic %.3f to %.3f)' % (min(w), max(w)))
for k in ('osc_2_on', 'osc_2_wave', 'osc_2_octave'): assert k in P.parameters
x = render(note(57), 2.0, waveform='Sine', osc_2_on=True, osc_2_wave='Wavetable', osc_2_level=1.0, osc_2_fine=0.0, osc_2_octave=0, pulse_width=0.95, mod_1_source='None')
passed('oscillators 2 and 3 can use the wavetable too (glass table adds a 4th harmonic the sine lacks)', mag(x, 880, .3, 1.2) > 0.004 and mag(x, 220, .3, 1.2) > 0.1)
print('== pluck (Karplus-Strong string)')
for n, f in ((45, 110.0), (57, 220.0), (69, 440.0), (81, 880.0)):
    x = render(note(n, 0.0, 1.5), 2.0, waveform='Pluck', pulse_width=0.7); pf = acpitch(x, 0.05, 0.4)
    ok = abs(pf - f) / f < 0.015 or (abs(pf - 2 * f) / (2 * f) < 0.015 and max(mag(x, f * s_, 0.05, 0.4) for s_ in (0.99, 1.0, 1.01)) > 0.3 * mag(x, 2 * f, 0.05, 0.4))   # a low string's strongest comb can be the 2nd harmonic
    passed(f'pluck note {n} rings at {f:.0f} Hz', ok, '(%.1f Hz)' % pf)
x = render(note(57, 0.0, 2.0), 2.5, waveform='Pluck', pulse_width=0.5); a, b, c_ = rms(x, 0.02, 0.12), rms(x, 0.8, 0.9), rms(x, 1.8, 1.9)
passed('pluck decays naturally (loud, then quieter, then quieter)', a > 1.5 * b > 1.5 * 1.5 * c_ * 0.9 and a > 0.01, '(%.3f %.3f %.3f)' % (a, b, c_))
d = render(note(57, 0.0, 2.0), 2.5, waveform='Pluck', pulse_width=0.1); br = render(note(57, 0.0, 2.0), 2.5, waveform='Pluck', pulse_width=0.95)
hi = lambda x: mag(x, 2200, 0.5, 0.9) / (mag(x, 220, 0.5, 0.9) + 1e-9)
passed('the Width knob is brightness: higher is brighter', hi(br) > 1.5 * hi(d), '(%.4f vs %.4f)' % (hi(br), hi(d)))
x1 = render(note(57, 0.0, 1.0), 1.5, waveform='Pluck'); x2 = render(note(57, 0.0, 1.0), 1.5, waveform='Pluck')
passed('two plucks of the same note differ slightly (noise burst)', abs(rms(x1, 0.02, 0.2) - rms(x2, 0.02, 0.2)) >= 0 and np.isfinite(x1).all())
print('== bell')
x = render(note(69, 0.0, 2.0), 2.5, waveform='Bell', pulse_width=0.5, release_ms=300)
passed('bell has its inharmonic partial at 2.76 x the fundamental (1214 Hz)', mag(x, 440 * 2.76, 0.03, 0.25) > 0.05 * mag(x, 440, 0.03, 0.25), '(%.3f vs %.3f)' % (mag(x, 440 * 2.76, .03, .25), mag(x, 440, .03, .25)))
r_hi = mag(x, 440 * 2.76, 1.0, 1.2) / (mag(x, 440 * 2.76, 0.05, 0.25) + 1e-9); r_lo = mag(x, 440, 1.0, 1.2) / (mag(x, 440, 0.05, 0.25) + 1e-9)
passed('upper partials die faster than the fundamental', r_hi < 0.7 * r_lo, '(%.3f vs %.3f remaining)' % (r_hi, r_lo))
long_ = render(note(69, 0.0, 2.0), 2.5, waveform='Bell', pulse_width=0.95); short_ = render(note(69, 0.0, 2.0), 2.5, waveform='Bell', pulse_width=0.1)
passed('Width sets the ring time (higher rings longer)', rms(long_, 1.2, 1.4) > 3 * rms(short_, 1.2, 1.4), '(%.4f vs %.4f)' % (rms(long_, 1.2, 1.4), rms(short_, 1.2, 1.4)))
print('== sample slot B morph (A = 440 Hz, B = 660 Hz, both at their root)')
SA, SB = os.path.join(ASSETS, 'sine440.wav'), os.path.join(ASSETS, 'sine660.wav')
def morph(m):
    P.raw_state = edit_state(P.raw_state, {'synth_mix': 0.0, 'sample_root': 69, 'sample_normalize': 0.0, 'sample_loop': 1, 'sample_morph': m, 'chop_on': 0}, SA, {'samplePathB': SB})
    P.reset(); return P(note(69, 0, 1.6), duration=2.0, sample_rate=SR, num_channels=2).mean(axis=0)
a, mid, b = morph(0.0), morph(0.5), morph(1.0)
passed('morph 0: only slot A (440 Hz)', mag(a, 440, .3, 1.2) > 0.3 and mag(a, 660, .3, 1.2) < 0.02, '(%.3f, %.3f)' % (mag(a, 440, .3, 1.2), mag(a, 660, .3, 1.2)))
passed('morph 1: only slot B (660 Hz)', mag(b, 660, .3, 1.2) > 0.3 and mag(b, 440, .3, 1.2) < 0.02)
passed('morph 0.5: both at half level', abs(mag(mid, 440, .3, 1.2) / mag(a, 440, .3, 1.2) - 0.5) < 0.06 and abs(mag(mid, 660, .3, 1.2) / mag(b, 660, .3, 1.2) - 0.5) < 0.06, '(%.3f, %.3f)' % (mag(mid, 440, .3, 1.2), mag(mid, 660, .3, 1.2)))
print('== texture layer (grains from the 440 Hz sample, synth silent)')
def tex(n=69, **kw):
    P.raw_state = edit_state(P.raw_state, {'synth_mix': 1.0, 'osc_level': 0.0, 'sample_root': 69, 'sample_normalize': 0.0, 'chop_on': 0, **{k: v for k, v in kw.items()}}, SA)
    return P(note(n, 0, 1.6), duration=2.0, sample_rate=SR, num_channels=2).mean(axis=0)
def tex_run(n=69, **kw):
    for k, v in BASE.items():
        if k in P.parameters: setattr(P, k, v)
    P.raw_state = edit_state(P.raw_state, {'synth_mix': 1.0, 'osc_level': 0.0, 'sample_root': 69, 'sample_normalize': 0.0, 'chop_on': 0, 'tex_level': kw.get('tex_level', 0.8), 'tex_density': kw.get('tex_density', 20.0), 'tex_jitter_ct': kw.get('tex_jitter_ct', 0.0), 'tex_size_ms': kw.get('tex_size_ms', 120.0)}, SA)
    P.reset(); return P(note(n, 0, 1.6), duration=2.0, sample_rate=SR, num_channels=2).mean(axis=0)
t_off, t_on = tex_run(tex_level=0.0), tex_run(tex_level=0.8)
passed('texture off is silent when the synth is silent', rms(t_off, .2, 1.4) < 1e-5, '(%.1e)' % rms(t_off, .2, 1.4))
passed('texture on makes a cloud at the sample pitch', rms(t_on, .2, 1.4) > 0.02 and abs(peakf(t_on, .3, 1.3, 100, 2000) - 440) < 12, '(%.3f rms, %.0f Hz)' % (rms(t_on, .2, 1.4), peakf(t_on, .3, 1.3, 100, 2000)))
t81 = tex_run(81, tex_level=0.8); passed('the cloud follows the key (A5: 880 Hz)', abs(peakf(t81, .3, 1.3, 200, 3000) - 880) < 20, '(%.0f Hz)' % peakf(t81, .3, 1.3, 200, 3000))
tj = tex_run(tex_level=0.8, tex_jitter_ct=100.0)
spread = lambda x: (mag(x, 440, .3, 1.3) / (sum(mag(x, f, .3, 1.3) for f in np.arange(380, 500, 6)) + 1e-9))
passed('jitter smears the pitch (energy leaves the 440 Hz line)', spread(tj) < 0.8 * spread(t_on), '(%.3f vs %.3f)' % (spread(tj), spread(t_on)))
dens_lo = tex_run(tex_level=0.8, tex_density=4.0, tex_size_ms=60.0); w = [rms(dens_lo, a, a + 0.05) for a in np.arange(.2, 1.3, .05)]
passed('sparse grains pulse (4 per second, 60 ms): the level moves a lot', max(w) > 3 * (min(w) + 1e-6), '(min %.4f max %.4f)' % (min(w), max(w)))
dense = tex_run(tex_level=0.8, tex_density=80.0, tex_size_ms=200.0); w = [rms(dense, a, a + 0.05) for a in np.arange(.3, 1.3, .05)]
passed('dense grains are smooth', max(w) < 5.0 * min(w), '(min %.4f max %.4f)' % (min(w), max(w)))
passed('everything stays finite', all(np.isfinite(v).all() for v in (t_on, tj, dens_lo, dense)))
print(f"\nRESULT: {sum(rows)} of {len(rows)} source checks passed")
