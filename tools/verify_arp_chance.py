"""Arp CHANCE and SPRAY: exact events (DSP harness) plus audio through the built VST3. Relic's own design; the reference's equivalent knobs have no audible effect."""
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
    binary = str(Path(tmp) / 'dsp_chance')
    subprocess.run(['clang++', '-std=c++17', '-O1', str(ROOT / 'tools/dsp_arp_chance_test.cpp'), '-o', binary], check=True)
    r = subprocess.run([binary], capture_output=True, text=True); print(r.stdout.strip())
    check('DSP harness: all exact-event checks pass', r.returncode == 0)

fresh = load_plugin(PATH)
check('Chance defaults to 1 and Spray to 0', abs(fresh.arp_chance - 1.0) < 1e-6 and abs(fresh.arp_spray) < 1e-6)

def render(duration=5.0, **kw):
    p = load_plugin(PATH)
    p.raw_state = edit_state(p.raw_state, {**dict(arp_on=1, arp_run=1, arp_hold=0, arp_div=7, arp_mode=0, arp_order=0, arp_octaves=1, arp_grouping=0,
                                                 arp_gate=0.5, arp_swing=0, arp_human=0, arp_two_hand=0, arp_chance=1, arp_spray=0, chord_on=0, waveform=0, filter_on=0,
                                                 synth_mix=1, osc_level=0.3, master=1, attack_ms=0.5, decay_ms=0, sustain=1, release_ms=6, unison_voices=1), **kw})
    p.reset(); ev = []
    for n in (60, 64, 67): ev += [(bytes([0x90, n, 100]), 0.5), (bytes([0x80, n, 0]), duration - 0.3)]
    return p(ev, duration=duration, sample_rate=SR, num_channels=2).mean(axis=0)

def band(x, t0, t1, note):
    s = x[int(t0 * SR):int(t1 * SR)]; f = 440 * 2 ** ((note - 69) / 12)
    X = np.abs(np.fft.rfft(s * np.hanning(len(s)), 1 << 16)); fr = np.fft.rfftfreq(1 << 16, 1 / SR)
    return float(X[(fr > f * 0.97) & (fr < f * 1.03)].max())
def total(x, notes): return sum(band(x, 0.5, 4.5, n) for n in notes)

plain, none, sprayed, half = render(), render(arp_chance=0), render(arp_spray=1), render(arp_chance=0.5)
base, shifted = (60, 64, 67), (48, 52, 55, 72, 76, 79)
check('chance 0: the arp is silent', np.abs(none[int(0.6 * SR):int(4.5 * SR)]).max() < 1e-3 and np.abs(plain).max() > 0.05)
check('chance 0.5: sounds, but quieter overall than chance 1', 0.2 * np.mean(plain ** 2) < np.mean(half ** 2) < 0.9 * np.mean(plain ** 2))
check('spray 1: pattern notes move an octave (no energy left on C4 E4 G4)', total(sprayed, base) < 0.1 * total(sprayed, shifted))
check('spray 1: the octaves it lands on are audible', total(sprayed, shifted) > 0.5 * total(plain, base))
check('plain arp still sounds on C4 E4 G4', total(plain, base) > 10 * total(plain, shifted))
check('output is finite and under full scale', all(np.isfinite(x).all() and np.abs(x).max() < 1.0 for x in (plain, none, sprayed, half)))
print(f"\nRESULT: {sum(rows)} of {len(rows)} arp chance/spray checks passed")
sys.exit(0 if all(rows) else 1)
