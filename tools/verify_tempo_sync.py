"""Effect tempo sync and vibrato onset (headless, Pedalboard, host tempo 120 BPM): every synced effect equals the same effect with the knob set to the note value's rate,
the measured speeds, vibrato delay / fade-in per note, saved state, and projects from before these controls existed."""
import sys, subprocess, tempfile, numpy as np
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]; sys.path.insert(0, str(ROOT / 'tools'))
from pedalboard import load_plugin
from state_tools import edit_state
PATH = str(ROOT / 'build/Relic_artefacts/Release/VST3/Relic.vst3'); SR = 44100
rows = []
def check(name, ok, note=''): rows.append(bool(ok)); print(f"  [{'PASS' if ok else 'FAIL'}] {name} {note}", flush=True)
with tempfile.TemporaryDirectory() as tmp:
    binary = str(Path(tmp) / 'dsp_ts')
    subprocess.run(['clang++', '-std=c++17', '-O1', str(ROOT / 'tools/dsp_tempo_sync_test.cpp'), '-o', binary], check=True)
    r = subprocess.run([binary], capture_output=True, text=True); print(r.stdout.strip()); check('DSP harness: all exact checks pass', r.returncode == 0)
IDX = {'1/1': 1, '1/2': 2, '1/4': 3, '1/8': 4, '1/16': 5, '1/32': 6, '1/2T': 7, '1/4T': 8, '1/8T': 9, '1/16T': 10, '1/2D': 11, '1/4D': 12, '1/8D': 13, '1/16D': 14, '2/1': 15, '4/1': 16}
BEATS = {'1/1': 4, '1/2': 2, '1/4': 1, '1/8': .5, '1/16': .25, '1/32': .125, '1/2T': 4 / 3, '1/4T': 2 / 3, '1/8T': 1 / 3, '1/16T': 1 / 6, '1/2D': 3, '1/4D': 1.5, '1/8D': .75, '1/16D': .375, '2/1': 8, '4/1': 16}
BASE = dict(waveform=0, osc_level=0.4, unison_voices=1, synth_mix=1, filter_on=0, master=1.0, attack_ms=1, decay_ms=0, sustain=1, release_ms=30, voices=4, chord_on=0, arp_on=0, chop_on=0,
            osc2_on=0, osc3_on=0, trem_on=0, pan_on=0, chorus_on=0, flanger_on=0, wah_on=0, delay_on=0, pdelay_on=0, vib_on=0)
def plugin(params=None, drop=None):
    P = load_plugin(PATH); P.raw_state = edit_state(P.raw_state, {**BASE, **(params or {})}, None, None, drop); P.reset(); return P
def render(params=None, notes=((69, 0.0, 2.2),), dur=2.6, drop=None):
    ev = []
    for n, t0, t1 in notes: ev += [(bytes([0x90, n, 120]), t0), (bytes([0x80, n, 0]), t1)]
    return plugin(params, drop)(ev, duration=dur, sample_rate=SR, num_channels=2)
def mono(x): return x.mean(axis=0)
P0 = load_plugin(PATH)
check('defaults: every sync is Free, vibrato delay and fade are 0', all(str(getattr(P0, f'{e}_sync')) == 'Free' for e in ('trem', 'pan', 'chorus', 'flanger', 'wah', 'vib', 'delay', 'pdelay')) and P0.vibrato_delay_ms == 0 and P0.vibrato_fade_in_ms == 0)

# ---- equivalence: a synced effect is the same effect with the knob set to that note value's rate (120 BPM: 1 beat = 0.5 s)
FX = [('trem', dict(trem_on=1, trem_depth=1.0), 'trem_rate_hz', 'hz'), ('pan', dict(pan_on=1, pan_depth=1.0), 'pan_rate_hz', 'hz'), ('chorus', dict(chorus_on=1, chorus_depth=0.8, chorus_mix=0.6), 'chorus_rate_hz', 'hz'),
      ('flanger', dict(flanger_on=1, flanger_depth=0.8, flanger_mix=0.6), 'flanger_rate_hz', 'hz'), ('wah', dict(wah_on=1, wah_mode=0, wah_depth=1.0, wah_mix=1.0), 'wah_rate_hz', 'hz'),
      ('delay', dict(delay_on=1, delay_mix=0.5, delay_feedback=0.3), 'delay_time_ms', 'ms'), ('pdelay', dict(pdelay_on=1, pdelay_mix=0.5, pdelay_feedback=0.3, pdelay_semi=7), 'pdelay_time_ms', 'ms')]
HZ_MAX = dict(trem=20, pan=12, chorus=5, flanger=5, wah=8)
for name, base, knob, kind in FX:
    for nv in ('1/4', '1/8'):
        beats = BEATS[nv]; value = (1 / (beats * 0.5)) if kind == 'hz' else beats * 500.0
        ok_range = value <= HZ_MAX.get(name, 1e9)
        notes = ((69, 0.0, 0.25), (72, 0.7, 0.95)) if kind == 'ms' else ((69, 0.0, 2.2),)
        a = mono(render({**base, f'{name}_sync': IDX[nv], knob: 1.0 if kind == 'hz' else 90.0}, notes))
        b = mono(render({**base, knob: value}, notes)); c = mono(render({**base, knob: value * 1.37 if kind == 'hz' else value * 0.6}, notes))
        check(f'{name}: sync {nv} equals the free setting {value:g} {"Hz" if kind == "hz" else "ms"}, and differs from another rate', ok_range and np.abs(a - b).max() < 2e-3 and not np.allclose(a, c, atol=1e-2), f'(max diff {np.abs(a - b).max():.2e}; knob values are stored as floats, so within 2e-3)')

# ---- measured speeds
def env_hz(x, lo=0.5, hi=12.0):
    e = np.abs(x[int(0.3 * SR):int(2.1 * SR)]); k = int(0.004 * SR); e = np.convolve(e, np.ones(k) / k, 'same'); e = e - e.mean(); X = np.abs(np.fft.rfft(e * np.hanning(len(e)), 1 << 18)); fr = np.fft.rfftfreq(1 << 18, 1 / SR)
    m = (fr > lo) & (fr < hi); return float(fr[m][np.argmax(X[m])])
for nv in ('1/4', '1/8', '1/8T', '1/16D'):
    want = 1 / (BEATS[nv] * 0.5); got = env_hz(mono(render(dict(trem_on=1, trem_depth=1.0, trem_sync=IDX[nv]))))
    check(f'tremolo at {nv} pulses at {want:.2f} Hz (120 BPM)', abs(got - want) < 0.15, f'(measured {got:.2f})')
got_free = env_hz(mono(render(dict(trem_on=1, trem_depth=1.0, trem_rate_hz=3.0))))
check('tremolo set to Free still uses the knob (3 Hz)', abs(got_free - 3.0) < 0.15, f'({got_free:.2f})')
# delay echo time
x = mono(render(dict(delay_on=1, delay_mix=1.0, delay_feedback=0.0, delay_time_ms=900.0, delay_sync=IDX['1/8'], delay_pingpong=0), ((69, 0.0, 0.06),), 1.2))
def win(x, a, b): return float(np.sqrt(np.mean(x[int(a * SR):int(b * SR)] ** 2)))
check('delay at 1/8 (250 ms at 120 BPM): the echo arrives at 250 ms, not at the knob\'s 900 ms', win(x, 0.27, 0.31) > 0.02 and win(x, 0.5, 0.7) < 0.15 * win(x, 0.27, 0.31) and win(x, 0.9, 1.0) < 0.15 * win(x, 0.27, 0.31), f'({win(x, 0.27, 0.31):.3f})')

# ---- LFO sync list, extended
def lfo_run(**kw): return mono(render(dict(lfo1_shape=0, mod1_src=1, mod1_dst=3, mod1_amt=0.2, waveform=0, **kw)))
a = lfo_run(lfo1_sync=IDX['1/32']); b = lfo_run(lfo1_sync=0, lfo1_rate_hz=16.0)
check('LFO sync 1/32 equals 16 Hz at 120 BPM (the new list entries work on LFOs)', np.array_equal(a, b))
a = lfo_run(lfo1_sync=4); b = lfo_run(lfo1_sync=0, lfo1_rate_hz=4.0); check('LFO sync 1/8 is still 4 Hz at 120 BPM (old entries keep their meaning)', np.array_equal(a, b))

# ---- vibrato: per-note delay and fade-in
def inst_dev_ct(x, f0=440.0):
    n = len(x); X = np.fft.fft(x); h = np.zeros(n); h[0] = 1; h[1:n // 2] = 2; h[n // 2] = 1; z = np.fft.ifft(X * h)
    ph = np.unwrap(np.angle(z)); f = np.diff(ph) * SR / (2 * np.pi); k = int(0.01 * SR); f = np.convolve(f, np.ones(k) / k, 'same'); return 1200 * np.log2(np.maximum(f, 1) / f0)
VB = dict(vib_on=1, vib_depth_ct=60.0, vib_rate_hz=5.0, release_ms=60)
def swing(d, a, b): s = d[int(a * SR):int(b * SR)]; return float(s.max() - s.min())
d = inst_dev_ct(mono(render(VB)))
check('no delay, no fade: vibrato starts at once (swing about 120 ct in the first 0.3 s)', swing(d, 0.05, 0.35) > 80, f'({swing(d, 0.05, 0.35):.0f} ct)')
d = inst_dev_ct(mono(render({**VB, 'vib_delay_ms': 600.0})))
check('delay 600 ms: the pitch is steady for the first 0.5 s, then the full vibrato starts', swing(d, 0.1, 0.5) < 6 and swing(d, 0.9, 1.5) > 80, f'({swing(d, 0.1, 0.5):.1f} ct, then {swing(d, 0.9, 1.5):.0f} ct)')
d = inst_dev_ct(mono(render({**VB, 'vib_fade_ms': 1000.0})))
s1, s2, s3 = swing(d, 0.02, 0.2), swing(d, 0.5, 0.8), swing(d, 1.3, 1.9)
check('fade-in 1 s: the vibrato grows from nothing to full depth', s1 < s2 < s3 and s1 < 35 and s3 > 80, f'({s1:.0f} < {s2:.0f} < {s3:.0f} ct)')
d = inst_dev_ct(mono(render({**VB, 'vib_delay_ms': 400.0, 'vib_fade_ms': 400.0})))
check('delay 400 + fade 400: steady, then growing, then full', swing(d, 0.05, 0.35) < 6 and swing(d, 0.45, 0.75) < swing(d, 1.2, 1.9) and swing(d, 1.2, 1.9) > 80)
# each note starts its own onset
d = inst_dev_ct(mono(render({**VB, 'vib_delay_ms': 500.0}, notes=((69, 0.0, 1.4), (69, 1.5, 2.6)), dur=2.8)))
check('a second note starts its own delay (steady again just after it begins)', swing(d, 0.9, 1.35) > 80 and swing(d, 1.6, 1.95) < 8, f'({swing(d, 1.6, 1.95):.1f} ct)')
# per-voice onset with two notes at once
x = mono(render({**VB, 'vib_delay_ms': 500.0}, notes=((69, 0.0, 2.0), (76, 1.0, 2.0)), dur=2.4))
check('onset does not break chords (both notes sound after the delay)', np.abs(x[int(1.7 * SR):int(2.0 * SR)]).max() > 0.2)
# synced vibrato rate: 1/4 at 120 BPM is 2 Hz
def dev_hz(d):
    s = d[int(0.3 * SR):int(2.0 * SR)]; s = s - s.mean(); X = np.abs(np.fft.rfft(s * np.hanning(len(s)), 1 << 18)); fr = np.fft.rfftfreq(1 << 18, 1 / SR); m = (fr > 0.5) & (fr < 12); return float(fr[m][np.argmax(X[m])])
h_sync = dev_hz(inst_dev_ct(mono(render({**VB, 'vib_sync': IDX['1/4']})))); h_free = dev_hz(inst_dev_ct(mono(render(VB))))
check('vibrato synced to 1/4 wobbles at 2 Hz, Free at the knob\'s 5 Hz', abs(h_sync - 2.0) < 0.2 and abs(h_free - 5.0) < 0.3, f'({h_sync:.2f}, {h_free:.2f})')
h_s2 = dev_hz(inst_dev_ct(mono(render({**VB, 'vib_sync': IDX['1/4'], 'vib_delay_ms': 300.0}))))
check('vibrato sync also applies with a delay (2 Hz)', abs(h_s2 - 2.0) < 0.25, f'({h_s2:.2f})')

# ---- saved state and old projects
Pa = plugin(dict(trem_sync=IDX['1/8T'], delay_sync=IDX['1/4D'], vib_sync=IDX['2/1'], vib_delay_ms=321.0, vib_fade_ms=654.0, chorus_sync=16)); Pb = load_plugin(PATH); Pb.raw_state = Pa.raw_state
check('sync values and the vibrato delay / fade survive a save and restore', str(Pb.trem_sync) == '1/8T' and str(Pb.delay_sync) == '1/4D' and str(Pb.vib_sync) == '2/1' and abs(Pb.vibrato_delay_ms - 321) < 1 and abs(Pb.vibrato_fade_in_ms - 654) < 1 and str(Pb.chorus_sync) == '4/1')
new_ids = [f'{e}_sync' for e in ('trem', 'pan', 'chorus', 'flanger', 'wah', 'vib', 'delay', 'pdelay')] + ['vib_delay_ms', 'vib_fade_ms']
full = dict(trem_on=1, trem_depth=0.8, trem_rate_hz=5.0, delay_on=1, delay_time_ms=300.0, chorus_on=1, flanger_on=1, pan_on=1, wah_on=1, vib_on=1)
check('a project saved without any of the new settings sounds the same as the defaults', np.array_equal(render(full), render(full, drop=new_ids)))
check('legacy vibrato (delay 0, fade 0) is the shared vibrato as before: two identical renders', np.array_equal(render(VB), render(VB)))
print(f"RESULT: {'PASS' if all(rows) else 'FAIL'}  ({sum(rows)}/{len(rows)})"); sys.exit(0 if all(rows) else 1)
