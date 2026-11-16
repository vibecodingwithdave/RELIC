"""External wavetables (headless, Pedalboard, synthetic test files written to a temp folder): loading, frame morphing, band limiting, every Wavetable oscillator,
saved state, missing or unreadable files, and projects saved before this existed. No collection assets are used."""
import sys, struct, wave, tempfile, subprocess, numpy as np
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]; sys.path.insert(0, str(ROOT / 'tools'))
from pedalboard import load_plugin
from state_tools import edit_state
PATH = str(ROOT / 'build/Relic_artefacts/Release/VST3/Relic.vst3'); SR = 44100
rows = []
def check(name, ok, note=''): rows.append(bool(ok)); print(f"  [{'PASS' if ok else 'FAIL'}] {name} {note}", flush=True)
TMP = tempfile.TemporaryDirectory(); T = Path(TMP.name)
with tempfile.TemporaryDirectory() as tmp:
    binary = str(Path(tmp) / 'dsp_wt')
    subprocess.run(['clang++', '-std=c++17', '-O1', str(ROOT / 'tools/dsp_wavetable_user_test.cpp'), '-o', binary], check=True)
    r = subprocess.run([binary], capture_output=True, text=True); print(r.stdout.strip()); check('DSP harness: all exact checks pass', r.returncode == 0)

def cyc(n, h, a=0.5): return a * np.sin(2 * np.pi * h * np.arange(n) / n)
def write(name, x, extra_chunk=None):
    x = np.clip(np.asarray(x), -1, 1); data = (x * 32767).astype('<i2').tobytes(); p = T / name
    fmt = struct.pack('<HHIIHH', 1, 1, SR, SR * 2, 2, 16)
    body = b'WAVE' + b'fmt ' + struct.pack('<I', 16) + fmt
    if extra_chunk: body += extra_chunk
    body += b'data' + struct.pack('<I', len(data)) + data
    p.write_bytes(b'RIFF' + struct.pack('<I', len(body)) + body); return str(p)
sine1 = write('sine_one.wav', cyc(2048, 1))
two = write('two_frames.wav', np.concatenate([cyc(2048, 1), cyc(2048, 3)]))
saw = write('saw_one.wav', 0.8 * (1 - 2 * np.arange(2048) / 2048))
odd = write('odd_cycle.wav', cyc(600, 1))
hint = write('hint_1024.wav', np.concatenate([cyc(1024, k) for k in (1, 2, 3, 4)]), b'clm ' + struct.pack('<I', 20) + b'<!>1024 10000000    ')
junk = T / 'junk.wav'; junk.write_bytes(b'this is not audio at all'); junk = str(junk)
(T / 'bank').mkdir(); write('bank/a_first.wav', cyc(2048, 2)); write('bank/b_second.wav', cyc(2048, 3)); write('bank/c_third.wav', cyc(2048, 4)); (T / 'bank/notes.txt').write_text('x')

BASE = dict(waveform=4, pulse_width=0.5, osc_level=0.8, unison_voices=1, synth_mix=1, filter_on=0, master=1.0, attack_ms=0.5, decay_ms=0, sustain=1, release_ms=20, voices=4,
            osc2_on=0, osc3_on=0, chord_on=0, arp_on=0, chop_on=0)
def plugin(params=None, wt=None, drop=None, attrs=None):
    P = load_plugin(PATH); a = dict(attrs or {}); 
    if wt: a['wavetablePath'] = wt
    P.raw_state = edit_state(P.raw_state, {**BASE, **(params or {})}, None, a or None, drop); P.reset(); return P
def render(P, note=69, hold=1.2, dur=1.4, sr=SR):
    return P([(bytes([0x90, note, 127]), 0.0), (bytes([0x80, note, 0]), hold)], duration=dur, sample_rate=sr, num_channels=2).mean(axis=0)
def amp(x, f, sr=SR, a=0.3, b=1.0):
    s = x[int(a * sr):int(b * sr)]; w = np.hanning(len(s)); X = np.abs(np.fft.rfft(s * w, 1 << 17)); fr = np.fft.rfftfreq(1 << 17, sr and 1 / sr)
    return float(2 * X[(fr > f * 0.985) & (fr < f * 1.015)].max() / w.sum())
def rms(x): s = x[int(0.3 * SR):int(1.0 * SR)]; return float(np.sqrt(np.mean(s ** 2)))
def offharm(x, f0, sr=SR, a=0.3, b=1.0):
    """fraction of spectral energy that is not at a multiple of f0 (aliasing shows up here)"""
    s = x[int(a * sr):int(b * sr)]; w = np.hanning(len(s)); X = np.abs(np.fft.rfft(s * w, 1 << 17)) ** 2; fr = np.fft.rfftfreq(1 << 17, 1 / sr)
    d = np.abs(fr / f0 - np.round(fr / f0)) * f0; near = d < f0 * 0.04; return float(X[~near].sum() / X.sum())

P0 = load_plugin(PATH)
check('default: "Wavetable From File" is off', not bool(P0.wavetable_from_file))
base = render(plugin()); off = render(plugin(wt=sine1)); miss = render(plugin(dict(wt_use_file=1), wt=str(T / 'nope.wav')))
check('a loaded file is not used while FILE is off (output equals the built-in bank)', np.array_equal(base, off))
check('FILE on with no usable file falls back to the built-in bank', np.array_equal(base, render(plugin(dict(wt_use_file=1)))) and np.array_equal(base, miss))
check('an unreadable file falls back to the built-in bank without a crash', np.array_equal(base, render(plugin(dict(wt_use_file=1), wt=junk))))
Pa = plugin(dict(wt_use_file=1), wt=sine1); Pb = load_plugin(PATH); Pb.raw_state = Pa.raw_state
check('the FILE setting survives a save and restore', bool(Pb.wavetable_from_file))
check('the file path is saved with the project (same sound after restore)', np.allclose(render(Pa), render(Pb), atol=1e-6))
x = render(Pa)
check('a one-frame sine file plays a pure sine at the key\'s pitch', amp(x, 440) > 0.1 and amp(x, 880) < 0.01 * amp(x, 440) and amp(x, 1320) < 0.01 * amp(x, 440), f"{amp(x, 440):.3f}")
check('the built-in bank at the same settings is not a pure sine (so the file really replaced it)', amp(base, 880) > 0.02 * amp(base, 440))
x57 = render(Pa, note=57); check('pitch tracks the keyboard (A3 is 220 Hz)', amp(x57, 220) > 0.1 and amp(x57, 440) < 0.01 * amp(x57, 220))
check('loudness is comparable to the built-in bank', 0.4 < rms(x) / rms(base) < 2.5, f"{rms(x) / rms(base):.2f}")

# frame morphing with the Width control
for pw, want, other in ((0.05, 440, 1320), (0.95, 1320, 440)):
    y = render(plugin(dict(wt_use_file=1, pulse_width=pw), wt=two))
    check(f'two frames: Width {pw} plays frame {"1" if pw < 0.5 else "2"} ({want} Hz)', amp(y, want) > 0.1 and amp(y, other) < 0.02 * amp(y, want), f"{amp(y, want):.3f} {amp(y, other):.3f}")
y = render(plugin(dict(wt_use_file=1, pulse_width=0.5), wt=two))
check('two frames: Width in the middle blends both', amp(y, 440) > 0.05 and amp(y, 1320) > 0.05 and abs(amp(y, 440) / amp(y, 1320) - 1) < 0.15, f"{amp(y, 440):.3f} {amp(y, 1320):.3f}")

# band limiting: a raw saw frame at a very high note must not alias
hi = render(plugin(dict(wt_use_file=1), wt=saw), note=108)
check('band limited: a saw frame at C8 has almost no energy off the harmonic series', offharm(hi, 4186.0) < 0.01, f"{offharm(hi, 4186.0):.5f}")
lo = render(plugin(dict(wt_use_file=1), wt=saw), note=45)
check('a saw frame at low pitch keeps its bright harmonics (20th harmonic present)', amp(lo, 110 * 20) > 0.003, f"{amp(lo, 2200):.4f}")
check('plays cleanly at 96 kHz and 48 kHz (right pitch)', all(amp(render(plugin(dict(wt_use_file=1), wt=sine1), sr=r), 440, sr=r) > 0.1 for r in (48000, 96000)))

# other oscillators and file layouts
y = render(plugin(dict(wt_use_file=1, pulse_width=0.95, waveform=0, osc_level=0.4, osc2_on=1, osc2_wave=4, osc2_level=1.0, osc2_oct=0, osc2_semi=0, osc2_fine=0), wt=two))
check('oscillator 2 set to Wavetable plays the loaded file too (main oscillator is a sine, so 1320 Hz can only come from oscillator 2)', amp(y, 1320) > 0.05 and amp(y, 440) > 0.05, f"{amp(y, 1320):.3f}")
y = render(plugin(dict(wt_use_file=1, osc2_on=1, osc2_wave=1, osc2_level=0.0), wt=sine1))
check('oscillators not set to Wavetable are untouched by the file (oscillator 2 saw at zero level changes nothing)', amp(y, 440) > 0.1)
y = render(plugin(dict(wt_use_file=1), wt=odd)); check('a 600-sample cycle (not 2048) is resampled to a clean sine', amp(y, 440) > 0.1 and amp(y, 880) < 0.01 * amp(y, 440))
for pw, h in ((0.05, 1), (0.95, 4)):
    y = render(plugin(dict(wt_use_file=1, pulse_width=pw), wt=hint))
    check(f'a file marked 1024-sample frames (clm chunk) is read as 4 frames: Width {pw} plays harmonic {h}', amp(y, 440 * h) > 0.1 and amp(y, 440 * (1 if h == 4 else 4)) < 0.03 * amp(y, 440 * h), f"{amp(y, 440 * h):.3f}")

# projects from before wavetable files existed
a = render(plugin(), note=60); b = render(plugin(drop=['wt_use_file']), note=60)
check('a project saved without the new setting sounds the same as the default', np.array_equal(a, b))
check('the file setting alone changes nothing about the other waveforms', np.array_equal(render(plugin(dict(waveform=1)), note=60), render(plugin(dict(waveform=1, wavetable_from_file=1), wt=two), note=60)))
print(f"RESULT: {'PASS' if all(rows) else 'FAIL'}  ({sum(rows)}/{len(rows)})")
sys.exit(0 if all(rows) else 1)
