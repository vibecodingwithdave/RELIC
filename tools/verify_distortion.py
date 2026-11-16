"""Distortion shapes, band routing and mix on the tape stage (headless, Pedalboard) plus the DSP harness; old projects unchanged."""
import sys, subprocess, tempfile, numpy as np
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]; sys.path.insert(0, str(ROOT / 'tools'))
from pedalboard import load_plugin
from state_tools import edit_state
PATH = str(ROOT / 'build/Relic_artefacts/Release/VST3/Relic.vst3'); SR = 44100; rows = []
def check(name, ok, note=''): rows.append(bool(ok)); print(f"  [{'PASS' if ok else 'FAIL'}] {name} {note}", flush=True)
with tempfile.TemporaryDirectory() as tmp:
    b = str(Path(tmp) / 'dd'); subprocess.run(['clang++', '-std=c++17', '-O1', str(ROOT / 'tools/dsp_distortion_test.cpp'), '-o', b], check=True)
    r = subprocess.run([b], capture_output=True, text=True); print(r.stdout.strip().splitlines()[-1]); check('DSP harness (shapes, band split, mix)', r.returncode == 0)
BASE = dict(waveform=0, osc_level=0.5, unison_voices=1, synth_mix=1, filter_on=0, master=1.0, attack_ms=1, decay_ms=0, sustain=1, release_ms=60, voices=4, chord_on=0, arp_on=0, chop_on=0,
            osc2_on=0, osc3_on=0, vib_on=0, tape_on=1, tape_drive=0.8, tape_tone=1.0, tape_wobble=0.0)
def render(params=None, note=57, drop=None, dur=1.5):
    P = load_plugin(PATH); P.raw_state = edit_state(P.raw_state, {**BASE, **(params or {})}, None, None, drop); P.reset()
    return P([(bytes([0x90, note, 110]), 0.0), (bytes([0x80, note, 0]), dur - 0.1)], duration=dur, sample_rate=SR, num_channels=2).mean(axis=0)
def amp(x, f, t0=0.3, t1=1.2):
    s = x[int(t0 * SR):int(t1 * SR)]; X = np.abs(np.fft.rfft(s * np.hanning(len(s)), 1 << 17)); fr = np.fft.rfftfreq(1 << 17, 1 / SR); return float(X[(fr > f * 0.985) & (fr < f * 1.015)].max())
f0 = 440.0 * 2 ** ((57 - 69) / 12)
P0 = load_plugin(PATH)
check('defaults: Tape, Full band, bias 0, mix 1, split 200 / 2000 Hz', str(P0.tape_type) == 'Tape' and str(P0.tape_band) == 'Full' and P0.tape_bias == 0 and P0.tape_mix == 1 and abs(P0.tape_low_split_hz - 200) < 1 and abs(P0.tape_high_split_hz - 2000) < 1)
tape = render({}); soft = render({'tape_type': 1}); hard = render({'tape_type': 2}); fold = render({'tape_type': 3})
check('each type changes the sound (Tape, Soft, Hard, Fold renders all differ)', all(not np.allclose(a, b) for a, b in ((tape, soft), (tape, hard), (soft, hard), (hard, fold))))
h = lambda x, k: amp(x, f0 * k)
check('hard clip has more 3rd harmonic than soft clip', h(hard, 3) > 1.2 * h(soft, 3), f'({h(hard, 3):.0f} vs {h(soft, 3):.0f})')
tube0 = render({'tape_type': 4, 'tape_drive': 0.5}); tube1 = render({'tape_type': 4, 'tape_drive': 0.5, 'tape_bias': 0.7})
check('Tube bias raises the 2nd harmonic', h(tube1, 2) > 4 * h(tube0, 2), f'({h(tube1, 2):.0f} vs {h(tube0, 2):.0f})')
check('and leaves no DC offset', abs(np.mean(tube1[int(0.5 * SR):int(1.2 * SR)])) < 0.01 * np.max(np.abs(tube1)))
rect = render({'tape_type': 6, 'tape_drive': 0.5}); check('Rectify makes a strong 2nd harmonic', h(rect, 2) > 0.05 * h(rect, 1), f'({h(rect, 2):.0f} vs {h(rect, 1):.0f})')
print('== BAND AND MIX ==')
hi_note = 81; fh = 440.0 * 2 ** ((hi_note - 69) / 12)
full = render({'tape_type': 2}, note=hi_note); low = render({'tape_type': 2, 'tape_band': 1}, note=hi_note); high = render({'tape_type': 2, 'tape_band': 2}, note=hi_note)
check('Low band only leaves an 880 Hz tone clean (3rd harmonic far below the full-band version)', amp(low, fh * 3) < 0.15 * amp(full, fh * 3), f'({amp(low, fh * 3):.0f} vs {amp(full, fh * 3):.0f})')
check('Mid band only distorts it (880 Hz is in the middle band; 3rd harmonic stays)', amp(high, fh * 3) > 0.4 * amp(full, fh * 3), f'({amp(high, fh * 3):.0f} vs {amp(full, fh * 3):.0f})')
lown = render({'tape_type': 2, 'tape_band': 1}, note=45); fulln = render({'tape_type': 2}, note=45); f1 = 440.0 * 2 ** ((45 - 69) / 12)
check('Low band only does distort a low note (110 Hz)', amp(lown, f1 * 3) > 0.4 * amp(fulln, f1 * 3), f'({amp(lown, f1 * 3):.0f} vs {amp(fulln, f1 * 3):.0f})')
mix0 = render({'tape_type': 2, 'tape_mix': 0.0}); mixh = render({'tape_type': 2, 'tape_mix': 0.5})
check('Mix 0 leaves almost no distortion; Mix 0.5 is in between', amp(mix0, f0 * 3) < 0.1 * amp(hard, f0 * 3) and amp(mix0, f0 * 3) < amp(mixh, f0 * 3) < amp(hard, f0 * 3), f'({amp(mix0, f0 * 3):.0f}, {amp(mixh, f0 * 3):.0f}, {amp(hard, f0 * 3):.0f})')
print('== STATE AND OLD PROJECTS ==')
Pa = load_plugin(PATH); Pa.raw_state = edit_state(Pa.raw_state, {'tape_type': 5, 'tape_band': 2, 'tape_bias': 0.4, 'tape_mix': 0.6, 'tape_lo_hz': 300.0, 'tape_hi_hz': 3000.0}); Pb = load_plugin(PATH); Pb.raw_state = Pa.raw_state
check('the new settings survive a save and restore', str(Pb.tape_type) == 'Diode' and str(Pb.tape_band) == 'Mid' and abs(Pb.tape_bias - 0.4) < 0.01 and abs(Pb.tape_mix - 0.6) < 0.01 and abs(Pb.tape_low_split_hz - 300) < 2 and abs(Pb.tape_high_split_hz - 3000) < 20)
NEW = ['tape_type', 'tape_bias', 'tape_mix', 'tape_band', 'tape_lo_hz', 'tape_hi_hz']
check('a project without the new settings renders the same as the defaults (bit-identical)', np.array_equal(render({}), render({}, drop=NEW)))
print(f'{sum(rows)}/{len(rows)} passed'); sys.exit(0 if all(rows) else 1)
