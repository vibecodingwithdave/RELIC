"""Verify talk box, spectral gate, pitch delay and delay modes: DSP harness (dsp_fx3_test.cpp) plus the plugin (Pedalboard)."""
import subprocess, os, numpy as np
from pedalboard import load_plugin
HERE = os.path.dirname(os.path.abspath(__file__)); import tempfile; SCR = tempfile.gettempdir()
rows = []
def passed(n, ok, note=''): rows.append(bool(ok)); print(f"  [{'PASS' if ok else 'FAIL'}] {n} {note}")
print('== DSP harness')
subprocess.run(['clang++', '-std=c++17', '-O2', '-o', f'{SCR}/dsp_fx3', f'{HERE}/dsp_fx3_test.cpp'], check=True)
r = subprocess.run([f'{SCR}/dsp_fx3'], capture_output=True, text=True); print(r.stdout.strip().splitlines()[-1]); passed('DSP checks', r.returncode == 0)
P = load_plugin(str(__import__('pathlib').Path(__file__).resolve().parents[1] / 'build/Relic_artefacts/Release/VST3/Relic.vst3')); SR = 44100
BASE = dict(waveform='Saw', filter_on=False, tremolo_on=False, delay_on=False, comp_on=False, crush_on=False, pan_on=False, chorus_on=False, flanger_on=False, reverb_on=False,
            eq_on=False, tape_on=False, widener_on=False, wah_on=False, lofi_on=False, ott_on=False, pitch_delay_on=False, talk_on=False, spectral_gate_on=False,
            synth_mix=1.0, master=1.0, osc_level=0.3, voices=4, unison_voices=1, attack_ms=1, decay_ms=0, sustain=1.0, release_ms=20, delay_mode='Clean', delay_ping_pong=False,
            delay_time_ms=250.0, delay_feedback=0.6, delay_mix=1.0)
def render(events, dur, **kw):
    for k, v in {**BASE, **kw}.items(): assert k in P.parameters, k; setattr(P, k, v)
    P.reset(); return P(events, duration=dur, sample_rate=SR, num_channels=2).mean(axis=0)
def note(n=57, a=0.0, b=1.5): return [(bytes([0x90, n, 110]), a), (bytes([0x80, n, 0]), b)]
def bin_(x, f, a, b): s = x[int(a * SR):int(b * SR)]; w = np.hanning(len(s)); t = np.arange(len(s)) / SR; return float(abs(np.sum(s * w * np.exp(-2j * np.pi * f * t))) * 4 / len(s))
print('== plugin')
x = render(note(), 2.0, talk_on=True, talk_vowel=0.0, talk_mix=1.0); y = render(note(), 2.0)
passed('talk box A: formant at 800 Hz now stands out against 300 Hz', bin_(x, 880, 0.3, 1.2) > 1.5 * bin_(x, 330, 0.3, 1.2) and bin_(y, 330, .3, 1.2) > bin_(x, 330, .3, 1.2), '(%.3f vs %.3f)' % (bin_(x, 880, 0.3, 1.2), bin_(x, 330, 0.3, 1.2)))
x0 = render(note(), 2.0, talk_on=True, talk_vowel=0.0); x1 = render(note(), 2.0, talk_on=True, talk_vowel=0.5)
passed('moving the vowel from A to I changes the sound', bin_(x0, 2200, .3, 1.2) < 0.7 * bin_(x1, 2200, .3, 1.2) or np.abs(x0 - x1).max() > 0.05)
x = render(note(57, 0.0, 0.15), 2.5, pitch_delay_on=True, pitch_delay_time_ms=500.0, pitch_delay_feedback=0.6, pitch_delay_semi=12, pitch_delay_mix=1.0, waveform='Sine')
band = lambda x, lo, hi, a, b: max(bin_(x, f, a, b) for f in np.arange(lo, hi, 4))
passed('pitch delay +12: first echo is about an octave up (400-470 Hz)', band(x, 400, 470, 0.52, 0.7) > 3 * bin_(x, 220, 0.52, 0.7), '(%.3f vs %.3f)' % (band(x, 400, 470, .52, .7), bin_(x, 220, .52, .7)))
passed('pitch delay: second echo is two octaves up (880 Hz)', band(x, 780, 920, 1.05, 1.2) > 2 * band(x, 400, 470, 1.05, 1.2))
x = render(note(57, 0.0, 1.5), 2.5, spectral_gate_on=True, spectral_gate_threshold_db=-30.0, spectral_gate_depth=1.0, waveform='Sine')
passed('spectral gate: a clean tone passes (latency about 23 ms)', bin_(x, 220, 0.5, 1.3) > 0.2, '(%.3f)' % bin_(x, 220, 0.5, 1.3))
nz = render(note(), 2.0, spectral_gate_on=True, spectral_gate_threshold_db=-12.0, spectral_gate_depth=1.0, spectral_gate_mix=1.0); dry = render(note(), 2.0)
passed('spectral gate at -12 dB removes the weaker harmonics of a saw', bin_(nz, 220 * 9, 0.5, 1.3) < 0.3 * bin_(dry, 220 * 9, 0.5, 1.3), '(%.4f vs %.4f)' % (bin_(nz, 220 * 9, .5, 1.3), bin_(dry, 220 * 9, .5, 1.3)))
a = render(note(57, 0.0, 0.2), 2.0, delay_on=True, delay_mode='Clean', waveform='Saw'); b = render(note(57, 0.0, 0.2), 2.0, delay_on=True, delay_mode='Tape'); c = render(note(57, 0.0, 0.2), 2.0, delay_on=True, delay_mode='Wash')
hf = lambda x: bin_(x, 220 * 20, 0.8, 1.3)
passed('delay modes: tape repeats are darker than clean', hf(b) < 0.7 * hf(a), '(%.4f vs %.4f)' % (hf(b), hf(a)))
passed('delay modes: wash differs from clean and is finite', np.isfinite(c).all() and np.abs(c - a).max() > 0.01)
x = render(note(), 2.0, delay_on=True, delay_mode='Wash', pitch_delay_on=True, talk_on=True, spectral_gate_on=True, reverb_on=True)
passed('everything on at once stays finite and bounded', np.isfinite(x).all() and np.abs(x).max() <= 1.0)
print(f"\nRESULT: {sum(rows)} of {len(rows)} checks passed")
