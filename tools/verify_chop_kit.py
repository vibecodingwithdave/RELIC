"""Custom chop kits (headless, Pedalboard): per-pad slice, pitch, reverse, level and choke group, saved state, and projects from before kits existed.
Test samples are synthetic tone bursts (a 4 s file, four 1 s slices at 220/330/440/660 Hz)."""
import sys, os, wave, numpy as np
sys.path.insert(0, os.path.dirname(__file__))
from pedalboard import load_plugin
from state_tools import edit_state
PATH = str(__import__('pathlib').Path(__file__).resolve().parents[1] / 'build/Relic_artefacts/Release/VST3/Relic.vst3')
ASSETS = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'test_assets'); SR = 44100; P = load_plugin(PATH); rows = []
def check(name, ok, note=''): rows.append(bool(ok)); print(f"  [{'PASS' if ok else 'FAIL'}] {name} {note}", flush=True)
def burst_file(name, freqs, dur, total):
    x = np.zeros(int(total * SR))
    for i, f in enumerate(freqs):
        n = int(dur * SR); t = np.arange(n) / SR; env = np.exp(-t * 2.0) * np.minimum(1, t / 0.002) * np.minimum(1, (dur - t) / 0.01); x[int(i * SR):int(i * SR) + n] += 0.8 * env * np.sin(2 * np.pi * f * t)
    with wave.open(os.path.join(ASSETS, name), 'wb') as w: w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR); w.writeframes((np.clip(x, -1, 1) * 32767).astype('<i2').tobytes())
burst_file('chop_even.wav', [220, 330, 440, 660], 0.9, 4.0)
BASE = dict(synth_mix=0.0, sample_gain=1.0, sample_root=60, sample_octave=0, sample_loop=0, sample_reverse=0, sample_normalize=1, sample_start=0.0, loop_start=0.0, loop_end=1.0, voices=8,
            attack_ms=0.5, decay_ms=0.0, sustain=1.0, release_ms=20.0, filter_on=0, osc_level=1.0, master=1.0, unison_voices=1, chord_on=0, arp_on=0, lead_on=0, chop_on=1, chop_mode=0, chop_count=4, chop_grid=2, chop_pitch=0)
KIT_IDS = [f'chop_kit_{n}_{k}' for n in range(1, 17) for k in ('slice', 'pitch', 'rev', 'level', 'choke')]
KIT_DEF = {i: 0.0 for i in KIT_IDS}
def render(notes, dur=2.5, raw=None, drop=None, **kw):       # every render starts from the default kit, so one test's pads do not leak into the next
    P.raw_state = raw if raw is not None else edit_state(P.raw_state, {**BASE, **KIT_DEF, **kw}, os.path.join(ASSETS, 'chop_even.wav'), None, drop); P.reset()
    ev = []
    for note, t_on, t_off in notes: ev += [(bytes([0x90, note, 110]), t_on), (bytes([0x80, note, 0]), t_off)]
    return P(ev, duration=dur, sample_rate=SR, num_channels=2).mean(axis=0)
def freq(x, a, b):
    s = x[int(a * SR):int(b * SR)]; n = 1 << 17; X = np.abs(np.fft.rfft(np.pad(s * np.hanning(len(s)), (0, n - len(s))))); k = int(np.argmax(X[1:])) + 1
    y0, y1, y2 = np.log(X[k - 1] + 1e-30), np.log(X[k] + 1e-30), np.log(X[k + 1] + 1e-30); return (k + 0.5 * (y0 - y2) / (y0 - 2 * y1 + y2)) * SR / n
rms = lambda x, a, b: float(np.sqrt(np.mean(x[int(a * SR):int(b * SR)] ** 2)))
def tone(x, a, b, f):
    s = x[int(a * SR):int(b * SR)]; t = np.arange(len(s)) / SR; return float(abs(np.sum(s * np.hanning(len(s)) * np.exp(-2j * np.pi * f * t))) / len(s))
near = lambda f, r, tol=0.015: abs(f - r) / r < tol
K = lambda n, **kw: {f'chop_kit_{n}_{k}': v for k, v in kw.items()}

print('== DEFAULTS ARE THE PLAIN CHOP SAMPLER ==')
a = render([(36, 0.0, 1.0), (38, 1.1, 2.0)]); b = render([(36, 0.0, 1.0), (38, 1.1, 2.0)], drop=KIT_IDS)
check('a state without any kit settings renders identically to one with the defaults', np.array_equal(a, b) and rms(a, 0.05, 0.5) > 0.01, f'(max diff {np.abs(a - b).max():.1e})')
check('pad 1 plays slice 1, pad 3 slice 3 as before', near(freq(a, 0.05, 0.5), 220) and near(freq(a, 1.15, 1.6), 440))
print('== SLICE ASSIGNMENT ==')
x = render([(36, 0.0, 1.0)], **K(1, slice=3)); check('pad 1 set to slice 3 plays the 440 Hz slice', near(freq(x, 0.05, 0.5), 440), f'({freq(x, 0.05, 0.5):.1f} Hz)')
x = render([(37, 0.0, 1.0)], **K(2, slice=1)); check('pad 2 set to slice 1 plays the 220 Hz slice', near(freq(x, 0.05, 0.5), 220), f'({freq(x, 0.05, 0.5):.1f} Hz)')
x = render([(36, 0.0, 1.0), (37, 0.0, 1.0)], **K(1, slice=3), **K(2, slice=3)); check('two pads can share one slice (both sound together)', near(freq(x, 0.05, 0.5), 440) and rms(x, 0.05, 0.5) > 1.7 * rms(render([(36, 0.0, 1.0)], **K(1, slice=3)), 0.05, 0.5))
x = render([(37, 0.0, 1.0)], **K(2, slice=17)); check('Mute silences a pad', rms(x, 0, 1.2) < 1e-5)
x = render([(36, 0.0, 1.0), (38, 0.0, 1.0)], **K(1, slice=17)); check('Mute leaves the other pads alone', near(freq(x, 0.05, 0.5), 440) and tone(x, 0.05, 0.5, 220) < 0.02 * tone(x, 0.05, 0.5, 440))
x = render([(36, 0.0, 1.0)], **K(1, slice=9)); check('a slice that does not exist (9 of 4) stays silent', rms(x, 0, 1.2) < 1e-5)
x = render([(40, 0.0, 1.0)], **K(5, slice=2)); check('a pad beyond the slice count can be given a slice', near(freq(x, 0.05, 0.5), 330), f'({freq(x, 0.05, 0.5):.1f} Hz)')
print('== PER-PAD PITCH, REVERSE, LEVEL ==')
x = render([(37, 0.0, 1.0), (38, 1.2, 2.2)], dur=2.6, **K(2, pitch=12)); check('pad 2 +12 st doubles its frequency, pad 3 is unchanged', near(freq(x, 0.03, 0.3), 660) and near(freq(x, 1.25, 1.7), 440), f'({freq(x, 0.03, 0.3):.1f} / {freq(x, 1.25, 1.7):.1f} Hz)')
x = render([(37, 0.0, 1.0)], **K(2, pitch=-12)); check('pad 2 -12 st halves its frequency', near(freq(x, 0.05, 0.6), 165), f'({freq(x, 0.05, 0.6):.1f} Hz)')
x = render([(37, 0.0, 1.0)], chop_pitch=12, **K(2, pitch=12)); check('kit pitch adds to the global Chop pitch (+24 st = x4)', near(freq(x, 0.02, 0.2), 1320, 0.02), f'({freq(x, 0.02, 0.2):.1f} Hz)')
x = render([(36, 0.0, 1.0)], **K(1, rev=1)); e1, e2 = rms(x, 0.0, 0.4), rms(x, 0.5, 0.9); check('pad reverse swells instead of decaying', e2 > 1.5 * e1, f'({e1:.3f} then {e2:.3f})')
x = render([(36, 0.0, 1.0), (37, 1.2, 2.2)], dur=2.6, **K(1, rev=1)); check('reverse on pad 1 leaves pad 2 playing forward', rms(x, 1.2, 1.6) > 1.5 * rms(x, 1.8, 2.15))
x = render([(36, 0.0, 1.0)], sample_reverse=1, **K(1, rev=1)); e1, e2 = rms(x, 0.0, 0.4), rms(x, 0.5, 0.9); check('pad reverse on top of the global reverse plays forward again', e1 > 1.5 * e2, f'({e1:.3f} then {e2:.3f})')
base = rms(render([(36, 0.0, 1.0)], sample_gain=0.25), 0.05, 0.5)       # quiet enough that the output stage never saturates, even at +6 dB
for db in (-6.0, 6.0, -12.0):
    got = rms(render([(36, 0.0, 1.0)], sample_gain=0.25, **K(1, level=db)), 0.05, 0.5) / base; want = 10 ** (db / 20)
    check(f'pad level {db:+.0f} dB scales the output by {want:.3f}', abs(got - want) / want < 0.03, f'(measured {got:.3f})')
print('== CHOKE GROUPS ==')
def two(c1, c2): return render([(36, 0.0, 1.5), (37, 0.5, 1.5)], dur=1.8, **K(1, choke=c1), **K(2, choke=c2))
free = two(0, 0); same = two(1, 1); other = two(1, 2)
check('no groups: pad 1 keeps ringing when pad 2 starts', tone(free, 0.7, 0.95, 220) > 0.02 and tone(free, 0.7, 0.95, 330) > 0.02, f'({tone(free, 0.7, 0.95, 220):.3f})')
check('same group: pad 2 cuts pad 1 off', tone(same, 0.7, 0.95, 220) < 0.05 * tone(free, 0.7, 0.95, 220) and tone(same, 0.7, 0.95, 330) > 0.02, f'({tone(same, 0.7, 0.95, 220):.4f} vs {tone(free, 0.7, 0.95, 220):.3f})')
check('different groups do not cut each other', tone(other, 0.7, 0.95, 220) > 0.9 * tone(free, 0.7, 0.95, 220))
check('pad 1 before pad 2 starts is unaffected by the group', abs(rms(same, 0.1, 0.45) - rms(free, 0.1, 0.45)) < 1e-6)
x = render([(36, 0.0, 0.4), (36, 0.5, 1.0)], dur=1.4, **K(1, choke=1)); check('a pad does not choke itself (retrigger plays)', rms(x, 0.55, 0.9) > 0.02)
print('== SAVED STATE ==')
kit = {**K(1, slice=3, pitch=7, rev=1, level=-3.0, choke=2), **K(16, slice=1, pitch=-5, level=2.5, choke=4)}
ref = render([(36, 0.0, 1.0), (51, 1.2, 2.2)], dur=2.6, **kit); raw = P.raw_state
check('kit settings are kept in the saved state', render([(36, 0.0, 1.0), (51, 1.2, 2.2)], dur=2.6, raw=raw).tolist() == ref.tolist() and rms(ref, 0.05, 0.5) > 0.005)
P2 = load_plugin(PATH); P2.raw_state = raw; P2.reset()
y = P2([(bytes([0x90, 36, 110]), 0.0), (bytes([0x80, 36, 0]), 1.0), (bytes([0x90, 51, 110]), 1.2), (bytes([0x80, 51, 0]), 2.2)], duration=2.6, sample_rate=SR, num_channels=2).mean(axis=0)
check('a fresh instance loaded from that state sounds the same', np.allclose(y, ref, atol=1e-6))
print(f'{sum(rows)}/{len(rows)} passed'); sys.exit(0 if all(rows) else 1)
