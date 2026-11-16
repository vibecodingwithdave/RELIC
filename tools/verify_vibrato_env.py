"""Vibrato envelope (headless, Pedalboard): Delay -> Fade-in -> Decay to Sustain, velocity and mod-wheel depth, saved state, and projects from before these controls existed."""
import sys, numpy as np
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]; sys.path.insert(0, str(ROOT / 'tools'))
from pedalboard import load_plugin
from state_tools import edit_state
PATH = str(ROOT / 'build/Relic_artefacts/Release/VST3/Relic.vst3'); SR = 44100; rows = []
def check(name, ok, note=''): rows.append(bool(ok)); print(f"  [{'PASS' if ok else 'FAIL'}] {name} {note}", flush=True)
BASE = dict(waveform=0, osc_level=0.4, unison_voices=1, synth_mix=1, filter_on=0, master=1.0, attack_ms=1, decay_ms=0, sustain=1, release_ms=60, voices=4, chord_on=0, arp_on=0, chop_on=0,
            osc2_on=0, osc3_on=0, vib_on=1, vib_depth_ct=60.0, vib_rate_hz=5.0)
NEW = ['vib_decay_ms', 'vib_sustain', 'vib_vel', 'vib_wheel']
def render(params=None, notes=((69, 0.0, 2.6, 120),), dur=3.0, drop=None, cc=None):
    P = load_plugin(PATH); P.raw_state = edit_state(P.raw_state, {**BASE, **(params or {})}, None, None, drop); P.reset(); ev = []
    if cc is not None: ev.append((bytes([0xB0, 1, cc]), 0.0))
    for n, a, b, v in notes: ev += [(bytes([0x90, n, v]), a), (bytes([0x80, n, 0]), b)]
    return P(ev, duration=dur, sample_rate=SR, num_channels=2).mean(axis=0)
def dev(x, f0=440.0):
    n = len(x); X = np.fft.fft(x); h = np.zeros(n); h[0] = 1; h[1:n // 2] = 2; h[n // 2] = 1; z = np.fft.ifft(X * h)
    f = np.diff(np.unwrap(np.angle(z))) * SR / (2 * np.pi); k = int(0.01 * SR); f = np.convolve(f, np.ones(k) / k, 'same'); return 1200 * np.log2(np.maximum(f, 1) / f0)
def swing(d, a, b): s = d[int(a * SR):int(b * SR)]; return float(s.max() - s.min())
P0 = load_plugin(PATH)
check('defaults: decay 0, sustain 1, velocity 0, wheel 0', P0.vibrato_decay_ms == 0 and P0.vibrato_sustain == 1 and P0.vibrato_velocity == 0 and P0.vibrato_wheel == 0)
full = swing(dev(render()), 0.3, 0.9); check('baseline: the plain vibrato swings about 120 ct', 100 < full < 140, f'({full:.0f} ct)')
print('== DECAY AND SUSTAIN ==')
d = dev(render({'vib_decay_ms': 800.0, 'vib_sustain': 0.25}))
s0, s1, s2 = swing(d, 0.05, 0.25), swing(d, 0.35, 0.55), swing(d, 1.7, 2.4)
check('decay 800 ms to sustain 0.25: full at the start, easing off, then holding at a quarter', s0 > 90 and s0 > s1 > s2 and 20 < s2 < 45, f'({s0:.0f} > {s1:.0f} > {s2:.0f} ct)')
d = dev(render({'vib_decay_ms': 500.0, 'vib_sustain': 0.0})); check('sustain 0: the vibrato dies away completely', swing(d, 0.05, 0.3) > 80 and swing(d, 1.4, 2.4) < 6, f'({swing(d, 1.4, 2.4):.1f} ct late)')
d = dev(render({'vib_sustain': 0.5})); check('sustain 0.5 with no decay time: half depth from the start', 45 < swing(d, 0.4, 1.2) < 75, f'({swing(d, 0.4, 1.2):.0f} ct)')
d = dev(render({'vib_delay_ms': 300.0, 'vib_fade_ms': 300.0, 'vib_decay_ms': 400.0, 'vib_sustain': 0.5}))
a, b, c = swing(d, 0.05, 0.28), swing(d, 0.55, 0.75), swing(d, 1.5, 2.3)
check('delay, fade-in, decay and sustain in a row: still, then a peak, then half depth', a < 6 and b > 75 and 45 < c < 75 and b > c, f'({a:.1f}, {b:.0f}, {c:.0f} ct)')
d = dev(render({'vib_decay_ms': 500.0, 'vib_sustain': 0.0}, notes=((69, 0.0, 1.4, 120), (69, 1.5, 2.8, 120)), dur=3.0)); check('each note runs its own envelope (the second note starts at full depth again)', swing(d, 1.55, 1.85) > 70)
print('== VELOCITY AND MOD WHEEL ==')
soft = swing(dev(render({'vib_vel': 1.0}, notes=((69, 0.0, 2.0, 40),))), 0.3, 1.5); hard = swing(dev(render({'vib_vel': 1.0}, notes=((69, 0.0, 2.0, 120),))), 0.3, 1.5)
check('velocity 1.0: a soft note (40) has about a third of a hard note\'s (120) vibrato', 0.22 < soft / hard < 0.45, f'({soft:.0f} / {hard:.0f} ct = {soft / hard:.2f})')
soft0 = swing(dev(render({}, notes=((69, 0.0, 2.0, 40),))), 0.3, 1.5); hard0 = swing(dev(render({}, notes=((69, 0.0, 2.0, 120),))), 0.3, 1.5)
check('velocity 0: velocity makes no difference', abs(soft0 - hard0) < 8, f'({soft0:.0f} vs {hard0:.0f} ct)')
w0 = swing(dev(render({'vib_depth_ct': 0.0, 'vib_wheel': 1.0}, cc=0)), 0.3, 1.5); w1 = swing(dev(render({'vib_depth_ct': 0.0, 'vib_wheel': 1.0}, cc=127)), 0.3, 1.5); w5 = swing(dev(render({'vib_depth_ct': 0.0, 'vib_wheel': 0.5}, cc=127)), 0.3, 1.5)
check('mod wheel: with no vibrato depth the wheel adds up to a semitone each way', w0 < 6 and 170 < w1 < 230 and 80 < w5 < 120, f'(wheel 0: {w0:.0f}, wheel full: {w1:.0f}, half amount: {w5:.0f} ct)')
w = swing(dev(render({'vib_depth_ct': 60.0, 'vib_wheel': 1.0}, cc=127)), 0.3, 1.5); check('the wheel adds to the V.Depth setting (60 ct + 100 ct each way)', 290 < w < 340, f'({w:.0f} ct)')
w = swing(dev(render({'vib_depth_ct': 60.0, 'vib_wheel': 0.0}, cc=127)), 0.3, 1.5); check('with the wheel amount at 0 the wheel does nothing', abs(w - full) < 12)
print('== STATE ==')
Pa = load_plugin(PATH); Pa.raw_state = edit_state(Pa.raw_state, {'vib_decay_ms': 1234.0, 'vib_sustain': 0.33, 'vib_vel': 0.7, 'vib_wheel': 0.4}); Pb = load_plugin(PATH); Pb.raw_state = Pa.raw_state
check('the new settings survive a save and restore', abs(Pb.vibrato_decay_ms - 1234) < 1 and abs(Pb.vibrato_sustain - 0.33) < 0.01 and abs(Pb.vibrato_velocity - 0.7) < 0.01 and abs(Pb.vibrato_wheel - 0.4) < 0.01)
a = render(); b = render(drop=NEW); check('a project without the new settings renders the same as the defaults', np.array_equal(a, b))
a = render({'vib_delay_ms': 300.0, 'vib_fade_ms': 400.0}); b = render({'vib_delay_ms': 300.0, 'vib_fade_ms': 400.0}, drop=NEW); check('delay and fade-in alone are unchanged by the new settings', np.array_equal(a, b))
print(f'{sum(rows)}/{len(rows)} passed'); sys.exit(0 if all(rows) else 1)
