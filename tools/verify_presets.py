"""Check every preset file against the plugin's real parameters, and render a note with a sample of them."""
import sys, os, glob, random, math, numpy as np
import xml.etree.ElementTree as ET
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pedalboard import load_plugin
from state_tools import edit_state
P = load_plugin(str(__import__('pathlib').Path(__file__).resolve().parents[1] / 'build/Relic_artefacts/Release/VST3/Relic.vst3'))
files = sorted(glob.glob(os.path.expanduser('~/Library/Relic/Presets/**/*.xml'), recursive=True))
bad = []; unknown = set(); nparam = 0
for f in files:
    root = ET.parse(f).getroot()
    for e in root.findall('PARAM'):
        k, v = e.get('id'), float(e.get('value')); nparam += 1
        if k not in [n for n in P.parameters] and k not in {'delay_ping_pong'} and k not in PARAMIDS if False else False: pass
print(len(files), 'preset files,', nparam, 'parameter values')
ids = set(P.parameters.keys())
# pedalboard names ids differently (snake_case of the display name); build the reverse map from display names in a state dump
import re
from state_tools import unwrap
xml, comp, size = unwrap(bytes(P.raw_state)); st = ET.fromstring(comp[8:8 + size].rstrip(b'\0').decode()); stateids = {e.get('id') for e in st.findall('PARAM')}
RANGE = {}
for name, prm in P.parameters.items():
    pass
bad = []; unknown = collections = None
from collections import Counter
unk = Counter(); oor = []
for f in files:
    root = ET.parse(f).getroot()
    for e in root.findall('PARAM'):
        k, v = e.get('id'), float(e.get('value'))
        if k not in stateids: unk[k] += 1
print('unknown parameter ids:', dict(unk) if unk else 'none')
random.seed(3); factory = [f for f in files if '/Factory/' in f]; imported = [f for f in files if '/Imported/' in f]
sample = factory + random.sample(imported, 40); silent = []; nan = []; peak = []
for f in sample:
    P = load_plugin(str(__import__('pathlib').Path(__file__).resolve().parents[1] / 'build/Relic_artefacts/Release/VST3/Relic.vst3'))     # fresh plugin each time so one preset's settings never leak into the next
    root = ET.parse(f).getroot(); params = {e.get('id'): float(e.get('value')) for e in root.findall('PARAM')}
    P.raw_state = edit_state(P.raw_state, params, root.get('samplePath') or '')
    P.reset()
    out = P([(bytes([0x90, 64, 110]), 0.0), (bytes([0x80, 64, 0]), 1.2)], duration=2.0, sample_rate=44100, num_channels=2)
    if not np.isfinite(out).all(): nan.append(f)
    pk = float(np.abs(out).max()); peak.append(pk)
    if pk > 0.9: print('  hot:', os.path.basename(f), round(pk, 3))
    if pk < 1e-4: silent.append(os.path.basename(f))
print(f'rendered {len(sample)} presets ({len(factory)} factory + {len(sample) - len(factory)} imported): non-finite {len(nan)}, silent {len(silent)}, peak min/median/max {min(peak):.3f}/{np.median(peak):.3f}/{max(peak):.3f}')
if silent: print('silent:', silent[:10])
ok = not unk and not nan and len(silent) <= 2
print('RESULT:', 'PASS' if ok else 'FAIL')
