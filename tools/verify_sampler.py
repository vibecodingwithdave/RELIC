"""Verify the Relic sampler (headless, Pedalboard). Run after build."""
import sys, os, math, numpy as np
sys.path.insert(0, os.path.dirname(__file__))
from pedalboard import load_plugin
from state_tools import edit_state
PATH = str(__import__('pathlib').Path(__file__).resolve().parents[1] / 'build/Relic_artefacts/Release/VST3/Relic.vst3')
ASSETS = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'test_assets')
P = load_plugin(PATH); rows = []
def check(name, value, target, tol, unit=''):
    ok = abs(value - target) <= tol; rows.append(ok)
    print(f"  [{'PASS' if ok else 'FAIL'}] {name}: {value:+.3f}{unit}  (target {target:+.3f} +/-{tol})")
def passed(name, ok, note=''):
    rows.append(bool(ok)); print(f"  [{'PASS' if ok else 'FAIL'}] {name} {note}")
BASE = dict(synth_mix=0.0, sample_gain=1.0, sample_root=69, sample_octave=0, sample_loop=0, sample_reverse=0, sample_normalize=0,
            sample_start=0.0, loop_start=0.0, loop_end=1.0, voices=4, attack_ms=0.5, decay_ms=0.0, sustain=1.0, release_ms=30.0,
            filter_on=0, osc_level=1.0, master=1.0, unison_voices=1)
def render(note, dur=2.0, sr=44100, hold=None, wav='sine440.wav', **kw):
    params = {**BASE, **kw}
    P.raw_state = edit_state(P.raw_state, params, os.path.join(ASSETS, wav))
    P.reset()
    out = P([(bytes([0x90, note, 127]), 0.0), (bytes([0x80, note, 0]), dur - 0.1 if hold is None else hold)], duration=dur, sample_rate=sr, num_channels=2)
    return out.mean(axis=0)
def freq(x, sr, a=0.2, b=1.2):
    s = x[int(a * sr):int(b * sr)]; n = 1 << 17; X = np.abs(np.fft.rfft(np.pad(s * np.hanning(len(s)), (0, n - len(s))))); k = int(np.argmax(X[1:])) + 1
    y0, y1, y2 = np.log(X[k - 1] + 1e-30), np.log(X[k] + 1e-30), np.log(X[k + 1] + 1e-30)
    return (k + 0.5 * (y0 - y2) / (y0 - 2 * y1 + y2)) * sr / n
def cents(f, ref): return 1200 * math.log2(f / ref)

print('== PITCH (sample is a 440 Hz sine, root note A4) ==')
for note, ref in ((69, 440.0), (81, 880.0), (57, 220.0), (72, 523.25)):
    x = render(note); check(f'note {note}', cents(freq(x, 44100), ref), 0.0, 1.5, ' cents')
x = render(69, sample_root=57, sample_octave=0); check('root moved to A3: note A4 plays an octave up', cents(freq(x, 44100), 880.0), 0.0, 1.5, ' cents')
x = render(69, sample_octave=1); check('octave +1', cents(freq(x, 44100), 880.0), 0.0, 1.5, ' cents')
x = render(69, sr=48000, dur=2.0); check('host at 48 kHz: pitch preserved', cents(freq(x, 48000), 440.0), 0.0, 1.5, ' cents')
print('== LEVEL ==')
q = render(69, wav='sine440_quiet.wav', sample_normalize=0); n = render(69, wav='sine440_quiet.wav', sample_normalize=1)
check('quiet sample, normalize off: peak', float(np.abs(q).max()), 0.1, 0.02); check('quiet sample, normalize on: peak', float(np.abs(n).max()), 1.0, 0.1)
g = render(69, sample_gain=0.5); b = render(69, sample_gain=1.0); check('gain 0.5 vs 1.0 (ratio)', float(np.abs(g).max() / np.abs(b).max()), 0.5, 0.03)
print('== SAMPLE / SYNTH MIX ==')
full = render(69, synth_mix=0.0, waveform=1); half = render(69, synth_mix=0.5, waveform=1); syn = render(69, synth_mix=1.0, waveform=1)
spec = lambda x: np.abs(np.fft.rfft(x[22050:22050 + 44100] * np.hanning(44100)))
def h2(x): s = spec(x); return s[880] / (s[440] + 1e-12)       # saw has a 2nd harmonic, a sine does not
passed('mix 0: sample only (pure sine, no saw harmonics)', h2(full) < 0.02, f'(2nd/1st = {h2(full):.3f})')
passed('mix 1: synth only (saw harmonics present)', h2(syn) > 0.3, f'(2nd/1st = {h2(syn):.3f})')
passed('mix 0.5: both present', 0.05 < h2(half) < h2(syn), f'(2nd/1st = {h2(half):.3f})')
print('== DIRECTION, START, LOOP (ramp sample 0 to 0.8 over 1 s, played at root 60) ==')
sm = lambda x, a, b: float(np.mean(x[int(a * 44100):int(b * 44100)]))
f = render(60, wav='ramp.wav', sample_root=60, dur=1.2, hold=1.1); r = render(60, wav='ramp.wav', sample_root=60, sample_reverse=1, dur=1.2, hold=1.1)
passed('forward: rises', sm(f, 0.8, 0.95) > sm(f, 0.05, 0.2) + 0.3); passed('reverse: falls', sm(r, 0.8, 0.95) < sm(r, 0.05, 0.2) - 0.3)
st = render(60, wav='ramp.wav', sample_root=60, sample_start=0.5, dur=1.0, hold=0.9); check('start at 50%: first value', sm(st, 0.01, 0.04), 0.4, 0.05)
lo = render(60, wav='ramp.wav', sample_root=60, sample_loop=1, dur=3.0, hold=2.9); nl = render(60, wav='ramp.wav', sample_root=60, sample_loop=0, dur=3.0, hold=2.9)
passed('loop on: still sounding at 2.5 s', float(np.abs(lo[int(2.4 * 44100):int(2.6 * 44100)]).max()) > 0.2); passed('loop off: silent after the sample ends', float(np.abs(nl[int(1.3 * 44100):int(2.6 * 44100)]).max()) < 0.01)
la = render(60, wav='ramp.wav', sample_root=60, sample_loop=1, loop_start=0.0, loop_end=0.5, dur=3.0, hold=2.9)
passed('loop region 0..50%: stays under 0.45', float(np.abs(la[int(1.5 * 44100):int(2.6 * 44100)]).max()) < 0.45)
print('== STATE ==')
raw = bytes(P.raw_state); P2 = load_plugin(PATH); P2.raw_state = raw
o = P2([(bytes([0x90, 69, 127]), 0.0), (bytes([0x80, 69, 0]), 1.0)], duration=1.2, sample_rate=44100, num_channels=2).mean(axis=0)
passed('sample restored from saved state in a fresh instance', float(np.abs(o).max()) > 0.05)
print(f"\nRESULT: {sum(rows)} of {len(rows)} sampler checks passed")
