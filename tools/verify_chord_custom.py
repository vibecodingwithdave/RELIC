"""Custom chord shapes per scale degree: exact checks (DSP harness), saved state, projects from before it existed, and the chords heard through the built VST3."""
from pathlib import Path
import subprocess, sys, tempfile
import numpy as np
from pedalboard import load_plugin
from state_tools import edit_state
ROOT = Path(__file__).resolve().parents[1]; PATH = str(ROOT / 'build/Relic_artefacts/Release/VST3/Relic.vst3'); SR = 44100
rows = []
def check(name, ok, note=""): rows.append(bool(ok)); print(f"  [{'PASS' if ok else 'FAIL'}] {name} {note}", flush=True)
with tempfile.TemporaryDirectory() as tmp:
    binary = str(Path(tmp) / 'dsp_cc')
    subprocess.run(['clang++', '-std=c++17', '-O1', str(ROOT / 'tools/dsp_chord_custom_test.cpp'), '-o', binary], check=True)
    r = subprocess.run([binary], capture_output=True, text=True); print(r.stdout.strip()); check('DSP harness: all exact checks pass', r.returncode == 0)
def mask(*iv): return sum(1 << i for i in iv)
BASE = dict(chord_on=1, chord_key=0, chord_scale=0, chord_type=0, chord_inversion=0, chord_voicing=0, chord_lock=1, chord_strum_ms=0, voice_lead=0, arp_on=0, waveform=0, filter_on=0, synth_mix=1,
            osc_level=0.2, master=1, attack_ms=0.5, decay_ms=0, sustain=1, release_ms=10, unison_voices=1, voices=8)
def plugin(params, drop=None): p = load_plugin(PATH); p.raw_state = edit_state(p.raw_state, {**BASE, **params}, None, None, drop); p.reset(); return p
def render(params, note=60, dur=1.2, drop=None):
    return plugin(params, drop)([(bytes([0x90, note, 100]), 0.0), (bytes([0x80, note, 0]), dur - 0.2)], duration=dur, sample_rate=SR, num_channels=2).mean(axis=0)
def has(x, midi):
    s = x[int(0.3 * SR):int(0.9 * SR)]; f = 440 * 2 ** ((midi - 69) / 12); w = np.hanning(len(s)); X = np.abs(np.fft.rfft(s * w, 1 << 17)); fr = np.fft.rfftfreq(1 << 17, 1 / SR)
    return float(2 * X[(fr > f * 0.985) & (fr < f * 1.015)].max() / w.sum())
def notes(x, cand): a = {m: has(x, m) for m in cand}; top = max(a.values()); return sorted(m for m, v in a.items() if v > 0.25 * top)

P0 = load_plugin(PATH)
check('defaults: Custom shapes are the major scale triads (major, minor, minor, major, major, minor, diminished)', [int(getattr(P0, f'chord_degree_{i}_shape')) for i in range(1, 8)] == [mask(0, 4, 7), mask(0, 3, 7), mask(0, 3, 7), mask(0, 4, 7), mask(0, 4, 7), mask(0, 3, 7), mask(0, 3, 6)])
Pa = plugin({'chord_mode': 2, 'chord_deg_1_shape': mask(0, 4, 7, 11, 14), 'chord_deg_7_shape': mask(0, 21)}); Pb = load_plugin(PATH); Pb.raw_state = Pa.raw_state
check('custom shapes (including bit 21) survive a save and restore', str(Pb.chord_mode) == 'Custom' and int(Pb.chord_degree_1_shape) == mask(0, 4, 7, 11, 14) and int(Pb.chord_degree_7_shape) == mask(0, 21), str(int(Pb.chord_degree_7_shape)))
CAND = list(range(55, 85))
x = render({'chord_mode': 2, 'chord_deg_1_shape': mask(0, 4, 7, 11)})
check('heard: degree I set to maj7 plays C E G B', notes(x, CAND) == [60, 64, 67, 71], str(notes(x, CAND)))
x = render({'chord_mode': 2, 'chord_deg_5_shape': mask(0, 4, 7, 10)}, note=67)
check('heard: degree V set to a dominant 7th plays G B D F', notes(x, CAND) == [67, 71, 74, 77], str(notes(x, CAND)))
x = render({'chord_mode': 2, 'chord_deg_2_shape': mask(0, 7, 12)}, note=62)
check('heard: degree II set to root, fifth, octave plays D A D', notes(x, CAND) == [62, 69, 74], str(notes(x, CAND)))
x = render({'chord_mode': 2})
check('heard: default Custom plays the C major triad', notes(x, CAND) == [60, 64, 67])
x = render({'chord_mode': 2, 'chord_deg_1_shape': mask(0, 4, 7), 'chord_deg_4_shape': mask(0, 4, 7, 11)}, note=65)
check('shapes are separate per degree (degree IV maj7 F A C E, degree I untouched)', notes(x, CAND) == [65, 69, 72, 76], str(notes(x, CAND)))
x_s = render({'chord_mode': 0, 'chord_deg_1_shape': mask(0, 5, 9)}); x_o = render({'chord_mode': 0}, drop=[f'chord_deg_{i}_shape' for i in range(1, 8)])
check('Scale mode ignores the custom shapes, and a project without them renders the same', np.allclose(x_s, x_o, atol=1e-6))
print(f"\nRESULT: {sum(rows)} of {len(rows)} custom chord checks passed"); sys.exit(0 if all(rows) else 1)
