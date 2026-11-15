"""EQ, tape, widener, wah, lo-fi, OTT and lead mode: the C++ DSP harness, then the real plugin (Pedalboard)."""
import subprocess, sys, os, numpy as np
from scipy.signal import hilbert
from pedalboard import load_plugin
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE); exe = '/tmp/dsp_fx2'
subprocess.check_call(['clang++', '-std=c++17', '-O2', '-o', exe, os.path.join(HERE, 'dsp_fx2_test.cpp')])
r = subprocess.run([exe], capture_output=True, text=True); print(r.stdout.strip()); rows = [r.returncode == 0]
P = load_plugin(os.path.join(ROOT, 'build/Relic_artefacts/Release/VST3/Relic.vst3')); SR = 44100
def passed(n, ok, note=''): rows.append(bool(ok)); print(f"  [{'PASS' if ok else 'FAIL'}] {n} {note}")
FX = ['crush', 'pan', 'tremolo', 'comp', 'delay', 'chorus', 'flanger', 'reverb', 'eq', 'tape', 'widener', 'wah', 'lofi', 'ott']
BASE = dict(waveform='Sine', filter_on=False, synth_mix=1.0, master=1.0, osc_level=0.3, voices=8, unison_voices=1, unison_spread=0.0, attack_ms=1, decay_ms=0, sustain=1.0, release_ms=20, lead_on=False,
            vibrato_on=False, portamento_on=False, wah_mode='LFO')
def render(events, dur, **kw):
    for f in FX: setattr(P, f + '_on', False)
    for k, v in {**BASE, **kw}.items(): setattr(P, k, v)
    P.reset(); return P(events, duration=dur, sample_rate=SR, num_channels=2)
def note(n=57, vel=100, a=0.0, b=1.9): return [(bytes([0x90, n, vel]), a), (bytes([0x80, n, 0]), b)]
rms = lambda x, a, b: float(np.sqrt(np.mean(x[:, int(a * SR):int(b * SR)] ** 2)))
dry = render(note(), 2.0)
print('== inside the plugin')
for fx in ('eq', 'tape', 'widener', 'wah', 'lofi', 'ott'):
    pass
passed('all new effects off: identical to dry', float(np.abs(render(note(), 2.0, eq_low_db=12.0, tape_drive=1.0, ott_depth=1.0) - dry).max()) < 1e-9)
x = render(note(33), 2.0, eq_on=True, eq_low_db=12.0); y = render(note(33), 2.0)
passed('EQ low shelf +12 dB lifts a 55 Hz note by 11 to 13 dB', 11 < 20 * np.log10(rms(x, 0.3, 1.5) / rms(y, 0.3, 1.5)) < 13, '(%.1f dB)' % (20 * np.log10(rms(x, 0.3, 1.5) / rms(y, 0.3, 1.5))))
x = render(note(57), 2.0, tape_on=True, tape_drive=1.0, tape_tone=1.0, tape_wobble=0.0, osc_level=0.4)[0]; s = x[int(0.5 * SR):int(1.5 * SR)]; X = np.abs(np.fft.rfft(s * np.hanning(len(s)))); fr = np.fft.rfftfreq(len(s), 1 / SR)
h = lambda f: X[np.argmin(abs(fr - f))]; passed('tape drive adds a third harmonic', h(660) > 0.05 * h(220), '(%.3f of fundamental)' % (h(660) / h(220)))
SAWST = dict(waveform='Saw', unison_voices=5, unison_detune=25.0, unison_spread=1.0)
w0 = render(note(), 2.0, widener_on=True, widener_width=0.0, **SAWST); w1 = render(note(), 2.0, **SAWST)
passed('widener 0 makes the sound mono', float(np.abs(w0[0] - w0[1]).max()) < 1e-5, '(%.1e)' % float(np.abs(w0[0] - w0[1]).max()))
w2 = render(note(), 2.0, widener_on=True, widener_width=2.0, **SAWST); side = lambda z: rms(z[0:1] - z[1:2], 0.3, 1.5)
passed('widener 2 doubles the side signal', abs(side(w2) / side(w1) - 2.0) < 0.1, '(x%.2f)' % (side(w2) / side(w1)))
w = render(note(), 2.0, wah_on=True, wah_depth=1.0, wah_rate_hz=2.0, waveform='Saw'); ws = [rms(w, t, t + 0.05) for t in np.arange(0.2, 1.8, 0.05)]
passed('wah sweeps the sound in time', max(ws) / min(ws) > 2.0, '(%.1fx)' % (max(ws) / min(ws)))
lo = render([], 2.0, lofi_on=True, lofi_noise=1.0); passed('lo-fi noise is audible on silence', rms(lo, 0.5, 1.5) > 0.002, '(%.4f)' % rms(lo, 0.5, 1.5))
lf = render(note(), 2.0, lofi_on=True, lofi_tone_hz=1000.0, lofi_noise=0.0, lofi_wobble=0.0, waveform='Saw'); sp = lambda z: np.abs(np.fft.rfft(z[0, int(0.5 * SR):int(1.5 * SR)]))
fr2 = np.fft.rfftfreq(SR, 1 / SR); hi = lambda z: float(sp(z)[fr2 > 6000].sum() / sp(z).sum()); passed('lo-fi tone 1 kHz removes the top end of a saw', hi(lf) < 0.2 * hi(render(note(), 2.0, waveform='Saw')))
q = render(note(57, 5), 2.0); o = render(note(57, 5), 2.0, ott_on=True, ott_depth=1.0, ott_mix=1.0)
passed('OTT lifts a quiet note by at least 5 dB', 20 * np.log10(rms(o, 0.5, 1.5) / rms(q, 0.5, 1.5)) > 5.0, '(%.1f dB)' % (20 * np.log10(rms(o, 0.5, 1.5) / rms(q, 0.5, 1.5))))
allfx = render(note(), 2.0, osc_level=0.5, eq_on=True, eq_low_db=10.0, tape_on=True, widener_on=True, wah_on=True, lofi_on=True, ott_on=True, chorus_on=True, flanger_on=True, delay_on=True, reverb_on=True)
passed('everything on: finite, under full scale', bool(np.isfinite(allfx).all()) and float(np.abs(allfx).max()) < 1.0, '(peak %.2f)' % float(np.abs(allfx).max()))

print('== lead mode')
def ifreq(x, k=0.01): ph = np.unwrap(np.angle(hilbert(x[0]))); f = np.diff(ph) * SR / (2 * np.pi); n = int(k * SR); return np.convolve(f, np.ones(n) / n, 'same')
hz = lambda n: 440 * 2 ** ((n - 69) / 12)
ev = [(bytes([0x90, 57, 100]), 0.0), (bytes([0x90, 64, 100]), 1.0), (bytes([0x80, 57, 0]), 1.5), (bytes([0x80, 64, 0]), 2.5)]
x = render(ev, 3.2, lead_on=True, attack_ms=300.0, release_ms=40.0); f = ifreq(x)
passed('lead: first note plays, then the overlapping note takes over', abs(f[int(0.9 * SR)] - hz(57)) < 3 and abs(f[int(1.3 * SR)] - hz(64)) < 3, '(%.0f then %.0f Hz)' % (f[int(0.9 * SR)], f[int(1.3 * SR)]))
passed('lead: the envelope does not restart on the overlapping note (no dip)', rms(x, 0.99, 1.03) > 0.9 * rms(x, 0.9, 0.95), '(%.3f vs %.3f)' % (rms(x, 0.99, 1.03), rms(x, 0.9, 0.95)))
x2 = render(ev, 3.2, lead_on=False, attack_ms=300.0, release_ms=40.0); passed('poly mode, same notes: two voices sound together (contrast)', rms(x2, 1.1, 1.45) > 1.3 * rms(x, 1.1, 1.45), '(%.3f vs %.3f)' % (rms(x2, 1.1, 1.45), rms(x, 1.1, 1.45)))
ev2 = [(bytes([0x90, 57, 100]), 0.0), (bytes([0x90, 64, 100]), 0.6), (bytes([0x80, 64, 0]), 1.2), (bytes([0x80, 57, 0]), 2.0)]
x3 = render(ev2, 2.6, lead_on=True, attack_ms=50.0, release_ms=40.0); f3 = ifreq(x3)
passed('lead: releasing the top note returns to the note still held', abs(f3[int(0.9 * SR)] - hz(64)) < 3 and abs(f3[int(1.6 * SR)] - hz(57)) < 3, '(%.0f then %.0f Hz)' % (f3[int(0.9 * SR)], f3[int(1.6 * SR)]))
passed('lead: silent after the last note is released', rms(x3, 2.3, 2.6) < 1e-4)
x4 = render(ev, 3.2, lead_on=True, portamento_on=True, portamento_ms=200.0, attack_ms=20.0); f4 = ifreq(x4, 0.005)
passed('lead with glide: slides to the new note instead of jumping', 225 < f4[int(1.04 * SR)] < 300 and f4[int(1.04 * SR)] < f4[int(1.15 * SR)] < hz(64) and abs(f4[int(1.9 * SR)] - hz(64)) < 3, '(%.0f, %.0f, then %.0f Hz)' % (f4[int(1.04 * SR)], f4[int(1.15 * SR)], f4[int(1.9 * SR)]))
print(f"\nRESULT: {sum(rows)} of {len(rows)} groups/checks passed (DSP harness counts as one)"); sys.exit(0 if all(rows) else 1)
