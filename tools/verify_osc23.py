"""Verify oscillators 2 and 3 (headless, Pedalboard)."""
import numpy as np
from pedalboard import load_plugin
PATH = str(__import__('pathlib').Path(__file__).resolve().parents[1] / 'build/Relic_artefacts/Release/VST3/Relic.vst3')
P = load_plugin(PATH); SR = 44100; rows = []
def passed(n, ok, note=''): rows.append(bool(ok)); print(f"  [{'PASS' if ok else 'FAIL'}] {n} {note}")
BASE = dict(waveform='Sine', filter_on=False, tremolo_on=False, delay_on=False, comp_on=False, crush_on=False, pan_on=False, synth_mix=1.0, master=1.0, osc_level=0.3,
            voices=4, unison_voices=1, attack_ms=0.5, decay_ms=0.0, sustain=1.0, release_ms=6.0, chord_on=False, arp_on=False, lead_on=False, chop_on=False,
            osc_2_on=False, osc_2_wave='Sine', osc_2_level=1.0, osc_2_octave=0, osc_2_semi=0, osc_2_fine=0.0,
            osc_3_on=False, osc_3_wave='Sine', osc_3_level=1.0, osc_3_octave=0, osc_3_semi=0, osc_3_fine=0.0)
def render(note=57, **kw):
    for k, v in {**BASE, **kw}.items():
        assert k in P.parameters, k
        setattr(P, k, v)
    P.reset(); return P([(bytes([0x90, note, 127]), 0.0), (bytes([0x80, note, 0]), 1.4)], duration=1.6, sample_rate=SR, num_channels=2).mean(axis=0)
def amp_at(x, f, tol=0.012):
    s = x[int(0.2 * SR):int(1.2 * SR)]; n = 1 << 18; X = np.abs(np.fft.rfft(np.pad(s * np.hanning(len(s)), (0, n - len(s))))) / (len(s) / 4); fr = np.fft.rfftfreq(n, 1 / SR)
    m = (fr > f * (1 - tol)) & (fr < f * (1 + tol)); return float(X[m].max())
f0 = 220.0
x = render(); passed('osc 2 and 3 off by default: only the 220 Hz tone', amp_at(x, 220) > 0.25 and amp_at(x, 440) < 0.01 and amp_at(x, 330) < 0.01, '(%.3f, %.4f)' % (amp_at(x, 220), amp_at(x, 440)))
x = render(osc_2_on=True, osc_2_octave=1, osc_2_fine=0.0); a1, a2 = amp_at(x, 220), amp_at(x, 440)
passed('osc 2 +1 octave adds 440 Hz at its own level', abs(a2 / a1 - 1.0) < 0.05, '(%.3f / %.3f)' % (a2, a1))
x = render(osc_2_on=True, osc_2_octave=1, osc_2_level=0.5); passed('osc 2 level scales (half level, half amplitude)', abs(amp_at(x, 440) / amp_at(x, 220) - 0.5) < 0.04)
x = render(osc_2_on=True, osc_2_octave=1, osc_2_level=0.0); passed('osc 2 level 0 is silent', amp_at(x, 440) < 0.005)
x = render(osc_2_on=True, osc_2_octave=-1); passed('osc 2 -1 octave adds 110 Hz', amp_at(x, 110) > 0.25)
x = render(osc_2_on=True, osc_2_semi=7, osc_2_fine=0.0); passed('osc 2 +7 semitones adds a fifth (329.6 Hz)', amp_at(x, 220 * 2 ** (7 / 12), 0.004) > 0.25, '(%.3f)' % amp_at(x, 220 * 2 ** (7 / 12), 0.004))
x = render(osc_2_on=True, osc_2_semi=-12, osc_2_octave=2); passed('octaves and semitones add: +2 oct -12 st = +1 octave', amp_at(x, 440) > 0.25)
x = render(osc_2_on=True, osc_2_fine=50.0); fr = 220 * 2 ** (50 / 1200); passed('osc 2 fine +50 ct is a quarter-tone up', amp_at(x, fr, 0.002) > 0.25 and amp_at(x, 220, 0.002) > 0.25, '(%.1f Hz)' % fr)
x = render(osc_2_on=True, osc_2_wave='Square', osc_2_octave=1); passed('osc 2 square wave brings odd harmonics (1320 Hz)', amp_at(x, 1320) > 0.03)
x = render(osc_2_on=True, osc_2_wave='Sine', osc_2_octave=1); passed('osc 2 sine has none', amp_at(x, 1320) < 0.005)
x = render(osc_3_on=True, osc_3_octave=1); passed('osc 3 works on its own (440 Hz)', amp_at(x, 440) > 0.25)
x = render(osc_2_on=True, osc_2_octave=1, osc_3_on=True, osc_3_octave=1, osc_3_semi=7); a = (amp_at(x, 220), amp_at(x, 440), amp_at(x, 220 * 2 ** (19 / 12), 0.004))
passed('all three together are independent', min(a) > 0.25, '(%.2f %.2f %.2f)' % a)
x = render(note=69, osc_2_on=True, osc_2_octave=1); passed('they follow the keyboard (A4: 440 and 880)', amp_at(x, 440) > 0.25 and amp_at(x, 880) > 0.25)
x = render(osc_2_on=True, osc_2_octave=1, osc_3_on=True, osc_3_octave=2, osc_level=0.0); passed('the main oscillator level is the master for all three (zero is silence)', np.abs(x).max() < 1e-6)
x = render(osc_2_on=True, osc_2_octave=1, osc_level=0.15); passed('lowering it lowers the layers too (half level: 440 Hz layer halves)', abs(amp_at(x, 440) / amp_at(render(osc_2_on=True, osc_2_octave=1), 440) - 0.5) < 0.05)
x = render(osc_2_on=True, osc_2_octave=1, filter_on=True, cutoff_hz=300.0); passed('they go through the filter', amp_at(x, 440) < 0.5 * amp_at(x, 220))
print(f"\nRESULT: {sum(rows)} of {len(rows)} oscillator 2/3 checks passed")
