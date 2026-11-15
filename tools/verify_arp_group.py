"""Arp GROUPING in the plain modes: exact events (DSP harness) plus the grouped sequence heard through the built VST3. Relic's own design."""
from pathlib import Path
import subprocess, sys, tempfile
import numpy as np
from pedalboard import load_plugin
from state_tools import edit_state

ROOT = Path(__file__).resolve().parents[1]
PATH = str(ROOT / 'build/Relic_artefacts/Release/VST3/Relic.vst3')
SR = 44100
rows = []
def check(name, ok):
    rows.append(bool(ok)); print(f"  [{'PASS' if ok else 'FAIL'}] {name}", flush=True)

with tempfile.TemporaryDirectory() as tmp:
    binary = str(Path(tmp) / 'dsp_group')
    subprocess.run(['clang++', '-std=c++17', '-O1', str(ROOT / 'tools/dsp_arp_group_test.cpp'), '-o', binary], check=True)
    r = subprocess.run([binary], capture_output=True, text=True); print(r.stdout.strip())
    check('DSP harness: all exact-event checks pass', r.returncode == 0)

def render(mode, grouping, duration=5.0):
    p = load_plugin(PATH)
    p.raw_state = edit_state(p.raw_state, dict(arp_on=1, arp_run=1, arp_hold=0, arp_div=7, arp_mode=mode, arp_order=0, arp_octaves=1, arp_grouping=grouping, arp_gate=0.5,
                                                 arp_swing=0, arp_human=0, arp_chance=1, arp_spray=0, arp_two_hand=0, chord_on=0, waveform=0, filter_on=0, synth_mix=1,
                                                 osc_level=0.3, master=1, attack_ms=0.5, decay_ms=0, sustain=1, release_ms=6, unison_voices=1))
    p.reset(); ev = []
    for n in (60, 64, 67): ev += [(bytes([0x90, n, 100]), 0.5), (bytes([0x80, n, 0]), duration - 0.3)]
    return p(ev, duration=duration, sample_rate=SR, num_channels=2).mean(axis=0)

def level(x, t0, t1, note):
    s = x[int(t0 * SR):int(t1 * SR)]; f = 440 * 2 ** ((note - 69) / 12)
    X = np.abs(np.fft.rfft(s * np.hanning(len(s)), 1 << 16)); fr = np.fft.rfftfreq(1 << 16, 1 / SR)
    return float(X[(fr > f * 0.97) & (fr < f * 1.03)].max())

def heard(x, steps, step=0.25, start=0.525):
    out = []
    for k in range(steps):
        a = start + k * step + 0.015; b = a + 0.09
        out.append(int(np.argmax([level(x, a, b, n) for n in (60, 64, 67)])))
    return out      # 0 = C4, 1 = E4, 2 = G4

plain, grouped = render(0, 0), render(0, 4)
check('plain Up still plays C E G repeating (no grouping)', heard(plain, 9) == [0, 1, 2] * 3)
check('Up 4-4-4-4 through the plugin: C E G C, restarting every 4 steps', heard(grouped, 12) == [0, 1, 2, 0] * 3)
check('Down 4-4-4-4 through the plugin: G E C G', heard(render(1, 4), 8) == [2, 1, 0, 2] * 2)
check('Up 5-5-6 through the plugin: C E G C E | C E G C E', heard(render(0, 1), 10) == [0, 1, 2, 0, 1] * 2)
check('output is finite and under full scale', all(np.isfinite(x).all() and np.abs(x).max() < 1.0 for x in (plain, grouped)))
print(f"\nRESULT: {sum(rows)} of {len(rows)} arp grouping checks passed")
sys.exit(0 if all(rows) else 1)
