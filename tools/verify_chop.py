"""Verify the chop sampler (headless, Pedalboard). Test samples are synthetic tone bursts written here."""
import sys, os, math, wave, numpy as np
sys.path.insert(0, os.path.dirname(__file__))
from pedalboard import load_plugin
from state_tools import edit_state
PATH = str(__import__('pathlib').Path(__file__).resolve().parents[1] / 'build/Relic_artefacts/Release/VST3/Relic.vst3')
ASSETS = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'test_assets')
SR = 44100; P = load_plugin(PATH); rows = []
def passed(name, ok, note=''): rows.append(bool(ok)); print(f"  [{'PASS' if ok else 'FAIL'}] {name} {note}")
def burst_file(name, starts, freqs, dur, total):
    x = np.zeros(int(total * SR))
    for s, f in zip(starts, freqs):
        n = int(dur * SR); t = np.arange(n) / SR; env = np.exp(-t * 2.0) * np.minimum(1, t / 0.002) * np.minimum(1, (dur - t) / 0.01)
        x[int(s * SR):int(s * SR) + n] += 0.8 * env * np.sin(2 * np.pi * f * t)
    with wave.open(os.path.join(ASSETS, name), 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR); w.writeframes((np.clip(x, -1, 1) * 32767).astype('<i2').tobytes())
burst_file('chop_even.wav', [0, 1, 2, 3], [220, 330, 440, 660], 0.9, 4.0)
burst_file('chop_irreg.wav', [0.0, 0.7, 1.9, 3.1], [220, 330, 440, 660], 0.45, 4.0)
BASE = dict(synth_mix=0.0, sample_gain=1.0, sample_root=60, sample_octave=0, sample_loop=0, sample_reverse=0, sample_normalize=1, sample_start=0.0, loop_start=0.0, loop_end=1.0,
            voices=8, attack_ms=0.5, decay_ms=0.0, sustain=1.0, release_ms=20.0, filter_on=0, osc_level=1.0, master=1.0, unison_voices=1,
            chord_on=0, arp_on=0, lead_on=0, chop_on=1, chop_mode=0, chop_count=4, chop_grid=2, chop_pitch=0)
def render(notes, dur=2.5, wav='chop_even.wav', **kw):
    P.raw_state = edit_state(P.raw_state, {**BASE, **kw}, os.path.join(ASSETS, wav)); P.reset()
    ev = []
    for note, t_on, t_off in notes: ev += [(bytes([0x90, note, 110]), t_on), (bytes([0x80, note, 0]), t_off)]
    return P(ev, duration=dur, sample_rate=SR, num_channels=2).mean(axis=0)
def freq(x, a, b):
    s = x[int(a * SR):int(b * SR)]; n = 1 << 17; X = np.abs(np.fft.rfft(np.pad(s * np.hanning(len(s)), (0, n - len(s))))); k = int(np.argmax(X[1:])) + 1
    y0, y1, y2 = np.log(X[k - 1] + 1e-30), np.log(X[k] + 1e-30), np.log(X[k + 1] + 1e-30); return (k + 0.5 * (y0 - y2) / (y0 - 2 * y1 + y2)) * SR / n
rms = lambda x, a, b: float(np.sqrt(np.mean(x[int(a * SR):int(b * SR)] ** 2)))
near = lambda f, r, tol=0.01: abs(f - r) / r < tol

print('== EVEN SLICES (4 slices of a 4 s file; pads C1..D#1) ==')
for i, f in enumerate((220, 330, 440, 660)):
    x = render([(36 + i, 0.0, 1.6)]); got = freq(x, 0.05, 0.6)
    passed(f'pad {i + 1} plays slice {i + 1} ({f} Hz)', near(got, f), '(%.1f Hz)' % got)
x = render([(36, 0.0, 2.3)])
passed('one-shot stops at the slice end (no spill into the next slice)', rms(x, 1.05, 2.2) < 1e-3 * max(1e-9, rms(x, 0.05, 0.5)), '(after %.1e, during %.2f)' % (rms(x, 1.05, 2.2), rms(x, 0.05, 0.5)))
x = render([(40, 0.0, 1.0)]); passed('pad beyond the slice count is silent', rms(x, 0, 1.5) < 1e-5, '(%.1e)' % rms(x, 0, 1.5))
x = render([(60, 0.0, 1.5)], sample_root=60); passed('notes outside the pad range still play the sample pitched', near(freq(x, 0.05, 0.6), 220), '(%.1f Hz)' % freq(x, 0.05, 0.6))
x = render([(36, 0.0, 1.5)], chop_on=0); passed('chop off: C1 is an ordinary pitched note (3 octaves down)', near(freq(x, 0.05, 0.6), 220 / 2 ** 2.0, 0.03), '(%.1f Hz)' % freq(x, 0.05, 0.6))
print('== SLICE COUNT ==')
x = render([(38, 0.0, 1.0)], chop_count=8); passed('8 slices: pad 3 is 1.0-1.5 s of the file (330 Hz)', near(freq(x, 0.05, 0.4), 330), '(%.1f Hz)' % freq(x, 0.05, 0.4))
x = render([(51, 0.0, 1.0)], chop_count=16); passed('16 slices: pad 16 is the last quarter second (660 Hz, then it ends)', near(freq(x, 0.01, 0.14), 660) and rms(x, 0.3, 1.0) < 1e-4)
x = render([(36, 0.0, 1.0)], chop_count=16); passed('16 slices: pad 1 is only 0.25 s long', rms(x, 0.3, 1.0) < 1e-4 and rms(x, 0.02, 0.2) > 0.01)
print('== ONSET SLICES (bursts at 0, 0.7, 1.9, 3.1 s) ==')
for i, f in enumerate((220, 330, 440, 660)):
    x = render([(36 + i, 0.0, 1.0)], wav='chop_irreg.wav', chop_mode=1); got = freq(x, 0.04, 0.4)
    passed(f'onset pad {i + 1} starts on the {f} Hz hit', near(got, f), '(%.1f Hz)' % got)
x = render([(36, 0.0, 1.0)], wav='chop_irreg.wav', chop_mode=1); passed('onset slice 1 ends where hit 2 begins (about 0.7 s)', rms(x, 0.78, 1.0) < 1e-3 * rms(x, 0.05, 0.4))
x = render([(36, 0.0, 1.0)], wav='chop_irreg.wav', chop_mode=1, chop_count=2); passed('onset, 2 slices: pad 1 is everything up to the strongest onset', rms(x, 0.05, 0.4) > 0.01)
print('== GRID SLICES (120 BPM fallback) ==')
x = render([(38, 0.0, 1.0)], chop_mode=2, chop_grid=0, chop_count=8); passed('1/4 grid = 0.5 s slices: pad 3 is 1.0 s onward (330 Hz)', near(freq(x, 0.05, 0.4), 330), '(%.1f Hz)' % freq(x, 0.05, 0.4))
x = render([(36, 0.0, 2.0)], chop_mode=2, chop_grid=1, chop_count=8); passed('1/8 grid = 0.25 s slices: pad 1 is 0.25 s long', rms(x, 0.3, 1.5) < 1e-4 and rms(x, 0.02, 0.2) > 0.01)
print('== PLAYBACK OPTIONS ==')
x = render([(37, 0.0, 1.0)], chop_pitch=12); passed('pitch +12 doubles the frequency', near(freq(x, 0.03, 0.3), 660, 0.015), '(%.1f Hz)' % freq(x, 0.03, 0.3))
x = render([(37, 0.0, 1.0)], chop_pitch=-12); passed('pitch -12 halves the frequency', near(freq(x, 0.05, 0.6), 165, 0.015), '(%.1f Hz)' % freq(x, 0.05, 0.6))
x = render([(36, 0.0, 1.0)], sample_reverse=1); e1, e2 = rms(x, 0.0, 0.4), rms(x, 0.5, 0.9); passed('reverse plays the slice backwards (swell instead of decay)', e2 > 1.5 * e1, '(first half %.3f, second half %.3f)' % (e1, e2))
x = render([(36, 0.0, 2.4)], sample_loop=1, dur=2.6); passed('loop repeats the slice while the key is held', rms(x, 1.5, 2.3) > 0.02, '(%.3f)' % rms(x, 1.5, 2.3))
x = render([(36, 0.0, 0.3)], release_ms=10.0, dur=1.0); passed('releasing the key before the slice ends cuts it off', rms(x, 0.4, 0.9) < 1e-3)
x = render([(36, 0.0, 1.5)], velocity=0) if False else render([(36, 0.0, 1.0)], sample_gain=0.5); x2 = render([(36, 0.0, 1.0)], sample_gain=1.0)
passed('sample gain scales the chops', 0.4 < rms(x, 0.05, 0.4) / rms(x2, 0.05, 0.4) < 0.6)
x = render([(36, 0.0, 1.0), (37, 0.5, 1.5), (38, 0.9, 1.5)], dur=2.0); passed('several pads can overlap', rms(x, 0.55, 0.85) > 0.02 and rms(x, 0.95, 1.4) > 0.02)
x = render([(36, 0.0, 1.5)], lead_on=1, chop_on=1); passed('chop works with LEAD on', near(freq(x, 0.05, 0.6), 220))
print(f"\nRESULT: {sum(rows)} of {len(rows)} chop checks passed")
