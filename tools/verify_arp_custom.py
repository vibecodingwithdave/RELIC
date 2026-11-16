"""Arp CUSTOM mode: exact events (DSP harness), saved state, projects from before it existed, and the custom line heard through the built VST3."""
from pathlib import Path
import subprocess, sys, tempfile
import numpy as np
from pedalboard import load_plugin
from state_tools import edit_state
ROOT = Path(__file__).resolve().parents[1]; PATH = str(ROOT / 'build/Relic_artefacts/Release/VST3/Relic.vst3'); SR = 44100
rows = []
def check(name, ok): rows.append(bool(ok)); print(f"  [{'PASS' if ok else 'FAIL'}] {name}", flush=True)
with tempfile.TemporaryDirectory() as tmp:
    binary = str(Path(tmp) / 'dsp_custom')
    subprocess.run(['clang++', '-std=c++17', '-O1', str(ROOT / 'tools/dsp_arp_custom_test.cpp'), '-o', binary], check=True)
    r = subprocess.run([binary], capture_output=True, text=True); print(r.stdout.strip()); check('DSP harness: all exact-event checks pass', r.returncode == 0)

BASE = dict(arp_on=1, arp_run=1, arp_hold=0, arp_div=7, arp_mode=8, arp_order=0, arp_octaves=1, arp_grouping=0, arp_gate=0.5, arp_swing=0, arp_human=0, arp_chance=1, arp_spray=0, arp_two_hand=0,
            chord_on=0, waveform=0, filter_on=0, synth_mix=1, osc_level=0.3, master=1, attack_ms=0.5, decay_ms=0, sustain=1, release_ms=6, unison_voices=1)
def pat(values): return {'arp_pat_length': len(values), **{f'arp_pat_{i + 1}': v for i, v in enumerate(values)}}
def plugin(params): p = load_plugin(PATH); p.raw_state = edit_state(p.raw_state, {**BASE, **params}); p.reset(); return p
def render(params, held=(60, 64, 67), duration=5.0):
    p = plugin(params); ev = []
    for n in held: ev += [(bytes([0x90, n, 100]), 0.5), (bytes([0x80, n, 0]), duration - 0.3)]
    return p(ev, duration=duration, sample_rate=SR, num_channels=2).mean(axis=0)
def level(x, t0, t1, f):
    s = x[int(t0 * SR):int(t1 * SR)]; X = np.abs(np.fft.rfft(s * np.hanning(len(s)), 1 << 16)); fr = np.fft.rfftfreq(1 << 16, 1 / SR); return float(X[(fr > f * 0.97) & (fr < f * 1.03)].max())
def heard(x, steps, notes, step=0.25, start=0.525):
    out = []
    for k in range(steps):
        a = start + k * step + 0.015; out.append(int(np.argmax([level(x, a, a + 0.09, 440 * 2 ** ((n - 69) / 12)) for n in notes])))
    return out
def freq_notes(*ns): return list(ns)

P0 = load_plugin(PATH)
check('defaults: length 16, steps count 0 1 2 3 4 5 6 7 and repeat; the mode list ends with Custom', P0.arp_pattern_length == 16 and [int(getattr(P0, f'arp_pattern_step_{i + 1}')) for i in range(16)] == [i % 8 for i in range(16)]
      and str(P0.arp_mode) in ('Up',))
Pa = plugin({**pat([3, -2, 7, 0, 5]), 'arp_pat_40': -9}); Pb = load_plugin(PATH); Pb.raw_state = Pa.raw_state
check('the pattern (length, steps, a step in bar 3) survives a save and restore', Pb.arp_pattern_length == 5 and [int(getattr(Pb, f'arp_pattern_step_{i}')) for i in (1, 2, 3, 4, 5, 40)] == [3, -2, 7, 0, 5, -9] and str(Pb.arp_mode) == 'Custom')
old = {k: v for k, v in BASE.items()}; old['arp_mode'] = 0
x_def = render({**old}); x_old = render({**old}, duration=5.0)
check('Up mode is unaffected by the Custom parameters (same render with a different custom pattern)', np.allclose(render({**old, **pat([2, 2, 2, 2])}), x_def, atol=1e-6))

C, E, G = 60, 64, 67; N = [C, E, G, 72, 55]
x = render(pat([0, 1, 2, 1]))
check('heard through the plugin: pattern 0 1 2 1 plays C E G E C E G E', heard(x, 8, [C, E, G]) == [0, 1, 2, 1, 0, 1, 2, 1])
x = render(pat([2, 0, 2, 1, 1, 1]))
check('heard through the plugin: pattern 2 0 2 1 1 1 (six steps) repeats', heard(x, 12, [C, E, G]) == [2, 0, 2, 1, 1, 1] * 2)
x = render(pat([3, 3, 0, 0]))
check('heard through the plugin: number 3 plays C an octave up', heard(x, 4, [C, 72]) == [1, 1, 0, 0])
v = [0] * 16 + [2] * 16; x = render(pat(v), duration=10.5); h = heard(x, 36, [C, E, G])
check('heard through the plugin: a 32-step pattern spans two bars (16 on C, 16 on G, then again)', h == [0] * 16 + [2] * 16 + [0] * 4)
print(f"\nRESULT: {sum(rows)} of {len(rows)} custom arp checks passed"); sys.exit(0 if all(rows) else 1)
