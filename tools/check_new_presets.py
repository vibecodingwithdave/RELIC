import os, sys, re, numpy as np, xml.etree.ElementTree as ET
sys.path.insert(0, os.path.dirname(__file__))
from pedalboard import load_plugin
from state_tools import edit_state
SR = 44100; D = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'Assets', 'presets')
names = sys.argv[1:] or None
rows = []
for f in sorted(os.listdir(D)):
    n = f[:-4].replace('_', ' ')
    if names and n not in names: continue
    root = ET.parse(os.path.join(D, f)).getroot(); params = {p.get('id'): float(p.get('value')) for p in root.findall('PARAM')}
    P = load_plugin(str(__import__('pathlib').Path(__file__).resolve().parents[1] / 'build/Relic_artefacts/Release/VST3/Relic.vst3')); P.raw_state = edit_state(P.raw_state, params)
    ev = []
    for k in (48, 52, 55): ev += [(bytes([0x90, k, 100]), 0.0), (bytes([0x80, k, 0]), 1.6)]
    x = P(ev, duration=3.2, sample_rate=SR, num_channels=2).mean(axis=0)
    db = lambda s: 20 * np.log10(np.sqrt(np.mean(s ** 2)) + 1e-9)
    early, late = db(x[int(.05 * SR):int(.5 * SR)]), db(x[int(1.0 * SR):int(1.5 * SR)])
    rows.append((n, float(np.abs(x).max()), early, late))
    print(f"{n:20s} peak {np.abs(x).max():.2f}  rms early {early:6.1f}  held {late:6.1f}", '  <-- check' if np.abs(x).max() > .95 or max(early, late) < -26 or max(early, late) > -10 else '')
