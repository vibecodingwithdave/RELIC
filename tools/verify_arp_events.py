"""Arp EVENTS mode: exact events (DSP harness), saved state, old projects, and simultaneous notes / rests / long notes heard through the built VST3."""
from pathlib import Path
import subprocess, sys, tempfile
import numpy as np
from pedalboard import load_plugin
sys.path.insert(0, str(Path(__file__).resolve().parent))
from state_tools import edit_state
ROOT = Path(__file__).resolve().parents[1]; PATH = str(ROOT / 'build/Relic_artefacts/Release/VST3/Relic.vst3'); SR = 44100
rows = []
def check(name, ok, note=''): rows.append(bool(ok)); print(f"  [{'PASS' if ok else 'FAIL'}] {name} {note}", flush=True)
with tempfile.TemporaryDirectory() as tmp:
    binary = str(Path(tmp) / 'dsp_events')
    subprocess.run(['clang++', '-std=c++17', '-O1', str(ROOT / 'tools/dsp_arp_events_test.cpp'), '-o', binary], check=True)
    r = subprocess.run([binary], capture_output=True, text=True); print(r.stdout.strip()); check('DSP harness: all exact-event checks pass', r.returncode == 0)
BASE = dict(arp_on=1, arp_run=1, arp_hold=0, arp_div=7, arp_mode=9, arp_order=0, arp_octaves=1, arp_grouping=0, arp_gate=0.5, arp_swing=0, arp_human=0, arp_chance=1, arp_spray=0, arp_two_hand=0,
            chord_on=0, waveform=0, filter_on=0, synth_mix=1, osc_level=0.3, master=1, attack_ms=0.5, decay_ms=0, sustain=1, release_ms=6, unison_voices=1)
def ev(steps, length=None):
    d = {'arp_ev_length': length or len(steps)}
    for i, (mask, ln, vel) in enumerate(steps): d.update({f'arp_ev_{i + 1}_mask': mask, f'arp_ev_{i + 1}_len': ln, f'arp_ev_{i + 1}_vel': vel})
    return d
def plugin(params, drop=None): p = load_plugin(PATH); p.raw_state = edit_state(p.raw_state, {**BASE, **params}, None, None, drop); p.reset(); return p
def render(params, held=(60, 64, 67), duration=4.0, drop=None):
    p = plugin(params, drop); e = []
    for n in held: e += [(bytes([0x90, n, 100]), 0.5), (bytes([0x80, n, 0]), duration - 0.3)]
    return p(e, duration=duration, sample_rate=SR, num_channels=2).mean(axis=0)
def level(x, t0, t1, f):
    s = x[int(t0 * SR):int(t1 * SR)]; X = np.abs(np.fft.rfft(s * np.hanning(len(s)), 1 << 16)); fr = np.fft.rfftfreq(1 << 16, 1 / SR); return float(X[(fr > f * 0.97) & (fr < f * 1.03)].max())
F = {n: 440 * 2 ** ((n - 69) / 12) for n in (60, 64, 67)}
def heard(x, step, thr=0.25, start=0.525, dur=0.25):
    a = start + step * 0.25 + 0.015; lv = {n: level(x, a, a + 0.09, f) for n, f in F.items()}; m = max(lv.values()); return {n for n, v in lv.items() if v > thr * max(m, 1e-9) and m > 1.0}
P0 = load_plugin(PATH)
Pa = plugin({**ev([(5, 0, 90), (0, 0, 100), (2, 4, 40)]), 'arp_ev_40_mask': 4095, 'arp_ev_40_len': 16, 'arp_ev_40_vel': 7}); Pb = load_plugin(PATH); Pb.raw_state = Pa.raw_state
get = lambda P, n, k: int(getattr(P, f'arp_event_{n}_{k}'))
check('mode 9 is Events', str(Pb.arp_mode) == 'Events', str(Pb.arp_mode))
check('events (notes, length, velocity, in bar 3 too) survive a save and restore', Pb.arp_events_length == 3 and (get(Pb, 1, 'notes'), get(Pb, 3, 'length'), get(Pb, 3, 'velocity'), get(Pb, 40, 'notes'), get(Pb, 40, 'length'), get(Pb, 40, 'velocity')) == (5, 4, 40, 4095, 16, 7))
check('defaults: 16 steps, one rising note each (masks 1 2 4 ... 128), normal gate, velocity 100', P0.arp_events_length == 16 and [get(P0, i, 'notes') for i in range(1, 10)] == [1, 2, 4, 8, 16, 32, 64, 128, 1] and get(P0, 5, 'length') == 0 and get(P0, 5, 'velocity') == 100)
print('== HEARD ==')
x = render(ev([(0b101, 0, 100), (0, 0, 100), (0b010, 0, 100), (0b111, 0, 100)]))
check('a step with notes 0 and 2 sounds C and G together (not E)', heard(x, 0) == {60, 67}, str(heard(x, 0)))
check('the next step is a rest: silence', len(heard(x, 1)) == 0, str(heard(x, 1)))
check('a step with note 1 alone sounds E only', heard(x, 2) == {64}, str(heard(x, 2)))
check('a full chord step sounds C, E and G', heard(x, 3) == {60, 64, 67}, str(heard(x, 3)))
x = render(ev([(0b001, 4, 100), (0, 0, 100), (0, 0, 100), (0, 0, 100), (0b010, 0, 100), (0, 0, 100)]))
a = heard(x, 2); check('a length-4 event is still sounding two steps later (rests do not cut it)', 60 in a, str(a))
b = heard(x, 5); check('a normal-gate event is gone by the step after next', len(b) == 0, str(b))
loud = render(ev([(0b001, 0, 127), (0b001, 0, 30)]), duration=3.0); l0, l1 = level(loud, 0.54, 0.62, F[60]), level(loud, 0.79, 0.87, F[60])
check('velocity per event: 127 is much louder than 30', l0 > 2.2 * l1, f'({l0:.0f} vs {l1:.0f})')
print('== OTHER MODES, OLD PROJECTS ==')
a = render({**BASE, 'arp_mode': 8}); b = render({**BASE, 'arp_mode': 8, **ev([(7, 3, 20)])}); check('Custom mode ignores the event settings (bit-identical)', np.array_equal(a, b))
a = render({'arp_mode': 0}); b = render({'arp_mode': 0}, drop=['arp_ev_length'] + [f'arp_ev_{i}_{k}' for i in range(1, 65) for k in ('mask', 'len', 'vel')]); check('a project without the event settings renders the same as before', np.array_equal(a, b))
print(f'{sum(rows)}/{len(rows)} passed'); sys.exit(0 if all(rows) else 1)
