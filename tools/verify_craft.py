"""Check what CRAFT produces. Reads a JSON dump of N crafted parameter sets (written by a temporary hook in the standalone), checks the settings are musical, and renders each one to measure level and brightness."""
import json, os, sys, numpy as np
sys.path.insert(0, os.path.dirname(__file__))
from pedalboard import load_plugin
from state_tools import edit_state
path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(__import__('tempfile').gettempdir(), 'craft.json')
if not os.path.exists(path): print('No CRAFT dump at', path, '- see AGENTS.md (it needs a temporary hook in the standalone to write the 60 rolls); skipping.'); sys.exit(0)
rolls = json.load(open(path)); P = load_plugin(str(__import__('pathlib').Path(__file__).resolve().parents[1] / 'build/Relic_artefacts/Release/VST3/Relic.vst3')); SR = 44100
rows = []
def passed(n, ok, note=''): rows.append(bool(ok)); print(f"  [{'PASS' if ok else 'FAIL'}] {n} {note}")
FX = ['trem', 'delay', 'comp', 'crush', 'pan', 'chorus', 'flanger', 'reverb', 'eq', 'tape', 'widener', 'wah', 'lofi', 'ott', 'pdelay', 'talk', 'sgate']
SAFE = {'reverb', 'delay', 'chorus', 'tape'}
bad_fx = [i for i, r in enumerate(rolls) if any(r[f + '_on'] > 0.5 and f not in SAFE for f in FX)]
passed('only reverb, delay, chorus and tape are ever switched on', not bad_fx, f'(offending rolls: {bad_fx[:5]})')
passed('every roll has at least one of: wavetable/saw/square/triangle/sine (never pluck or bell)', all(int(r['waveform']) <= 4 for r in rolls))
passed('extra oscillators are consonant (semitone 0 or +7, fine tune within 12 cents, never random intervals)', all((r['osc2_on'] < .5 or (r['osc2_semi'] in (0.0, 7.0) and abs(r['osc2_fine']) <= 12.01)) and (r['osc3_on'] < .5 or (r['osc3_semi'] == 0.0 and abs(r['osc3_fine']) <= 12.01)) for r in rolls))
passed('unison detune stays within 20 cents, up to 5 voices', all(r['unison_detune'] <= 20.01 and r['unison_voices'] <= 5 for r in rolls))
passed('filter: low-pass, cutoff 300 Hz or above, resonance at most 0.4', all(r['filter_on'] > .5 and r['filter_type'] == 0 and r['cutoff_hz'] >= 299 and r['resonance'] <= 0.401 for r in rolls))
passed('modulation depth stays gentle (every amount at most 0.25)', all(abs(r[f'mod{k}_amt']) <= 0.2501 for r in rolls for k in (1, 2, 3, 4)))
passed('effect amounts stay moderate (reverb mix at most 0.4, delay mix at most 0.25, chorus mix at most 0.4)', all((r['reverb_on'] < .5 or r['reverb_mix'] <= .4) and (r['delay_on'] < .5 or r['delay_mix'] <= .25) and (r['chorus_on'] < .5 or r['chorus_mix'] <= .4) for r in rolls))
styles = {'pad': 0, 'pluck': 0, 'other': 0}
for r in rolls: styles['pad' if r['attack_ms'] > 200 else ('pluck' if r['sustain'] < 0.3 and r['decay_ms'] < 500 else 'other')] += 1
passed('a good spread of sound types (slow pads, plucks and the rest all appear)', styles['pad'] >= 8 and styles['pluck'] >= 8 and styles['other'] >= 8, str(styles))
passed('rolls differ from each other (at least 55 of 60 are distinct)', len({tuple(round(v, 3) for v in r.values()) for r in rolls}) >= 55)
print('== rendering all rolls as a C minor chord')
def render(r):
    P.raw_state = edit_state(P.raw_state, {k: v for k, v in r.items() if k not in ('master',)})
    P.reset(); ev = []
    for n in (48, 55, 63): ev += [(bytes([0x90, n, 100]), 0.0), (bytes([0x80, n, 0]), 1.6)]
    return P(ev, duration=3.0, sample_rate=SR, num_channels=2).mean(axis=0)
peaks, rmss, cents, quiet = [], [], [], 0
for r in rolls:
    x = render(r); assert np.isfinite(x).all()
    peaks.append(float(np.abs(x).max())); seg = x[int(0.4 * SR):int(1.4 * SR)]; db = lambda s: float(20 * np.log10(np.sqrt(np.mean(s ** 2)) + 1e-9)); rmss.append(max(db(x[int(0.05 * SR):int(0.5 * SR)]), db(x[int(1.1 * SR):int(1.55 * SR)])))   # loudest of the attack and the held part
    X = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))); f = np.fft.rfftfreq(len(seg), 1 / SR); cents.append(float((X * f).sum() / (X.sum() + 1e-9)))
    if float(np.sqrt(np.mean(x[int(0.05 * SR):int(0.4 * SR)] ** 2))) < 1e-3 and r['attack_ms'] < 200: quiet += 1
peaks, rmss, cents = np.array(peaks), np.array(rmss), np.array(cents)
passed('nothing hard-clips (peak under 0.99) and nothing is nearly silent', peaks.max() < 0.99 and peaks.min() > 0.15, '(peaks %.2f to %.2f)' % (peaks.min(), peaks.max()))
passed('loudness is consistent: within 14 dB (plucks decay, so a wider band than held sounds) across all rolls', rmss.max() - rmss.min() < 14, '(%.1f to %.1f dBFS)' % (rmss.min(), rmss.max()))
passed('never harsh: median brightness (spectral centroid) under 3.5 kHz and none over 7 kHz', np.median(cents) < 3500 and cents.max() < 7000, '(median %.0f Hz, max %.0f Hz)' % (np.median(cents), cents.max()))
passed('quick styles are audible straight away', quiet == 0, f'({quiet} silent)')
print(f"\nRESULT: {sum(rows)} of {len(rows)} CRAFT checks passed")
