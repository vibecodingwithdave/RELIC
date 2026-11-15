"""Verify the two modulation envelopes (matrix sources Env 1 / Env 2), headless via Pedalboard."""
import re, numpy as np
from scipy.signal import hilbert
from pedalboard import load_plugin
P = load_plugin(str(__import__('pathlib').Path(__file__).resolve().parents[1] / 'build/Relic_artefacts/Release/VST3/Relic.vst3')); SR = 44100; rows = []
def passed(n, ok, note=''): rows.append(bool(ok)); print(f"  [{'PASS' if ok else 'FAIL'}] {n} {note}")
BASE = dict(waveform='Sine', filter_on=False, tremolo_on=False, delay_on=False, comp_on=False, crush_on=False, pan_on=False, chorus_on=False, flanger_on=False, reverb_on=False,
            synth_mix=1.0, master=1.0, osc_level=0.3, voices=4, unison_voices=1, attack_ms=1, decay_ms=0, sustain=1.0, release_ms=20, vib_on=False, porta_on=False,
            menv1_attack_ms=0.0, menv1_decay_ms=500.0, menv1_sustain=0.0, menv1_release_ms=300.0, menv2_attack_ms=0.0, menv2_decay_ms=1000.0, menv2_sustain=0.0, menv2_release_ms=300.0)
for k in (1, 2, 3, 4): BASE.update({f'mod{k}_src': 'None', f'mod{k}_dst': 'None', f'mod{k}_amt': 0.0})
def pn(k):
    k = re.sub(r'^menv(\d)_', r'mod_env_\1_', k); k = re.sub(r'^mod(\d)_', r'mod_\1_', k)
    return k.replace('_src', '_source').replace('_dst', '_destination').replace('_amt', '_amount').replace('vib_', 'vibrato_').replace('porta_', 'portamento_')
def render(events, dur, **kw):
    for k, v in {**BASE, **kw}.items():
        assert pn(k) in P.parameters, pn(k); setattr(P, pn(k), v)
    P.reset(); return P(events, duration=dur, sample_rate=SR, num_channels=2).mean(axis=0)
def note(n=57, a=0.0, b=2.4): return [(bytes([0x90, n, 100]), a), (bytes([0x80, n, 0]), b)]
def cents(x, t, f0=220.0, w=0.03):
    ph = np.unwrap(np.angle(hilbert(x))); f = np.diff(ph) * SR / (2 * np.pi); k = int(w * SR); f = np.convolve(f, np.ones(k) / k, 'same'); return 1200 * np.log2(f[int(t * SR)] / f0)
def bright(x, a, b): s = x[int(a * SR):int(b * SR)]; X = np.abs(np.fft.rfft(s * np.hanning(len(s)))); fr = np.fft.rfftfreq(len(s), 1 / SR); return float(X[fr > 1200].sum() / (X.sum() + 1e-12))
rms = lambda x, a, b: float(np.sqrt(np.mean(x[int(a * SR):int(b * SR)] ** 2)))
print('== ENV 1 -> pitch (amount 0.1 = 1.2 semitones at full)')
x = render(note(), 3.0, mod1_src='Env 1', mod1_dst='Pitch', mod1_amt=0.1)
c1, c2, c3 = cents(x, 0.05), cents(x, 0.25), cents(x, 0.9)
passed('decay: starts high, falls through the middle, ends at zero', 90 < c1 < 125 and 45 < c2 < 75 and abs(c3) < 8, '(%.0f, %.0f, %.0f ct)' % (c1, c2, c3))
x = render(note(), 3.0, mod1_src='Env 1', mod1_dst='Pitch', mod1_amt=0.1, menv1_sustain=0.5); c = cents(x, 1.5); passed('sustain 0.5 holds half of the amount (60 ct)', 50 < c < 70, '(%.0f ct)' % c)
x = render(note(), 3.0, mod1_src='Env 1', mod1_dst='Pitch', mod1_amt=0.1, menv1_attack_ms=400.0, menv1_decay_ms=10.0, menv1_sustain=1.0); ca, cb = cents(x, 0.1), cents(x, 0.6)
passed('attack 400 ms: slow rise to the full amount', ca < 45 and 100 < cb < 125, '(%.0f then %.0f ct)' % (ca, cb))
x = render(note(57, 0.0, 1.0), 2.2, mod1_src='Env 1', mod1_dst='Pitch', mod1_amt=0.1, menv1_sustain=1.0, menv1_release_ms=300.0, release_ms=700.0); cr0, cr1 = cents(x, 1.04), cents(x, 1.3)
passed('release: pitch falls back after the key comes up', cr0 > 80 and cr1 < 0.5 * cr0, '(%.0f then %.0f ct)' % (cr0, cr1))
x = render(note(57, 0.0, 0.2) + note(57, 1.5, 3.0), 3.4, mod1_src='Env 1', mod1_dst='Pitch', mod1_amt=0.1); c = cents(x, 1.55)
passed('a new note restarts the envelope', c > 90, '(%.0f ct)' % c)
print('== ENV 2 and routing')
x = render(note(), 3.0, mod1_src='Env 2', mod1_dst='Pitch', mod1_amt=0.1); a, b = cents(x, 0.25), cents(x, 0.9)
passed('env 2 has its own settings (1000 ms decay: 75 percent at 0.25 s, 10 percent at 0.9 s)', 80 < a < 100 and 5 < b < 30, '(%.0f, %.0f ct)' % (a, b))
x = render(note(), 3.0, mod1_src='Env 1', mod1_dst='Pitch', mod1_amt=0.1, mod2_src='Env 2', mod2_dst='Pitch', mod2_amt=-0.1); c = cents(x, 0.05)
passed('two envelopes in two slots add (env 1 up, env 2 down)', abs(c) < 20, '(%.0f ct)' % c)
x = render(note(), 3.0, menv1_decay_ms=500.0); passed('envelope set but unrouted does nothing', abs(cents(x, 0.05)) < 3)
x = render(note(), 3.0, mod1_src='Env 1', mod1_dst='Pitch', mod1_amt=-0.1); c = cents(x, 0.05); passed('negative amount bends down', c < -90, '(%.0f ct)' % c)
print('== ENV -> filter and level')
x = render(note(), 3.0, waveform='Saw', filter_on=True, cutoff_hz=300.0, mod1_src='Env 1', mod1_dst='Cutoff', mod1_amt=0.6); b0, b1 = bright(x, 0.01, 0.15), bright(x, 1.0, 1.4)
passed('env to cutoff: bright at the start, dull later', b0 > 2.5 * b1, '(%.3f vs %.3f)' % (b0, b1))
x = render(note(), 3.0, waveform='Saw', filter_on=True, cutoff_hz=300.0, mod1_src='Env 1', mod1_dst='Cutoff', mod1_amt=0.6, menv1_attack_ms=500.0, menv1_decay_ms=300.0, menv1_sustain=0.0)
passed('env attack opens the filter slowly', bright(x, 0.02, 0.15) < 0.5 * bright(x, 0.4, 0.55), '(%.3f then %.3f)' % (bright(x, 0.02, 0.15), bright(x, 0.4, 0.55)))
x = render(note(), 3.0, mod1_src='Env 1', mod1_dst='Level', mod1_amt=-1.0, menv1_decay_ms=800.0); e0, e1 = rms(x, 0.03, 0.1), rms(x, 1.2, 1.6)
passed('env to level (negative): swells in as the envelope falls', e1 > 3 * e0, '(%.3f then %.3f)' % (e0, e1))
print(f"\nRESULT: {sum(rows)} of {len(rows)} envelope checks passed")
