"""Chorus, flanger, reverb: the DSP harness (C++), then the effects inside the real plugin (Pedalboard)."""
import subprocess, sys, os, numpy as np
from pedalboard import load_plugin
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
exe = os.path.join(__import__('tempfile').gettempdir(), 'dsp_modfx' + ('.exe' if os.name == 'nt' else ''))
subprocess.check_call(['clang++', '-std=c++17', '-O2', '-o', exe, os.path.join(HERE, 'dsp_modfx_test.cpp')])
r = subprocess.run([exe], capture_output=True, text=True); print(r.stdout.strip()); ok_dsp = r.returncode == 0
P = load_plugin(os.path.join(ROOT, 'build/Relic_artefacts/Release/VST3/Relic.vst3')); SR = 44100; rows = [ok_dsp]
def passed(n, ok, note=''): rows.append(bool(ok)); print(f"  [{'PASS' if ok else 'FAIL'}] {n} {note}")
BASE = dict(waveform='Saw', filter_on=False, tremolo_on=False, delay_on=False, comp_on=False, crush_on=False, pan_on=False, chorus_on=False, flanger_on=False, reverb_on=False,
            synth_mix=1.0, master=1.0, osc_level=0.2, voices=4, unison_voices=1, attack_ms=1, decay_ms=0, sustain=1.0, release_ms=20)
def render(dur=3.0, off=0.4, **kw):
    for k, v in {**BASE, **kw}.items(): setattr(P, k, v)
    P.reset(); return P([(bytes([0x90, 57, 110]), 0.0), (bytes([0x80, 57, 0]), off)], duration=dur, sample_rate=SR, num_channels=2)
print('== inside the plugin')
dry = render(); tail = lambda x, a, b: float(np.sqrt(np.mean(x[:, int(a * SR):int(b * SR)] ** 2)))
rv = render(reverb_on=True, reverb_size=0.6, reverb_mix=0.4, reverb_predelay_ms=0.0)
passed('dry: silent after the note ends', tail(dry, 1.0, 2.0) < 1e-4, '(%.1e)' % tail(dry, 1.0, 2.0))
passed('reverb: a tail rings on after the note ends', tail(rv, 0.6, 1.2) > 1e-3 and tail(rv, 0.6, 1.2) > 10 * tail(dry, 0.6, 1.2), '(%.1e)' % tail(rv, 0.6, 1.2))
passed('reverb: the tail then dies away', tail(rv, 2.6, 3.0) < 0.5 * tail(rv, 0.6, 1.0), '(%.1e -> %.1e)' % (tail(rv, 0.6, 1.0), tail(rv, 2.6, 3.0)))
passed('reverb: left and right differ', float(np.abs(rv[0] - rv[1]).max()) > 1e-3)
big = render(reverb_on=True, reverb_size=0.9, reverb_mix=0.4); small = render(reverb_on=True, reverb_size=0.1, reverb_mix=0.4)
passed('reverb: bigger size rings longer', tail(big, 1.5, 2.0) > 5 * tail(small, 1.5, 2.0), '(%.1e vs %.1e)' % (tail(big, 1.5, 2.0), tail(small, 1.5, 2.0)))
off = render(reverb_on=False, reverb_size=0.9, reverb_mix=1.0)
passed('reverb off: identical to dry', float(np.abs(off - dry).max()) < 1e-6)
ch = render(chorus_on=True, chorus_mix=0.5, chorus_depth=0.8); fl = render(flanger_on=True, flanger_mix=0.5, flanger_depth=0.8, flanger_feedback=0.6)
sp = lambda x: np.abs(np.fft.rfft(x[0, int(0.1 * SR):int(0.4 * SR)] * np.hanning(int(0.3 * SR))))
passed('chorus: changes the sound and widens the stereo image', float(np.abs(ch[0] - dry[0]).max()) > 0.01 and float(np.abs(ch[0] - ch[1]).max()) > 0.005)
passed('flanger: changes the spectrum', float(np.abs(sp(fl) - sp(dry)).max()) > 0.1 * float(sp(dry).max()))
passed('chorus off and flanger off: identical to dry', float(np.abs(render(chorus_on=False, flanger_on=False) - dry).max()) < 1e-6)
allfx = render(chorus_on=True, flanger_on=True, reverb_on=True, delay_on=True, osc_level=0.5)
passed('everything on: finite and under full scale', bool(np.isfinite(allfx).all()) and float(np.abs(allfx).max()) < 1.0, '(peak %.2f)' % float(np.abs(allfx).max()))
print(f"\nRESULT: {sum(rows)} of {len(rows)} groups/checks passed (DSP harness counts as one)")
sys.exit(0 if all(rows) else 1)
