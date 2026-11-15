"""Arp 2-HAND: exact events (DSP harness) plus an audio check through the built VST3.

With 2-HAND on, a held C-E-G arp (1/8, sine) gains a low C3 on the first step, E3 one 4-step period later, G3 after that;
with it off there is no energy at those pitches. The arp line itself does not change.
"""
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
    binary = str(Path(tmp) / 'dsp_twohand')
    subprocess.run(['clang++', '-std=c++17', '-O1', str(ROOT / 'tools/dsp_arp_twohand_test.cpp'), '-o', binary], check=True)
    r = subprocess.run([binary], capture_output=True, text=True); print(r.stdout.strip())
    check('DSP harness: all exact-event checks pass', r.returncode == 0)

def render(two_hand, duration=5.0):
    p = load_plugin(PATH)
    assert 'arp_two_hand' in p.parameters
    p.raw_state = edit_state(p.raw_state, dict(arp_on=1, arp_run=1, arp_hold=0, arp_div=7, arp_mode=0, arp_order=0, arp_octaves=1, arp_grouping=0,
                                                 arp_gate=0.5, arp_swing=0, arp_human=0, arp_two_hand=int(two_hand), chord_on=0, waveform=0, filter_on=0,
                                                 synth_mix=1, osc_level=0.3, master=1, attack_ms=0.5, decay_ms=0, sustain=1, release_ms=6, unison_voices=1))
    p.reset()
    ev = []
    for n in (60, 64, 67): ev += [(bytes([0x90, n, 100]), 0.5), (bytes([0x80, n, 0]), duration - 0.3)]
    return p(ev, duration=duration, sample_rate=SR, num_channels=2).mean(axis=0)

def band(x, t0, t1, note):
    s = x[int(t0 * SR):int(t1 * SR)]; f = 440 * 2 ** ((note - 69) / 12)
    X = np.abs(np.fft.rfft(s * np.hanning(len(s)), 1 << 16)); fr = np.fft.rfftfreq(1 << 16, 1 / SR)
    return float(X[(fr > f * 0.97) & (fr < f * 1.03)].max())

on, off = render(True), render(False)
C3, E3, G3 = 48, 52, 55
ref = max(band(on, 0.5, 3.5, n) for n in (60, 64, 67))   # arp note level for scale (strongest of C4 E4 G4 over 3 s)
# first low hit near 0.5 + 0.025 s, period 1.0 s at 120 BPM: C3 ~0.53-1.45, E3 ~1.53-2.45, G3 ~2.53-3.45
c3, e3, g3 = band(on, 0.7, 1.3, C3), band(on, 1.7, 2.3, E3), band(on, 2.7, 3.3, G3)
check('on: C3 sounds during the first period', c3 > 0.3 * ref)
check('on: E3 sounds during the second period', e3 > 0.3 * ref)
check('on: G3 sounds during the third period', g3 > 0.3 * ref)
check('on: E3 is absent in the first period (one low note at a time)', band(on, 0.7, 1.3, E3) < 0.1 * c3)
check('off: no low C3, E3 or G3', max(band(off, 0.7, 1.3, C3), band(off, 1.7, 2.3, E3), band(off, 2.7, 3.3, G3)) < 0.05 * ref)
arp_on, arp_off = max(band(on, 0.5, 3.5, n) for n in (60, 64, 67)), max(band(off, 0.5, 3.5, n) for n in (60, 64, 67))
check('the arp line keeps its level with 2-HAND on', abs(20 * np.log10(arp_on / arp_off)) < 1.0)
check('output is finite and stays under full scale', np.isfinite(on).all() and np.abs(on).max() < 1.0)
print(f"\nRESULT: {sum(rows)} of {len(rows)} arp 2-HAND checks passed")
sys.exit(0 if all(rows) else 1)
