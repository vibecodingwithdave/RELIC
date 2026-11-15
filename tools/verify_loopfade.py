"""Verify the sampler loop crossfade (headless, Pedalboard)."""
import sys, os, numpy as np
sys.path.insert(0, os.path.dirname(__file__))
from pedalboard import load_plugin
from state_tools import edit_state
PATH = str(__import__('pathlib').Path(__file__).resolve().parents[1] / 'build/Relic_artefacts/Release/VST3/Relic.vst3')
ASSETS = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'test_assets'); SR = 44100
P = load_plugin(PATH); rows = []
def passed(n, ok, note=''): rows.append(bool(ok)); print(f"  [{'PASS' if ok else 'FAIL'}] {n} {note}")
BASE = dict(synth_mix=0.0, sample_gain=1.0, sample_root=69, sample_octave=0, sample_loop=1, sample_reverse=0, sample_normalize=0, sample_start=0.0, loop_start=0.05, loop_end=0.05 + 0.009659 / 2.0,
            voices=4, attack_ms=0.5, decay_ms=0.0, sustain=1.0, release_ms=30.0, filter_on=0, osc_level=1.0, master=1.0, unison_voices=1, chop_on=0, loop_xfade=0)
def render(wav='sine440.wav', notes=((69, 0.0, 1.8),), dur=2.0, **kw):
    P.raw_state = edit_state(P.raw_state, {**BASE, **kw}, os.path.join(ASSETS, wav)); P.reset()
    ev = []
    for n, a, b in notes: ev += [(bytes([0x90, n, 127]), a), (bytes([0x80, n, 0]), b)]
    return P(ev, duration=dur, sample_rate=SR, num_channels=2).mean(axis=0)
def jump(x): s = x[int(0.5 * SR):int(1.5 * SR)]; return float(np.abs(np.diff(s)).max())
def rms(x): s = x[int(0.5 * SR):int(1.5 * SR)]; return float(np.sqrt(np.mean(s ** 2)))
x0 = render(); j0 = jump(x0)
passed('without a fade the loop point clicks (a big sample-to-sample jump)', j0 > 0.3, '(%.2f)' % j0)
for i, name in ((1, '5 ms'), (2, '10 ms'), (3, '20 ms')):
    x = render(loop_xfade=i); passed(f'{name} fade: no click (jump well under the unfaded jump)', jump(x) < 0.3 * j0, '(%.3f vs %.2f)' % (jump(x), j0))
x = render(loop_xfade=3); passed('faded loop keeps playing at a steady level', rms(x) > 0.2 and abs(rms(render(loop_xfade=3, loop_end=0.05 + 0.019318 / 2.0)) - rms(x)) < 0.5, '(rms %.3f)' % rms(x))
x = render(loop_xfade=7); passed('a fade longer than half the loop is limited, not broken', np.isfinite(x).all() and rms(x) > 0.1, '(rms %.3f)' % rms(x))
x = render(loop_xfade=0, loop_end=1.0, loop_start=0.0, notes=((69, 0.0, 0.3),)); y = render(loop_xfade=3, loop_end=1.0, loop_start=0.0, notes=((69, 0.0, 0.3),))
passed('fade does not change non-looped behaviour', np.allclose(render(sample_loop=0, loop_xfade=0), render(sample_loop=0, loop_xfade=5), atol=1e-6))
# chop loop
CB = dict(chop_on=1, chop_mode=0, chop_count=3, sample_loop=1, loop_start=0.0, loop_end=1.0)
def chop(i, **kw):
    P.raw_state = edit_state(P.raw_state, {**BASE, **CB, **kw}, os.path.join(ASSETS, 'sine440.wav')); P.reset()
    return P([(bytes([0x90, 36, 110]), 0.0), (bytes([0x80, 36, 0]), 3.0)], duration=3.2, sample_rate=SR, num_channels=2).mean(axis=0)
c0 = chop(0, loop_xfade=0); c1 = chop(0, loop_xfade=4)
d = lambda x: float(np.abs(np.diff(x[int(1.5 * SR):int(2.8 * SR)])).max())
passed('chop slice loop: fade removes the loop click', d(c1) < 0.3 * d(c0) and d(c0) > 0.3 and rms(c1) > 0.1, '(%.3f vs %.3f)' % (d(c1), d(c0)))
print(f"\nRESULT: {sum(rows)} of {len(rows)} loop-fade checks passed")
