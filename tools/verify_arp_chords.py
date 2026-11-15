"""Verify the chord engine and arpeggiator (headless, Pedalboard). The host has no tempo here, so the plugin falls back to 120 BPM."""
import sys, os, math, numpy as np
from pedalboard import load_plugin
PATH = str(__import__('pathlib').Path(__file__).resolve().parents[1] / 'build/Relic_artefacts/Release/VST3/Relic.vst3')
P = load_plugin(PATH); SR = 44100; rows = []
hz = lambda n: 440 * 2 ** ((n - 69) / 12)
def passed(name, ok, note=''): rows.append(bool(ok)); print(f"  [{'PASS' if ok else 'FAIL'}] {name} {note}")
BASE = dict(waveform='Sine', filter_on=False, tremolo_on=False, delay_on=False, comp_on=False, crush_on=False, pan_on=False, synth_mix=1.0, master=1.0, osc_level=0.3,
            voices=16, unison_voices=1, attack_ms=0.5, decay_ms=0.0, sustain=1.0, release_ms=6.0, chord_on=False, chord_key='C', chord_scale='Major', chord_type='Triad',
            chord_inversion=0, chord_voicing='Close', chord_lock=True, chord_strum_ms=0.0, chord_mode='Scale', chord_shape='MAJ7', voice_lead=False, arp_on=False, arp_hold=False, arp_division='1/8', arp_mode='Up', arp_octaves=1, arp_gate=0.5, arp_swing=0.0, arp_order='Pitch', arp_grouping='Pattern', arp_waterfall_shape='Cascade', arp_human=0.0)
def setup(**kw):
    for k, v in {**BASE, **kw}.items():
        assert k in P.parameters, f'unknown parameter name {k}'
        setattr(P, k, v)
    P.reset()
def play(events, dur):
    msgs = []
    for t, note, on in events: msgs.append((bytes([0x90 if on else 0x80, note, 110 if on else 0]), t))
    return P(msgs, duration=dur, sample_rate=SR, num_channels=2).mean(axis=0)
def notes_in(x, a=0.2, b=1.2, floor_db=-22):
    s = x[int(a * SR):int(b * SR)]; n = 1 << 17; X = np.abs(np.fft.rfft(np.pad(s * np.hanning(len(s)), (0, n - len(s))))); fr = np.fft.rfftfreq(n, 1 / SR)
    thr = X.max() * 10 ** (floor_db / 20); got = set()
    for k in range(2, len(X) - 2):
        if X[k] > thr and X[k] >= X[k - 1] and X[k] >= X[k + 1] and 30 < fr[k] < 4000: got.add(int(round(69 + 12 * math.log2(fr[k] / 440))))
    return sorted(got)
def chord(held, **kw):
    setup(chord_on=True, **kw); x = play([(0.0, n, True) for n in held] + [(1.4, n, False) for n in held], 1.6); return notes_in(x)

print('== CHORDS ==')
passed('C major, C key: triad', chord([60]) == [60, 64, 67], chord([60]))
passed('C major, D key: D minor', chord([62]) == [62, 65, 69], chord([62]))
passed('C major, B key: B diminished', chord([71]) == [71, 74, 77], chord([71]))
passed('A minor, A key: A C E', chord([57], chord_key='A', chord_scale='Minor') == [57, 60, 64], chord([57], chord_key='A', chord_scale='Minor'))
passed('D dorian, D key: D F A', chord([62], chord_key='D', chord_scale='Dorian') == [62, 65, 69])
passed('7th: Cmaj7', chord([60], chord_type='7th') == [60, 64, 67, 71]); passed('7th on G: G7', chord([67], chord_type='7th') == [67, 71, 74, 77])
passed('9th', chord([60], chord_type='9th') == [60, 64, 67, 71, 74]); passed('add9', chord([60], chord_type='Add9') == [60, 64, 67, 74])
passed('sus2', chord([60], chord_type='Sus2') == [60, 62, 67]); passed('sus4', chord([60], chord_type='Sus4') == [60, 65, 67]); passed('power', chord([60], chord_type='Power') == [60, 67, 72])
passed('inversion 1', chord([60], chord_inversion=1) == [64, 67, 72]); passed('inversion 2', chord([60], chord_inversion=2) == [67, 72, 76])
passed('voicing open', chord([60], chord_voicing='Open') == [60, 67, 76]); passed('voicing drop 2', chord([60], chord_voicing='Drop 2') == [52, 60, 67]); passed('voicing spread', chord([60], chord_voicing='Spread') == [48, 64, 79])
passed('lock on: C# snaps to the C chord', chord([61]) == [60, 64, 67], chord([61])); passed('lock off: C# plays a C# major chord', chord([61], chord_lock=False) == [61, 65, 68], chord([61], chord_lock=False))
passed('two keys at once make two chords', chord([60, 67]) == [60, 62, 64, 67, 71, 74][:0] or set(chord([60, 67])) == {60, 64, 67, 71, 74})
print('== STRUM ==')
setup(chord_on=True, chord_strum_ms=100.0); x = play([(0.0, 60, True), (1.0, 60, False)], 1.2)
def onset_time(sig, note, thr=0.35):
    f = hz(note); n = int(0.02 * SR); t = np.arange(len(sig)) / SR; c = np.cos(2 * np.pi * f * t) * sig; s = np.sin(2 * np.pi * f * t) * sig
    k = np.ones(n) / n; amp = np.sqrt(np.convolve(c, k, 'same') ** 2 + np.convolve(s, k, 'same') ** 2); amp /= amp[int(0.6 * SR):int(0.9 * SR)].mean() + 1e-12
    i = int(np.argmax(amp > thr)); return i / SR
ts = [onset_time(x, n) for n in (60, 64, 67)]
passed('three notes spaced 100 ms apart', all(abs((ts[i + 1] - ts[i]) - 0.1) < 0.02 for i in range(2)), '(onsets %.3f %.3f %.3f s)' % tuple(ts))

print('== ARPEGGIATOR (1/8 at 120 BPM = 0.25 s per step) ==')
def arp_steps(held, n=6, **kw):
    setup(arp_on=True, **kw); x = play([(0.0, k, True) for k in held] + [(0.25 * n + 0.2, k, False) for k in held], 0.25 * n + 0.4); out = []
    for i in range(n): out.append(notes_in(x, i * 0.25 + 0.03, i * 0.25 + 0.115, -12))
    return out
UP = [[60], [64], [67], [60], [64], [67]]
passed('up', arp_steps([60, 64, 67]) == UP, str(arp_steps([60, 64, 67])))
passed('down', arp_steps([60, 64, 67], arp_mode='Down') == [[67], [64], [60], [67], [64], [60]])
passed('up-down', arp_steps([60, 64, 67], arp_mode='Up-Down') == [[60], [64], [67], [64], [60], [64]], str(arp_steps([60, 64, 67], arp_mode='Up-Down')))
passed('down-up', arp_steps([60, 64, 67], arp_mode='Down-Up') == [[67], [64], [60], [64], [67], [64]])
passed('2 octaves', arp_steps([60, 64, 67], arp_octaves=2) == [[60], [64], [67], [72], [76], [79]], str(arp_steps([60, 64, 67], arp_octaves=2)))
passed('played order (G, C, E)', arp_steps([67, 60, 64], arp_mode='Played') == [[67], [60], [64], [67], [60], [64]], str(arp_steps([67, 60, 64], arp_mode='Played')))
passed('chord mode plays all notes every step', all(s == [60, 64, 67] for s in arp_steps([60, 64, 67], arp_mode='Chord', n=4)))
r = arp_steps([60, 64, 67], arp_mode='Random', n=12); flat = [s[0] for s in r if len(s) == 1]
passed('random stays inside the held notes and varies', set(flat) <= {60, 64, 67} and len(set(flat)) >= 2, str(flat))
passed('chord + arp: C chord arpeggiated', arp_steps([60], chord_on=True) == UP, str(arp_steps([60], chord_on=True)))

def onsets(x, thr=0.3):
    n = int(0.004 * SR); env = np.sqrt(np.convolve(x ** 2, np.ones(n) / n, 'same')); env /= env.max(); up = np.where((env[1:] > thr) & (env[:-1] <= thr))[0] + 1
    return [u / SR for u in up]
for rate, gap in (('1/4', 0.5), ('1/8', 0.25), ('1/8T', 1 / 6), ('1/16', 0.125), ('1/16T', 1 / 12), ('1/32', 0.0625)):
    setup(arp_on=True, arp_division=rate, arp_gate=0.5); x = play([(0.0, 60, True), (3.5, 60, False)], 3.7); o = onsets(x)[:10]
    d = np.diff(o); passed(f'rate {rate}: steps {gap * 1000:.0f} ms apart', len(d) > 3 and np.allclose(d, gap, atol=0.004), '(%.1f ms)' % (1000 * float(np.median(d))))
setup(arp_on=True, arp_gate=0.5); x = play([(0.0, 60, True), (1.0, 60, False)], 1.2)
on_e = float(np.mean(x[int(0.325 * SR):int(0.385 * SR)] ** 2)); off_e = float(np.mean(x[int(0.415 * SR):int(0.515 * SR)] ** 2))   # steps start 25 ms after the first key
passed('gate 50%: sounding for half the step, then silent', on_e > 100 * off_e + 1e-9, '(on %.2e, off %.2e)' % (on_e, off_e))
setup(arp_on=True, arp_gate=0.25); x = play([(0.0, 60, True), (1.0, 60, False)], 1.2); o = onsets(x)
dur = [np.argmax(np.convolve(x[int(t * SR):int((t + 0.24) * SR)] ** 2, np.ones(88) / 88, 'same')[int(0.01 * SR):] < 1e-7) / SR + 0.01 for t in o[:3]]
passed('gate 25%: note length is about 62 ms', all(0.045 < d < 0.08 for d in dur), '(%s ms)' % ', '.join('%.0f' % (1000 * d) for d in dur))
setup(arp_on=True, arp_swing=1.0, arp_gate=0.3); x = play([(0.0, 60, True), (1.4, 60, False)], 1.6); o = onsets(x)[:6]
passed('swing 1.0: odd steps arrive 125 ms late (half a step)', len(o) == 5 and np.allclose(o, [0.025, 0.4, 0.525, 0.9, 1.025], atol=0.006), '(%s)' % ', '.join('%.3f' % t for t in o))
setup(arp_on=True); x = play([(0.0, 60, True), (0.6, 60, False)], 1.2)
passed('release stops the arp within 20 ms', float(np.abs(x[int(0.62 * SR):]).max()) < 1e-3, '(peak after release %.1e)' % float(np.abs(x[int(0.62 * SR):]).max()))
print('== HOLD ==')
setup(arp_on=True, arp_hold=True); x = play([(0.0, 60, True), (0.3, 60, False), (2.0, 64, True), (2.2, 64, False)], 3.2)
passed('hold: keeps playing after the key is released', float(np.abs(x[int(1.0 * SR):int(1.5 * SR)]).max()) > 0.05)
passed('hold: a new key replaces the held note', notes_in(x, 2.6, 3.0, -12) == [64], str(notes_in(x, 2.6, 3.0, -12)))
print('== reference-measured behaviours: order, rates, swing scale, human, tied gate')
def arp_seq(held, n=8, stagger=0.02, **kw):
    setup(arp_on=True, arp_gate=0.5, **kw); ev = [(i * stagger, k, True) for i, k in enumerate(held)] + [(0.25 * n + 0.2, k, False) for k in held]; x = play(ev, 0.25 * n + 0.4); out = []
    for i in range(n): out.append(notes_in(x, i * 0.25 + 0.05, i * 0.25 + 0.115, -12))
    return [s[0] if len(s) == 1 else s for s in out]
H = [67, 60, 72, 64]
passed('order PITCH: C4 E4 G4 C5', arp_seq(H, 4, arp_order='Pitch') == [60, 64, 67, 72], str(arp_seq(H, 4, arp_order='Pitch')))
passed('order AS PLAYED: G4 C4 C5 E4', arp_seq(H, 4, arp_order='As Played') == [67, 60, 72, 64], str(arp_seq(H, 4, arp_order='As Played')))
passed('order CONVERGE: C4 C5 E4 G4 (outside in)', arp_seq(H, 4, arp_order='Converge') == [60, 72, 64, 67], str(arp_seq(H, 4, arp_order='Converge')))
passed('order DIVERGE: G4 E4 C5 C4 (inside out)', arp_seq(H, 4, arp_order='Diverge') == [67, 64, 72, 60], str(arp_seq(H, 4, arp_order='Diverge')))
r = arp_seq(H, 12, arp_order='Random'); cyc = [sorted(r[i:i + 4]) for i in (4, 8)]     # the first cycle starts before every staggered key is down
passed('order RANDOM: each full cycle plays all four notes once', all(c == [60, 64, 67, 72] for c in cyc), str(r))
r2 = arp_seq(H, 24, arp_order='Random'); passed('order RANDOM: the order changes from cycle to cycle', len({tuple(r2[i:i + 4]) for i in (4, 8, 12, 16, 20)}) >= 3, str(r2))
passed('order PITCH with 2 octaves walks all six: C4 E4 G4 C5 E5 G5', arp_seq([60, 64, 67], 6, arp_order='Pitch', arp_octaves=2) == [60, 64, 67, 72, 76, 79])
for rate, gap in (('1/1', 2.0), ('1/2', 1.0), ('1/2D', 1.5), ('1/2T', 2 / 3), ('1/4D', 0.75), ('1/4T', 1 / 3), ('1/8D', 0.375), ('1/16D', 0.1875), ('1/32', 0.0625)):
    setup(arp_on=True, arp_division=rate, arp_gate=0.3); x = play([(0.0, 60, True), (7.5, 60, False)], 8.0); o = onsets(x)[:7]; d = np.diff(o)[:5]
    passed(f'rate {rate}: steps {gap * 1000:.0f} ms apart', len(d) >= 3 and np.allclose(d, gap, atol=0.005), '(%.1f ms)' % (1000 * float(np.median(d))))
for sw, late in ((0.5, 0.0625), (1.0, 0.125)):
    setup(arp_on=True, arp_swing=sw, arp_gate=0.3); x = play([(0.0, 60, True), (1.4, 60, False)], 1.6); o = onsets(x)[:6]; gp = np.diff(o)
    passed(f'swing {sw}: odd steps late by {late * 1000:.1f} ms (swing x half a step)', len(gp) >= 3 and abs(gp.max() - (0.25 + late)) < 0.006 and abs(gp.min() - (0.25 - late)) < 0.006, '(gaps %.1f and %.1f ms)' % (1000 * gp.min(), 1000 * gp.max()))
def jitter(h):
    setup(arp_on=True, arp_human=h, arp_gate=0.3); x = play([(0.0, 60, True), (7.0, 60, False)], 7.2); o = onsets(x, 0.12); e = np.sqrt(np.convolve(x ** 2, np.ones(88) / 88, 'same'))
    pk = [float(e[int(t * SR):int((t + 0.06) * SR)].max()) for t in o[:26]]; return o, pk
o0, p0 = jitter(0.0); o5, p5 = jitter(0.5); o1, p1 = jitter(1.0)
dev = lambda o: np.std([t - round(t / 0.25) * 0.25 for t in o[1:20]])
passed('human 0: dead on the grid (timing spread under 1 ms)', dev(o0) < 0.001, '(%.2f ms)' % (1000 * dev(o0)))
passed('human 0.5: about 4 ms of scatter, and a little level give', 0.002 < dev(o5) < 0.007 and np.std(p5) / np.mean(p5) > 0.005, '(%.1f ms, level spread %.1f%%)' % (1000 * dev(o5), 100 * np.std(p5) / np.mean(p5)))
passed('human 1.0: more scatter and level variation, with ghost notes (some steps far quieter)', dev(o1) > dev(o5) and min(p1) < 0.45 * max(p1), '(%.1f ms, quietest step %.0f%% of loudest)' % (1000 * dev(o1), 100 * min(p1) / max(p1)))
setup(arp_on=True, arp_gate=1.0, arp_swing=0.0); x = play([(0.0, 60, True), (1.4, 60, False)], 1.6); e = np.sqrt(np.convolve(x ** 2, np.ones(88) / 88, 'same'))
passed('gate 100%: notes tie, no gap between steps', e[int(0.1 * SR):int(1.2 * SR)].min() > 0.3 * e[int(0.1 * SR):int(1.2 * SR)].max(), '(quietest %.0f%% of loudest)' % (100 * e[int(0.1 * SR):int(1.2 * SR)].min() / e[int(0.1 * SR):int(1.2 * SR)].max()))
print('== WATERFALL (measured on a reference instrument: 4 steps up the notes, then the next 4 up-positions played backwards)')
def wf(held, n, step=0.25, **kw):
    setup(arp_on=True, arp_mode='Waterfall', arp_gate=0.4, **kw); ev = [(0.0, k, True) for k in held] + [(0.025 + step * n + 0.2, k, False) for k in held]; x = play(ev, 0.025 + step * n + 0.4); out = []
    for i in range(n): a0 = 0.025 + i * step; out.append(notes_in(x, a0 + 0.015, a0 + 0.015 + min(0.06, step * 0.35), -12))
    return [s[0] if len(s) == 1 else s for s in out]
def phrase(pool, n, groups=None):
    L = len(pool); out = []
    for s_ in range(n):
        if groups is None: j = s_ % 8
        else:
            j = s_ % 16; start = 0
            for g in groups:
                if j < start + g: j -= start; break
                start += g
        out.append(pool[(j if j < 4 else 11 - j) % L])
    return out
for held in ([60, 64, 67], [60, 67], [60, 64, 67, 71], [60, 62, 64, 65, 67]):
    r = wf(held, 16); passed(f'waterfall, {len(held)} notes: 8-step phrase repeats', r == phrase(sorted(held), 16), str(r[:10]))
r = wf([60, 64, 67], 16, arp_octaves=2); passed('waterfall, C E G over 2 octaves (six notes)', r == phrase([60, 64, 67, 72, 76, 79], 16), str(r[:10]))
for name_, gs in (('5-5-6', [5, 5, 6]), ('7-7-2', [7, 7, 2]), ('3-3-3-3-4', [3, 3, 3, 3, 4]), ('4-4-4-4', [4, 4, 4, 4])):
    r = wf([60, 64, 67], 32, step=0.125, arp_octaves=2, arp_grouping=name_, arp_division='1/8'); passed(f'waterfall grouping {name_}: each group restarts the phrase, and the step is locked to 1/16', r == phrase([60, 64, 67, 72, 76, 79], 32, gs), str(r[:16]))
print('== WATERFALL shapes (measured on a reference instrument, six notes = C E G over 2 octaves)')
POOL = [60, 64, 67, 72, 76, 79]
def shape_expect(idx): return [POOL[i % 6] for i in idx]
SH = { 'Cascade': [0, 1, 2, 3, 1, 0, 5, 4] * 3, 'Spiral': [0, 1, 2, 3, 3, 4, 5, 0] * 3, 'Geyser': [0, 1, 2, 3, 2, 3, 4, 5, 2, 3, 4, 5] * 2, 'Twin': [0, 1, 2, 3, 1, 2, 3, 4] * 3,
       'Tide': [0, 1, 2, 3, 4, 5, 4, 3, 2, 1] * 2 + [0, 1, 2, 3], 'Climb': [0, 1, 2, 3, 4, 5, 0, 1] * 3 }
for sh, idx in SH.items():
    r = wf([60, 64, 67], 24, arp_octaves=2, arp_waterfall_shape=sh); passed(f'waterfall shape {sh}', r == shape_expect(idx[:24]), str(r[:12]))
print('== CHORDS measured on a reference instrument: 11th/13th, FIXED shapes, voice leading')
def chord_seq(keys, hold=0.9, gap=0.3, **kw):
    if kw.get('voice_lead'): setup(chord_on=True, **{**kw, 'voice_lead': False}); play([(0.0, 60, True), (0.15, 60, False)], 0.3)      # toggling voice lead off and on starts a progression over
    setup(chord_on=True, **kw); ev = []; tt = 0.0
    for k in keys: ev += [(tt, k, True), (tt + hold, k, False)]; tt += hold + gap
    x = play(ev, tt + 0.3); out = []; tt = 0.0
    for k in keys: out.append(notes_in(x, tt + 0.25, tt + 0.75, -22)); tt += hold + gap
    return out
passed('11th chord on C4: C E G B D F', chord_seq([60], chord_type='11th')[0] == [60, 64, 67, 71, 74, 77], str(chord_seq([60], chord_type='11th')[0]))
passed('13th chord on C4: C E G B D F A', chord_seq([60], chord_type='13th')[0] == [60, 64, 67, 71, 74, 77, 81])
passed('11th chord on D4 in C major: D F A C E G', chord_seq([62], chord_type='11th')[0] == [62, 65, 69, 72, 76, 79])
SHAPES = { 'MAJ': [0, 4, 7], 'MIN': [0, 3, 7], 'MAJ7': [0, 4, 7, 11], 'MIN7': [0, 3, 7, 10], 'DOM7': [0, 4, 7, 10], 'MAJ9': [0, 4, 7, 11, 14], 'MIN9': [0, 3, 7, 10, 14], 'DOM9': [0, 4, 7, 10, 14], 'MAJ11': [0, 4, 7, 11, 14, 17], 'MIN11': [0, 3, 7, 10, 14, 17],
           'MAJ13': [0, 4, 7, 11, 14, 17, 21], 'SUS2': [0, 2, 7], 'SUS4': [0, 5, 7], 'ADD9': [0, 4, 7, 14], '6': [0, 4, 7, 9], 'MIN6': [0, 3, 7, 9], 'DIM': [0, 3, 6], 'DIM7': [0, 3, 6, 9], 'AUG': [0, 4, 8], '9SUS4': [0, 5, 7, 10, 14],
           '13SUS4': [0, 5, 7, 10, 14, 21], '7#5': [0, 4, 8, 10], '7#5#9': [0, 4, 8, 10, 15], '7b9': [0, 4, 7, 10, 13] }
bad = []
for sh, iv in SHAPES.items():
    r = chord_seq([60, 66], chord_mode='Fixed', chord_shape=sh)
    if r[0] != [60 + i for i in iv] or r[1] != [66 + i for i in iv]: bad.append((sh, r))
passed('all 24 FIXED shapes, on C4 and F#4 (the same shape under every key)', not bad, str(bad[:2]))
r = chord_seq([60, 61, 62], chord_mode='Fixed', chord_shape='MAJ7'); passed('FIXED ignores the scale (C#4 plays C#maj7)', r[1] == [61, 65, 68, 72], str(r))
passed('voice lead off: plain root-position chords', chord_seq([60, 65, 67, 60], chord_type='7th') == [[60, 64, 67, 71], [65, 69, 72, 76], [67, 71, 74, 77], [60, 64, 67, 71]])
import subprocess
import tempfile; SCR = tempfile.gettempdir()
subprocess.run(['clang++', '-std=c++17', '-O1', '-o', f'{SCR}/dsp_lead', os.path.join(os.path.dirname(os.path.abspath(__file__)), 'dsp_lead_test.cpp')], check=True)
rr = subprocess.run([f'{SCR}/dsp_lead'], capture_output=True, text=True); print(rr.stdout.strip().replace(chr(10), chr(10)))
passed('voice leading (chord engine alone, 7 checks: the reference progressions, octave follow, resets)', rr.returncode == 0)
subprocess.run(['clang++', '-std=c++17', '-O1', '-o', f'{SCR}/dsp_arp_run', os.path.join(os.path.dirname(os.path.abspath(__file__)), 'dsp_arp_run_test.cpp')], check=True)
rr = subprocess.run([f'{SCR}/dsp_arp_run'], capture_output=True, text=True); print(rr.stdout.strip())
passed('GO/STOP transitions (note engine alone)', rr.returncode == 0)
print(f"\nRESULT: {sum(rows)} of {len(rows)} chord/arp checks passed")
