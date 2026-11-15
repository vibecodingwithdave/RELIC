"""Verify XY pad / macro matrix sources and the keyboard split (headless, Pedalboard)."""
import os, sys, numpy as np
sys.path.insert(0, os.path.dirname(__file__))
from scipy.signal import hilbert
from pedalboard import load_plugin
from state_tools import edit_state
P = load_plugin(str(__import__('pathlib').Path(__file__).resolve().parents[1] / 'build/Relic_artefacts/Release/VST3/Relic.vst3')); SR = 44100; rows = []
ASSETS = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'test_assets')
def passed(n, ok, note=''): rows.append(bool(ok)); print(f"  [{'PASS' if ok else 'FAIL'}] {n} {note}")
BASE = dict(waveform='Sine', filter_on=False, tremolo_on=False, delay_on=False, comp_on=False, crush_on=False, pan_on=False, synth_mix=1.0, master=1.0, osc_level=0.3, voices=4, unison_voices=1,
            attack_ms=1, decay_ms=0, sustain=1.0, release_ms=20, mod_1_source='None', mod_1_destination='None', mod_1_amount=0.0, xy_x=0.5, xy_y=0.5, macro_1=0.0, macro_2=0.0,
            split_on=False, split_note=60, chop_on=False, chord_on=False, arp_on=False, lead_on=False)
def render(events, dur, **kw):
    for k, v in {**BASE, **kw}.items(): assert k in P.parameters, k; setattr(P, k, v)
    P.reset(); return P(events, duration=dur, sample_rate=SR, num_channels=2).mean(axis=0)
def note(n=57, a=0.0, b=1.5): return [(bytes([0x90, n, 110]), a), (bytes([0x80, n, 0]), b)]
def cents(x, t, f0=220.0, w=0.03):
    ph = np.unwrap(np.angle(hilbert(x))); f = np.diff(ph) * SR / (2 * np.pi); k = int(w * SR); f = np.convolve(f, np.ones(k) / k, 'same'); return 1200 * np.log2(f[int(t * SR)] / f0)
def mag(x, f, a, b): s = x[int(a * SR):int(b * SR)]; w = np.hanning(len(s)); t = np.arange(len(s)) / SR; return float(abs(np.sum(s * w * np.exp(-2j * np.pi * f * t))) * 4 / len(s))
print('== XY pad and macros as matrix sources (amount 0.1 to pitch = 120 cents at full)')
for src, kw, want in (('Macro 1', dict(macro_1=1.0), 120), ('Macro 1', dict(macro_1=0.5), 60), ('Macro 2', dict(macro_2=1.0), 120), ('XY X', dict(xy_x=1.0), 120), ('XY X', dict(xy_x=0.25), 30), ('XY Y', dict(xy_y=0.75), 90)):
    x = render(note(), 1.5, mod_1_source=src, mod_1_destination='Pitch', mod_1_amount=0.1, **kw); c = cents(x, 0.6)
    passed(f'{src} at {list(kw.values())[0]}: pitch {want} cents up', abs(c - want) < 8, '(%.0f ct)' % c)
x = render(note(), 1.5, mod_1_source='Macro 1', mod_1_destination='Pitch', mod_1_amount=0.1, macro_1=0.0); passed('macro at zero: no change', abs(cents(x, 0.6)) < 4)
x = render(note(), 1.5, mod_1_source='XY Y', mod_1_destination='Level', mod_1_amount=-1.0, xy_y=1.0); y = render(note(), 1.5, mod_1_source='XY Y', mod_1_destination='Level', mod_1_amount=-1.0, xy_y=0.0)
passed('XY Y to level (negative): top of the pad is quiet, bottom is full', np.abs(x).max() < 0.2 * np.abs(y).max())
print('== keyboard split (sample plays 440 Hz at its root note 57, synth is a sine at the key)')
def two(n, **kw):
    P.raw_state = edit_state(P.raw_state, {'synth_mix': 0.5, 'sample_root': 57, 'sample_gain': 1.0, 'sample_normalize': 0.0, 'sample_loop': 1, 'chop_on': 0, 'split_on': kw.get('split_on', 0), 'split_note': 60}, os.path.join(ASSETS, 'sine440.wav'))
    P.reset(); return P(note(n, 0, 1.6), duration=2.0, sample_rate=SR, num_channels=2).mean(axis=0)
off = two(57, split_on=0); passed('split off: both layers sound (220 Hz synth and 440 Hz sample)', mag(off, 220, .3, 1.3) > 0.03 and mag(off, 440, .3, 1.3) > 0.1)
lo = two(57, split_on=1); passed('below the split: sample only (440 Hz, no 220 Hz)', mag(lo, 440, .3, 1.3) > 0.2 and mag(lo, 220, .3, 1.3) < 0.01, '(%.3f / %.3f)' % (mag(lo, 440, .3, 1.3), mag(lo, 220, .3, 1.3)))
f72 = 440 * 2 ** (3 / 12); hi = two(72, split_on=1); passed('from the split up: synth only (523 Hz, no sample at 1047 Hz)', mag(hi, f72, .3, 1.3) > 0.1 and mag(hi, 440 * 2 ** (15 / 12), .3, 1.3) < 0.01, '(%.3f / %.3f)' % (mag(hi, f72, .3, 1.3), mag(hi, 440 * 2 ** (15 / 12), .3, 1.3)))
e = two(60, split_on=1); passed('the split note itself belongs to the synth (60 >= 60)', mag(e, 261.6, .3, 1.3) > 0.1 and mag(e, 440 * 2 ** (3 / 12), .3, 1.3) < 0.01)
print(f"\nRESULT: {sum(rows)} of {len(rows)} batch-4 checks passed")
