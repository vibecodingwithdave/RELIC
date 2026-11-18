"""A released key must stop sounding even if a setting changed while it was held (a preset switch, for instance). Run: python tools/verify_stuck_notes.py"""
import sys, numpy as np
from pedalboard import load_plugin
PATH = 'build/Relic_artefacts/Release/VST3/Relic.vst3'; SR = 44100
def tail_rms(setup, flip, notes=(60, 64)):
    P = load_plugin(PATH); setup(P)
    P([(bytes([0x90, n, 100]), 0.0) for n in notes], duration=1.0, sample_rate=SR, num_channels=2, reset=False)
    flip(P)
    y = P([(bytes([0x80, n, 0]), 0.0) for n in notes], duration=4.0, sample_rate=SR, num_channels=2, reset=False)
    return float(np.sqrt(np.mean(y[:, -SR:] ** 2)))
def base(P): P.release_ms = 100.0; P.reverb_on = False; P.delay_on = False
def st(**kw):
    def f(P):
        for k, v in kw.items(): setattr(P, k, v)
    return f
def chord(P): base(P); P.chord_on = True
def arp(P): base(P); P.arp_on = True
def lead(P): base(P); P.lead_on = True
cases = [('no change', base, st()), ('lead switched on while two keys are held', base, st(lead_on=True)), ('lead switched off while held', lead, st(lead_on=False)),
         ('voices 8 -> 1 while held', base, st(voices=1)), ('split switched on while held', base, st(split_on=True)), ('chords on while held', base, st(chord_on=True)),
         ('chords off while held', chord, st(chord_on=False)), ('chord type changed while held', chord, st(chord_type='7th')), ('arp on while held', base, st(arp_on=True)), ('arp off while held', arp, st(arp_on=False))]
ok = 0
for name, setup, flip in cases:
    r = tail_rms(setup, flip); good = r < 1e-4; ok += good
    print(f"  [{'PASS' if good else 'FAIL'}] {name}: level 4 s after release {r:.5f}")
print(f"\n{ok} of {len(cases)} checks passed"); print('RESULT: PASS' if ok == len(cases) else 'RESULT: FAIL'); sys.exit(0 if ok == len(cases) else 1)
