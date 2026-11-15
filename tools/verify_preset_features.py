"""Showcase factory presets: each one's headline feature (2-HAND, Chance, Spray, grouping, rhythm lane, Waterfall shape, chord voicing,
talk box, pitch delay, spectral gate, mod matrix, second oscillator, OTT, lo-fi ...) must change the sound when it is switched off."""
import sys, re, numpy as np
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from pedalboard import load_plugin
from state_tools import edit_state
PATH = str(ROOT / 'build/Relic_artefacts/Release/VST3/Relic.vst3')
SR = 44100
def params(name):
    s = (ROOT / 'Assets' / 'presets' / f'{name.replace(" ", "_")}.xml').read_text(encoding='utf-8')
    return {m[0]: float(m[1]) for m in re.findall(r'<PARAM id="([^"]+)" value="([^"]+)"', s)}
def render(p, dur=5.0):
    P = load_plugin(PATH); P.raw_state = edit_state(P.raw_state, p); P.reset()
    ev = []
    for k in (48, 52, 55, 59): ev += [(bytes([0x90, k, 100]), 0.3), (bytes([0x80, k, 0]), dur - 0.3)]
    return P(ev, duration=dur, sample_rate=SR, num_channels=2).mean(axis=0)
# preset -> (parameter that is its headline feature, value that turns it off)
tests = {'Two Hand Climb': ('arp_two_hand', 0), 'Two Hand Waterfall': ('arp_two_hand', 0), 'Chance Pluck': ('arp_chance', 1), 'Spray Harp': ('arp_spray', 0),
         'Scatter Bells': ('arp_chance', 1), 'Grouped Cascade': ('arp_grouping', 0), 'Falling Groups': ('arp_grouping', 0), 'Sevens And Two': ('arp_grouping', 0),
         'Syncopated Lane': ('arp_rhythm_on', 0), 'Tied Lane Pad': ('arp_rhythm_on', 0), 'Spiral Waterfall': ('arp_wshape', 0), 'Geyser Chords': ('arp_wshape', 0),
         'Swing Triplets': ('arp_swing', 0), 'Played Order Drift': ('arp_human', 0), 'Wind Chimes': ('arp_spray', 0), 'Open Voicing Pad': ('chord_voicing', 0),
         'Jazz Minor Nine': ('chord_mode', 0), 'Drop Two Keys': ('chord_voicing', 0), 'Lydian Glow': ('chord_strum_ms', 0), 'Choir Bed': ('talk_on', 0),
         'Frozen Shimmer': ('pdelay_on', 0), 'Spectral Mist': ('sgate_on', 0), 'Mod Wheel Pad': ('mod1_amt', 0), 'Tide Drone': ('mod1_amt', 0),
         'Pitch Echo Lead': ('pdelay_on', 0), 'Fifth Lead': ('osc2_on', 0), 'Two Octave Bass': ('osc2_on', 0), 'Envelope Filter Bass': ('mod1_amt', 0),
         'OTT Punch Bass': ('ott_on', 0), 'Lo-Fi Bass': ('lofi_on', 0), 'Robot Choir': ('talk_rate_hz', 0), 'Glass Rain': ('pdelay_on', 0)}
bad = 0
for name, (k, off) in tests.items():
    p = params(name); assert k in p, (name, k)
    a = render(p); b = render({**p, k: off}); n = min(len(a), len(b))
    diff = 20 * np.log10(np.sqrt(np.mean((a[:n] - b[:n]) ** 2)) / (np.sqrt(np.mean(a[:n] ** 2)) + 1e-12) + 1e-12)
    ok = diff > -30
    bad += not ok
    print(f"  [{'PASS' if ok else 'FAIL'}] {name}: turning off {k} changes the sound (by {diff:.1f} dB relative to the preset)")
print(f'\nRESULT: {len(tests) - bad} of {len(tests)} preset feature checks passed')
sys.exit(1 if bad else 0)
