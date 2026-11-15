"""Verify filter models, filter source routing and LFO key sync (headless, Pedalboard)."""
import os, sys, re, numpy as np
sys.path.insert(0, os.path.dirname(__file__))
from scipy.signal import hilbert
from pedalboard import load_plugin
from state_tools import edit_state
P = load_plugin(str(__import__('pathlib').Path(__file__).resolve().parents[1] / 'build/Relic_artefacts/Release/VST3/Relic.vst3')); SR = 44100; rows = []
ASSETS = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'test_assets')
def passed(n, ok, note=''): rows.append(bool(ok)); print(f"  [{'PASS' if ok else 'FAIL'}] {n} {note}")
BASE = dict(waveform='Saw', filter_on=True, filter_type='LP', filter_model='Clean', filter_source='All', cutoff_hz=20000.0, resonance=0.0, tremolo_on=False, delay_on=False, comp_on=False, crush_on=False,
            pan_on=False, synth_mix=1.0, master=1.0, osc_level=0.3, voices=4, unison_voices=1, attack_ms=1, decay_ms=0, sustain=1.0, release_ms=20, lfo_1_key_sync=False, lfo_2_key_sync=False,
            mod_1_source='None', mod_1_destination='None', mod_1_amount=0.0)
def render(events, dur, **kw):
    for k, v in {**BASE, **kw}.items(): assert k in P.parameters, k; setattr(P, k, v)
    P.reset(); return P(events, duration=dur, sample_rate=SR, num_channels=2).mean(axis=0)
def note(n=57, a=0.0, b=1.5): return [(bytes([0x90, n, 110]), a), (bytes([0x80, n, 0]), b)]
def bin_(x, f, a, b): s = x[int(a * SR):int(b * SR)]; w = np.hanning(len(s)); t = np.arange(len(s)) / SR; return float(abs(np.sum(s * w * np.exp(-2j * np.pi * f * t))) * 4 / len(s))
db = lambda a, b: 20 * np.log10(max(a, 1e-9) / max(b, 1e-9))
print('== models')
c = render(note(), 2.0, cutoff_hz=1000.0); s = render(note(), 2.0, cutoff_hz=1000.0, filter_model='SP-12')
passed('SP-12 (two stages) rolls off steeper than clean: 4.4 kHz at least 8 dB lower', db(bin_(c, 4400, .3, 1.2), bin_(s, 4400, .3, 1.2)) > 8, '(%.1f dB)' % db(bin_(c, 4400, .3, 1.2), bin_(s, 4400, .3, 1.2)))
passed('SP-12 keeps the fundamental at the same level (within 3 dB)', abs(db(bin_(c, 220, .3, 1.2), bin_(s, 220, .3, 1.2))) < 3)
c = render(note(69), 2.0, waveform='Sine', osc_level=0.9); o = render(note(69), 2.0, waveform='Sine', osc_level=0.9, filter_model='OBX')
passed('OBX saturates: a hot sine gains a 3rd harmonic that clean lacks', bin_(o, 1320, .3, 1.2) > 5 * bin_(c, 1320, .3, 1.2) + 1e-4, '(%.4f vs %.4f)' % (bin_(o, 1320, .3, 1.2), bin_(c, 1320, .3, 1.2)))
c = render(note(127), 2.0, waveform='Sine', osc_level=0.5); m = render(note(127), 2.0, waveform='Sine', osc_level=0.5, filter_model='MPC60')
f = 440 * 2 ** ((127 - 69) / 12)
passed('MPC60 softens the top: %.0f Hz is at least 1.5 dB down' % f, db(bin_(c, f, .3, 1.2), bin_(m, f, .3, 1.2)) > 1.5, '(%.1f dB)' % db(bin_(c, f, .3, 1.2), bin_(m, f, .3, 1.2)))
c = render(note(), 2.0, waveform='Sine', osc_level=0.05); m = render(note(), 2.0, waveform='Sine', osc_level=0.05, filter_model='SP-12')
passed('12-bit models add a quantisation floor on a very quiet signal', np.abs(m - c)[int(0.3 * SR):int(1.3 * SR)].max() > 1e-5 or bin_(m, 660, .3, 1.2) > bin_(c, 660, .3, 1.2))
for md in ('Clean', 'OBX', 'SP-12', 'MPC60'):
    x = render(note(), 2.0, filter_model=md, cutoff_hz=800.0, resonance=0.9); passed(f'{md}: finite and bounded at high resonance', np.isfinite(x).all() and np.abs(x).max() <= 1.0)
x = render(note(), 2.0, filter_model='MPC60', filter_type='HP', cutoff_hz=3000.0); passed('models work with the other filter types (MPC60 high-pass)', bin_(x, 220, .3, 1.2) < 0.2 * bin_(render(note(), 2.0), 220, .3, 1.2) + 1e-6)
print('== source routing (synth sine 220 Hz, sample 440 Hz, LP at 300 Hz)')
def two(src, **kw):
    P.raw_state = edit_state(P.raw_state, {'synth_mix': 0.5, 'sample_root': 57, 'sample_gain': 1.0, 'sample_normalize': 0.0, 'sample_loop': 1}, os.path.join(ASSETS, 'sine440.wav'))
    return render(note(57, 0, 1.6), 2.0, waveform='Sine', osc_level=0.6, filter_source=src, cutoff_hz=300.0, synth_mix=0.5, **kw)
for k in ('chop_on',): pass
a, sy, sm = two('All'), two('Synth'), two('Sample')
v = lambda x, f: bin_(x, f, .4, 1.3)
passed('All: the 440 Hz sample is filtered down', db(v(sy, 440), v(a, 440)) > 6, '(%.1f dB below the unfiltered sample)' % db(v(sy, 440), v(a, 440)))
passed('Synth only: the sample stays bright, the 220 Hz synth passes', v(sy, 440) > 0.1 and v(sy, 220) > 0.05)
passed('Sample only: the sample is filtered, the synth is untouched', db(v(sy, 440), v(sm, 440)) > 6 and abs(db(v(sy, 220), v(sm, 220))) < 3)
print('== LFO key sync')
mk = lambda ks: render(note(57, 0.0, 0.3) + note(57, 0.62, 1.8), 2.0, waveform='Sine', filter_on=False, lfo_1_rate_hz=1.3, lfo_1_sync='Free', lfo_1_shape='Sine', mod_1_source='LFO 1', mod_1_destination='Pitch', mod_1_amount=0.05, lfo_1_key_sync=ks)
def dev_at(x, t, w=0.03):
    ph = np.unwrap(np.angle(hilbert(x))); f = np.diff(ph) * SR / (2 * np.pi); k = int(w * SR); f = np.convolve(f, np.ones(k) / k, 'same'); return 1200 * np.log2(f[int(t * SR)] / 220.0)
d_free, d_sync = dev_at(mk(False), 0.65), dev_at(mk(True), 0.65)
passed('free-running: the second note starts mid-cycle (large pitch offset)', abs(d_free) > 25, '(%.0f ct)' % d_free)
passed('key sync: the second note starts from the LFO start (near zero)', abs(d_sync) < 15, '(%.0f ct)' % d_sync)
print(f"\nRESULT: {sum(rows)} of {len(rows)} filter/key-sync checks passed")
