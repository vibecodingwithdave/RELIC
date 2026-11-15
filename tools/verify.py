"""Verify Relic against its acceptance numbers (headless, Pedalboard)."""
import sys, math, numpy as np
from pedalboard import load_plugin
PATH = sys.argv[1] if len(sys.argv) > 1 else str(__import__('pathlib').Path(__file__).resolve().parents[1] / 'build/Relic_artefacts/Release/VST3/Relic.vst3')
P = load_plugin(PATH); SR = 44100
BASE = dict(waveform='Saw', pulse_width=0.5, osc_level=1.0, unison_voices=1, unison_detune=0.0, unison_spread=0.0, voices=1,
            attack_ms=1.0, decay_ms=0.0, sustain=1.0, release_ms=50.0, filter_on=False, filter_type='LP', cutoff_hz=1000.0, resonance=0.0,
            tremolo_on=False, delay_on=False, comp_on=False, crush_on=False, pan_on=False, master=1.0)
LAST = {}
def setp(**kw):
    for k, v in {**BASE, **kw}.items():
        if LAST.get(k, '__') == v: continue
        LAST[k] = v; setattr(P, k, v)
def render(note, dur=3.0, hold=None, stereo=False, **kw):
    setp(**kw); P.reset()
    out = P([(bytes([0x90, note, 127]), 0.0), (bytes([0x80, note, 0]), (dur - 0.4) if hold is None else hold)], duration=dur, sample_rate=SR, num_channels=2)
    return out if stereo else out.mean(axis=0)
hz = lambda n: 440 * 2 ** ((n - 69) / 12)
db = lambda a: 10 * np.log10(np.maximum(a, 1e-30))
def spec(x, nfft=1 << 17, start=1.0):
    s = x[int(start * SR):int(start * SR) + nfft]; s = np.pad(s, (0, nfft - len(s))) if len(s) < nfft else s
    return np.abs(np.fft.rfft(s * np.hanning(nfft))) ** 2, SR / nfft
def harm(x, f0, kmax=400, fmax=20500):
    X, bh = spec(x); k0 = int(f0 / bh); lo = max(1, int(k0 * .97)); hi = int(k0 * 1.03) + 2
    kk = lo + int(np.argmax(X[lo:hi]))
    a, b, c = np.log(X[kk - 1] + 1e-30), np.log(X[kk] + 1e-30), np.log(X[kk + 1] + 1e-30)
    fm = (kk + 0.5 * (a - c) / (a - 2 * b + c)) * bh          # parabolic peak interpolation
    ks, am = [], []
    for k in range(1, kmax + 1):
        f = fm * k
        if f > fmax: break
        ci = int(round(f / bh)); ks.append(f); am.append(X[max(ci - 3, 1):ci + 4].max())
    return fm, np.array(ks), np.array(am), X, bh
rows = []
def check(name, value, target, tol, unit='', note=''):
    ok = abs(value - target) <= tol
    rows.append(ok); print(f"  [{'PASS' if ok else 'FAIL'}] {name}: {value:+.2f}{unit}  (target {target:+.2f} +/-{tol}){'  ' + note if note else ''}")
def below(name, value, limit, unit='', note=''):
    ok = value <= limit; rows.append(ok); print(f"  [{'PASS' if ok else 'FAIL'}] {name}: {value:.1f}{unit}  (must be <= {limit}){'  ' + note if note else ''}")

print('== OSCILLATOR ==')
for wave, note in (('Saw', 36), ('Saw', 60), ('Square', 36), ('Triangle', 36)):
    x = render(note, waveform=wave); fm, ks, am, _, _ = harm(x, hz(note), kmax=9); h = db(am / am[0])
    cents = 1200 * math.log2(fm / hz(note))
    check(f'{wave} note {note} pitch', cents, 0.0, 1.0, ' cents')
    ideal = {'Saw': [-6.0, -9.5, -12.0, -14.0], 'Square': [None, -9.5, None, -14.0], 'Triangle': [None, -19.1, None, -28.0]}[wave]
    for i, t in enumerate(ideal):
        if t is not None and i < len(h) - 1: check(f'{wave} harmonic {i + 2}', h[i + 1], t, 1.5, ' dB')
    if wave in ('Square', 'Triangle'): below(f'{wave} even harmonic 2', h[1], -40.0, ' dB')
print('  -- relative level vs sine (reference: saw -0.7 dB peak-ish, see SPEC) --')
rs = {w: float(np.sqrt((render(60, waveform=w)[SR:2 * SR] ** 2).mean())) for w in ('Sine', 'Saw', 'Square', 'Triangle')}
for w in ('Saw', 'Square', 'Triangle'): print(f'      {w} rms re sine: {20 * math.log10(rs[w] / rs["Sine"]):+.1f} dB   (reference: Saw -3.2, Square -0.1, Triangle -2.2)')
print('  -- aliasing: energy at fold-back frequencies of harmonics above Nyquist --')
for note in (84, 96):
    x = render(note, waveform='Saw'); fm, ks, am, X, bh = harm(x, hz(note)); p1 = X[int(round(fm / bh)) - 3:int(round(fm / bh)) + 4].max(); k = len(ks) + 1; lv = []
    for kk in range(k, k + 10):
        fa = (fm * kk) % SR; fa = SR - fa if fa > SR / 2 else fa
        if min(abs(fa - fm * j) for j in range(1, len(ks) + 1)) < 80: continue
        c = int(round(fa / bh)); lv.append(db(X[max(c - 3, 1):c + 4].max() / p1))
    below(f'Saw note {note} alias level (mean of {len(lv)} lines)', float(np.mean(lv)), -70.0, ' dB', '(reference: -81 to -93)')

print('== FILTER (saw note 36) ==')
def resp(**kw):
    # run quiet: resonance peaks must stay under the output safety stage (it is transparent below 0.9)
    x0 = render(36, filter_on=False, osc_level=0.08); x1 = render(36, filter_on=True, osc_level=0.08, **kw)
    _, ks, a0, _, _ = harm(x0, hz(36)); _, _, a1, _, _ = harm(x1, hz(36)); return ks, db(a1) - db(a0)
def summ(ks, r):
    low = np.median(r[(ks > 40) & (ks < 150)]); rel = r - low; pk = int(np.argmax(rel)); f3 = None
    for i in range(1, len(ks)):
        if ks[i] > 100 and rel[i] < -3 and rel[i - 1] >= -3: f3 = ks[i - 1] + (ks[i] - ks[i - 1]) * (rel[i - 1] + 3) / (rel[i - 1] - rel[i]); break
    sl = None
    if f3 and f3 * 6 < 19000: m = (ks > f3 * 2) & (ks < f3 * 6); sl = np.polyfit(np.log2(ks[m]), rel[m], 1)[0]
    return low, rel[pk], ks[pk], f3, sl
for c in (250.0, 1000.0, 4000.0):
    ks, r = resp(filter_type='LP', cutoff_hz=c, resonance=0.0); low, pk, pf, f3, sl = summ(ks, r)
    check(f'LP cutoff {c:.0f}: DC gain', low, 0.0, 0.5, ' dB'); check(f'LP cutoff {c:.0f}: -3 dB point', f3 / c, 1.0, 0.05, 'x cutoff')
    if sl is not None: check(f'LP cutoff {c:.0f}: slope', sl, -13.0, 2.5, ' dB/oct', '(2-pole)')
for q, ref in ((0.25, 6.4), (0.5, 10.5), (0.8, 13.7), (1.0, 15.4)):
    ks, r = resp(filter_type='LP', cutoff_hz=1000.0, resonance=q); low, pk, pf, f3, sl = summ(ks, r)
    check(f'LP reso {q}: peak', pk, ref, 1.5, ' dB', f'(reference {ref})'); check(f'LP reso {q}: DC gain (no compensation)', low, 0.0, 0.7, ' dB')
for t in ('HP', 'BP', 'Notch'):
    ks, r = resp(filter_type=t, cutoff_hz=1000.0, resonance=0.0); pts = [250, 500, 1000, 2000, 4000]
    print(f'      {t}: ' + ' '.join(f'{p}Hz:{np.interp(p, ks, r):+.0f}' for p in pts) + '   (reference: HP -24 -12 -3 0 0 | BP -9 -3 0 -3 -9 | Notch -1 -3 -29 -3 -1)')

print('== TREMOLO (sine 1 kHz) ==')
def envelope(x, ms=2):
    n = int(SR * ms / 1000); return np.sqrt(np.convolve(x ** 2, np.ones(n) / n, 'same'))
for depth, rate in ((0.5, 2.0), (0.5, 10.0), (1.0, 5.0)):
    x = render(84, dur=5.0, waveform='Sine', tremolo_on=True, tremolo_depth=depth, tremolo_rate_hz=rate)
    e = envelope(x[SR:int(4.5 * SR)])[2000:-2000]; f = np.abs(np.fft.rfft(e - e.mean())); fr = np.fft.rfftfreq(len(e), 1 / SR); k = np.argmax(f[1:]) + 1
    check(f'tremolo rate {rate} Hz', fr[k], rate, 0.1, ' Hz'); check(f'tremolo depth {depth}: min/max', e.min() / e.max(), 1 - depth, 0.05)

print('== DELAY (ping-pong, 500 ms, feedback 0.5, mix 0.5) ==')
x = render(57, dur=4.0, hold=0.15, stereo=True, waveform='Saw', delay_on=True, delay_time_ms=500.0, delay_feedback=0.5, delay_mix=0.5, delay_ping_pong=True)
m = envelope(x.mean(axis=0), 10); m /= m.max(); pk = []; sep = int(SR * 0.25); i = 0
while i < len(m):
    if m[i] > 0.01 and m[i] == m[max(0, i - sep):i + sep].max(): pk.append(i); i += sep
    else: i += 1
t = [p / SR for p in pk]; a = [20 * np.log10(m[p]) for p in pk]
L = envelope(x[0], 10); Rr = envelope(x[1], 10); side = ''.join('L' if L[p] > Rr[p] * 1.2 else 'R' if Rr[p] > L[p] * 1.2 else 'C' for p in pk)
print('      echo times:', ' '.join(f'{v:.3f}' for v in t[:5]), '| levels', ' '.join(f'{v:.1f}' for v in a[:5]), '| channels', side[:5])
if len(a) >= 4: check('echo-to-echo step', a[3] - a[2], -6.02, 0.8, ' dB', '(reference PING-PONG -6.0)')
if len(t) >= 3: check('echo spacing', t[2] - t[1], 0.5, 0.02, ' s')

print('== COMPRESSOR (sine 1 kHz, steady state) ==')
def rmsdb(x): return 20 * math.log10(float(np.sqrt((x[SR:2 * SR] ** 2).mean())) + 1e-12)
for thr, ratio, mk in ((-12.0, 3.0, 0.0), (-24.0, 10.0, 0.0), (-12.0, 3.0, 6.0)):
    worst = 0.0
    for lv in (0.03, 0.1, 0.3, 1.0):
        d = rmsdb(render(84, dur=3.0, waveform='Sine', osc_level=lv)); c = rmsdb(render(84, dur=3.0, waveform='Sine', osc_level=lv, comp_on=True, comp_threshold_db=thr, comp_ratio=ratio, comp_makeup_db=mk))
        over = max(0.0, d + 2.0 - thr); pred = -(1 - 1 / ratio) * over + mk; worst = max(worst, abs((c - d) - pred))
    check(f'threshold {thr:.0f} ratio {ratio:.0f} makeup {mk:.0f}: worst error vs formula', worst, 0.0, 1.0, ' dB')

print('== BITCRUSHER (sine 440 Hz) ==')
xd = render(69, dur=3.0, waveform='Sine', osc_level=1.0)[SR:2 * SR]; pk_amp = float(np.abs(xd).max())
for bits in (4.0, 8.0):
    x = render(69, dur=3.0, waveform='Sine', crush_on=True, crush_bits=bits, crush_downsample=1.0)[SR:2 * SR]
    e = x - xd; snr = 10 * np.log10((xd ** 2).sum() / (e ** 2).sum()); ideal = 6.02 * bits + 1.76 + 20 * np.log10(pk_amp)
    check(f'{bits:.0f}-bit SNR', snr, ideal, 3.0, ' dB', f'(ideal for this level {ideal:.1f})')
for ds in (4.0,):
    x = render(69, dur=3.0, waveform='Sine', crush_on=True, crush_bits=16.0, crush_downsample=ds)[SR:SR + 65536]
    X = np.abs(np.fft.rfft(x * np.hanning(len(x)))) ** 2; fr = np.fft.rfftfreq(len(x), 1 / SR); X2 = X.copy(); X2[(fr > 410) & (fr < 470)] = 0
    img = fr[np.argmax(X2)]; exp = SR / ds - 440
    check(f'downsample {ds:.0f}: strongest image frequency', img, exp, 40.0, ' Hz', '(sample-and-hold aliasing)')

print('== AUTO-PANNER (sine 1 kHz) ==')
x = render(84, dur=6.0, stereo=True, waveform='Sine', pan_on=True, pan_rate_hz=3.0, pan_depth=1.0, pan_shape='Sine')
Lm = envelope(x[0], 5)[SR:5 * SR]; Rm = envelope(x[1], 5)[SR:5 * SR]; bal = (Lm - Rm) / (Lm + Rm + 1e-9)
f = np.abs(np.fft.rfft(bal - bal.mean())); fr = np.fft.rfftfreq(len(bal), 1 / SR); k = np.argmax(f[1:]) + 1
check('panner rate', fr[k], 3.0, 0.1, ' Hz'); check('panner depth 1.0: max |balance|', float(np.abs(bal).max()), 1.0, 0.05)

print(f"\nRESULT: {sum(rows)} of {len(rows)} checks passed")
