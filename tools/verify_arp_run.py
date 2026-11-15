"""One-cycle STOP audio/wiring and saved-state checks; exact events use the DSP harness."""
from pathlib import Path
import struct
import sys
import xml.etree.ElementTree as ET
import numpy as np
from pedalboard import load_plugin
from state_tools import edit_state, unwrap, rewrap

PATH = str(Path(__file__).resolve().parents[1] / 'build/Relic_artefacts/Release/VST3/Relic.vst3')
SR = 44100
rows = []
def check(name, ok):
    rows.append(bool(ok))
    print(f"  [{'PASS' if ok else 'FAIL'}] {name}", flush=True)

def plugin(run=True, arp=True, **settings):
    p = load_plugin(PATH)
    assert 'arp_run' in p.parameters
    values = dict(
        arp_run=int(run), arp_on=int(arp), arp_hold=0, arp_div=7,
        arp_mode=0, arp_order=0, arp_octaves=1, arp_grouping=0,
        arp_gate=0.3, arp_swing=0, arp_human=0,
        chord_on=0, waveform=0, filter_on=0, synth_mix=1,
        osc_level=0.3, master=1, attack_ms=0.5, decay_ms=0,
        sustain=1, release_ms=6)
    values.update(settings)
    p.raw_state = edit_state(p.raw_state, values)
    return p

def keys(notes, t, on=True):
    return [(bytes([0x90 if on else 0x80, n, 100 if on else 0]), t + i * 0.00001) for i, n in enumerate(notes)]

def render(p, extra=(), duration=6.0):
    # Clear discovery notes, then isolate the actual gesture in a single render.
    ev = keys(range(128), 0, False) + keys([60,64,67], 0.75) + list(extra)
    ev += keys(range(128), duration - 0.1, False)
    return p(sorted(ev, key=lambda e: e[1]), duration=duration,
             sample_rate=SR, num_channels=2).mean(axis=0)

def onsets(audio, start=0.7):
    energy = np.sqrt(np.convolve(audio ** 2, np.ones(176) / 176, 'same'))
    threshold = energy.max() * 0.2
    if threshold < 1e-8:
        return []
    idx = np.where((energy[1:] > threshold) & (energy[:-1] <= threshold))[0] + 1
    out = []
    for i in idx:
        t = float(i / SR)
        if t >= start and (not out or t - out[-1] > 0.05):
            out.append(t)
    return out

def quiet_tail(audio):
    return float(np.sqrt(np.mean(audio[int(4.8 * SR):int(5.5 * SR)] ** 2))) < 1e-7

p = load_plugin(PATH)
check('Arp Run is exposed and defaults to GO', 'arp_run' in p.parameters and bool(p.arp_run))
for mode, count in enumerate([3,3,4,4,3,3,1,8]):
    audio = render(plugin(run=False, arp_mode=mode))
    check(f'STOP mode {mode}: {count} hits, then silence while held', len(onsets(audio)) == count and quiet_tail(audio))
for group in range(1,5):
    audio = render(plugin(run=False, arp_mode=7, arp_grouping=group))
    check(f'STOP grouped Waterfall {group}: 16 hits, then silence', len(onsets(audio)) == 16 and quiet_tail(audio))
audio = render(plugin(run=False, arp_octaves=2))
check('STOP includes the octave-expanded pattern', len(onsets(audio)) == 6 and quiet_tail(audio))
audio = render(plugin(run=False), keys([72], 2.5))
check('adding a key after completion does not re-arm', len(onsets(audio)) == 3 and quiet_tail(audio))
audio = render(plugin(run=False), keys([72], 1.1))
check('adding a key during playback extends the current pass', len(onsets(audio)) == 4 and quiet_tail(audio))
audio = render(plugin(run=False), keys([60,64,67], 2.5, False) + keys([60,64,67], 3.25))
check('release all then press plays exactly one more pass', len(onsets(audio)) == 6 and quiet_tail(audio))
audio = render(plugin(run=True))
check('GO continues looping while held', len(onsets(audio)) > 12 and not quiet_tail(audio))
audio = render(plugin(run=False, arp=False))
check('STOP with ARP bypassed still sustains keys', not quiet_tail(audio))

p = plugin(run=False)
# Cache the parameter before the stream; avoid parameter discovery between buffers.
run_parameter = p.parameters['arp_run']
ev = keys(range(128), 0, False) + keys([60,64,67], 0.75)
p(ev, duration=2.5, sample_rate=SR, num_channels=2)
run_parameter.raw_value = 1.0
audio = p(keys([60,64,67], 1.15, False), duration=1.3,
          sample_rate=SR, num_channels=2, reset=False).mean(axis=0)
times = onsets(audio, start=0)
check('GO resumes the held chord and loops without new note-ons',
      len(times) == 5 and abs(times[0] - 0.025) < 0.01)

p = plugin(run=False)
saved = bytes(p.raw_state)
q = plugin()
q.raw_state = saved
check('STOP survives a project state round trip', not bool(q.arp_run))
_, comp, size = unwrap(saved)
root = ET.fromstring(comp[8:8 + size].rstrip(b'\0'))
for child in list(root):
    if child.get('id') == 'arp_run':
        root.remove(child)
inner = ET.tostring(root) + b'\0'
q.raw_state = rewrap(saved, b'VC2!' + struct.pack('<I', len(inner)) + inner + comp[8 + size:])
check('older projects without Arp Run restore GO', bool(q.arp_run))
print(f'RESULT: {sum(rows)} of {len(rows)} arp run plugin checks passed')
sys.exit(0 if all(rows) else 1)
