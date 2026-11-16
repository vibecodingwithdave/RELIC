"""Partial preset types (headless, Pedalboard): a state tagged with a part (Synth, Sample, Modulation, Effects, Arp & Chords) changes only that part's controls.
Checks that every one of the plugin's controls belongs to exactly one part, that a part leaves everything else untouched, that controls the file omits fall back to
their defaults, that samples follow their part, and that the result sounds like the equivalent full state. (Saving a part and the browser's TYPE filter are on screen only.)"""
import sys, re, glob, tempfile, os, numpy as np
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]; sys.path.insert(0, str(ROOT / 'tools'))
from pedalboard import load_plugin
from state_tools import edit_state, unwrap
PATH = str(ROOT / 'build/Relic_artefacts/Release/VST3/Relic.vst3'); SR = 44100; rows = []
def check(name, ok, note=''): rows.append(bool(ok)); print(f"  [{'PASS' if ok else 'FAIL'}] {name} {note}", flush=True)
def values(raw):
    xml, comp, size = unwrap(bytes(raw)); inner = comp[8:8 + size].decode().rstrip('\0'); return {k: float(v) for k, v in re.findall(r'<PARAM id="([^"]+)" value="([^"]+)"', inner)}
# the expected parts, written independently of the plugin's own rule
EFFECTS = ('trem_', 'delay_', 'comp_', 'crush_', 'pan_', 'chorus_', 'flanger_', 'reverb_', 'eq_', 'tape_', 'widener_', 'wah_', 'lofi_', 'ott_', 'pdelay_', 'talk_', 'sgate_')
MOD = ('lfo1_', 'lfo2_', 'menv1_', 'menv2_', 'macro_', 'xy_') + tuple(f'mod{i}_' for i in range(1, 9))
SAMPLE = ('sample_', 'sampleb_', 'loop_', 'tex_', 'chop_', 'split_')
NOTES = ('arp_', 'chord_', 'voice_lead')
def part_of(i):
    if i.startswith(EFFECTS): return 'Effects'
    if i.startswith(MOD): return 'Modulation'
    if i.startswith(SAMPLE): return 'Sample'
    if i.startswith(NOTES): return 'Arp & Chords'
    return 'Synth'
PARTS = ['Synth', 'Sample', 'Modulation', 'Effects', 'Arp & Chords']
fresh = load_plugin(PATH); DEF = values(fresh.raw_state); IDS = sorted(DEF)
check('every control is assigned to a part (463 controls, five parts, none left over)', len(IDS) == 463 and {part_of(i) for i in IDS} == set(PARTS), f'({ {p: sum(part_of(i) == p for i in IDS) for p in PARTS} })')
# a different value for every control: taken from the factory presets, or set by hand where no preset moves it
seen = {}
for f in glob.glob(str(ROOT / 'Assets/presets/*.xml')):
    for k, v in re.findall(r'<PARAM id="([^"]+)" value="([^"]+)"', Path(f).read_text(encoding='utf-8')):
        if k in DEF and abs(float(v) - DEF[k]) > 1e-6: seen.setdefault(k, float(v))
HAND = {'_sync': 2, '_slice': 3, '_pitch': 5, '_rev': 1, '_level': -3.0, '_choke': 2, 'vib_delay_ms': 100.0, 'vib_fade_ms': 100.0, 'wt_use_file': 1}
V = {}
for i in IDS:
    if i in seen: V[i] = seen[i]
    else:
        for suf, val in HAND.items():
            if (i.endswith(suf) and (suf == '_sync' or i.startswith('chop_kit_'))) or i == suf: V[i] = float(val); break
def readback(params):
    P = load_plugin(PATH); P.raw_state = edit_state(P.raw_state, params); return values(P.raw_state)
for cand in (lambda d: d + 1, lambda d: d - 1, lambda d: d + 0.5, lambda d: d - 0.5, lambda d: 0.0, lambda d: 1.0, lambda d: 5.0):       # the plugin clamps each to its range; keep what moved
    todo = [i for i in IDS if i not in V]
    if not todo: break
    got = readback({i: cand(DEF[i]) for i in todo})
    for i in todo:
        if abs(got[i] - DEF[i]) > 1e-4: V[i] = got[i]
missing = [i for i in IDS if i not in V]
check('every control has a test value different from its default', not missing, f'(none for: {missing[:12]})')
BASEV = readback(V)        # what the plugin holds once those values are set (it clamps to each control's range)
def make(base_raw, params, part=None, drop_outside=False, only=None, attrs=None):
    a = dict(attrs or {})
    if part: a['partType'] = part
    d = [i for i in IDS if (only is not None and i not in only) or (drop_outside and part_of(i) != part)] if (only is not None or drop_outside) else None
    return edit_state(base_raw, params, None, a, d)
def loaded(raw):
    P = load_plugin(PATH); P.raw_state = raw; P.reset(); return P
def after(base_params, partial_raw):
    P = load_plugin(PATH); P.raw_state = edit_state(P.raw_state, base_params); P.reset(); P.raw_state = partial_raw; return values(P.raw_state), P
print('== A PART CHANGES ONLY ITS OWN CONTROLS ==')
base_state = edit_state(fresh.raw_state, V)        # everything at its test value
for part in PARTS:
    # the file carries a value for EVERY control, but is tagged as one part: only that part's controls may take effect (here: all at their defaults)
    partial = edit_state(base_state, {i: DEF[i] for i in IDS}, None, {'partType': part})
    got, _ = after(V, partial)
    moved = [i for i in IDS if abs(got[i] - BASEV[i]) > 1e-4]
    expect = [i for i in IDS if part_of(i) == part and abs(BASEV[i] - DEF[i]) > 1e-4]
    check(f'{part}: exactly its {len(expect)} controls went to the file\'s values, all {len(IDS) - len(expect)} others kept theirs', sorted(moved) == sorted(expect) and len(expect) > 0, f'(moved {len(moved)}, expected {len(expect)}; stray: {sorted(set(moved) ^ set(expect))[:6]})')
print('== CONTROLS THE FILE LEAVES OUT FALL BACK TO THEIR DEFAULTS ==')
for part in ('Effects', 'Modulation'):
    keep = [i for i in IDS if part_of(i) == part][:1]
    partial = edit_state(base_state, {}, None, {'partType': part}, [i for i in IDS if i not in keep])
    got, _ = after(V, partial); mine = [i for i in IDS if part_of(i) == part and i not in keep]
    check(f'{part}: a file with one control leaves the part\'s others at their defaults', all(abs(got[i] - DEF[i]) < 1e-4 for i in mine) and all(abs(got[i] - BASEV[i]) < 1e-4 for i in IDS if part_of(i) != part))
print('== A FULL STATE IS UNCHANGED ==')
full = edit_state(fresh.raw_state, V); got, _ = after({}, full)
check('a state without a type still replaces every control', all(abs(got[i] - BASEV[i]) < 1e-4 for i in IDS))
got, _ = after(V, edit_state(base_state, {}, None, {'partType': 'Nonsense'})); check('an unknown type is read as a full preset', all(abs(got[i] - BASEV[i]) < 1e-4 for i in IDS))
print('== SOUND ==')
SINE = str(ROOT / 'tools/test_assets/sine440.wav')
BASE = dict(waveform=0, osc_level=0.5, unison_voices=1, synth_mix=1, filter_on=0, master=1.0, attack_ms=1, decay_ms=0, sustain=1, release_ms=30, voices=4, chord_on=0, arp_on=0, chop_on=0,
            osc2_on=0, osc3_on=0, vib_on=0)
FXON = dict(delay_on=1, delay_mix=0.6, delay_feedback=0.5, delay_time_ms=180, reverb_on=1, reverb_mix=0.5, trem_on=1, trem_depth=0.8, trem_rate_hz=6.0)
def play(P, dur=2.0):
    P.reset(); return P([(bytes([0x90, 69, 110]), 0.0), (bytes([0x80, 69, 0]), 1.0)], duration=dur, sample_rate=SR, num_channels=2)
def build(params):
    P = load_plugin(PATH); P.raw_state = edit_state(P.raw_state, params); P.reset(); return P
wet = build({**BASE, **FXON}); dry = build(BASE)
fx_off = {i: DEF[i] for i in IDS if part_of(i) == 'Effects'}
a = play(wet); b = play(dry)
check('the wet and dry sounds differ (non-vacuous)', np.abs(a - b).max() > 0.02)
P = build({**BASE, **FXON}); P.raw_state = edit_state(P.raw_state, fx_off, None, {'partType': 'Effects'}); c = play(P)
check('loading an Effects part with everything off gives the dry sound, synth settings kept', np.abs(c - b).max() < 1e-5, f'(diff {np.abs(c - b).max():.1e})')
P = build({**BASE, **FXON}); P.raw_state = edit_state(P.raw_state, {'waveform': 1, 'osc_level': 0.2}, None, {'partType': 'Effects'}, [i for i in IDS if part_of(i) != 'Effects']); d = play(P)
check('a part ignores controls that are not its own (waveform in an Effects file)', np.abs(d - a).max() < 1e-5)
P = build({**BASE, **FXON}); P.raw_state = edit_state(P.raw_state, {**{i: DEF[i] for i in IDS if part_of(i) == 'Synth'}, **BASE, 'waveform': 2}, None, {'partType': 'Synth'}); e = play(P)
ref = build({**BASE, **FXON, 'waveform': 2}); r = play(ref)
check('a Synth part changes the oscillator and keeps the effects', np.abs(e - r).max() < 1e-4 and np.abs(e - a).max() > 0.02, f'(diff {np.abs(e - r).max():.1e})')
print('== SAMPLES FOLLOW THEIR PART ==')
S = dict(BASE, osc_level=0.0, synth_mix=0.0, sample_gain=1.0, sample_root=69, sample_loop=1, sample_normalize=1)
def sampled(params):
    P = load_plugin(PATH); P.raw_state = edit_state(P.raw_state, params, SINE); P.reset(); return P
rms = lambda x: float(np.sqrt(np.mean(x ** 2)))
P = sampled(S); loud = rms(play(P)[:, :int(0.9 * SR)])
P.raw_state = edit_state(P.raw_state, {'delay_on': 0}, None, {'partType': 'Effects'}, [i for i in IDS if part_of(i) != 'Effects']); check('an Effects part keeps the loaded sample', rms(play(P)[:, :int(0.9 * SR)]) > 0.5 * loud and loud > 0.01, f'({rms(play(P)[:, :int(0.9 * SR)]):.3f} vs {loud:.3f})')
P = sampled(S); P.raw_state = edit_state(P.raw_state, {}, None, {'partType': 'Sample', 'samplePath': ''}, [i for i in IDS if part_of(i) != 'Sample']); check('a Sample part with no file clears the sample (as a full preset does)', rms(play(P)[:, :int(0.9 * SR)]) < 1e-4)
P = sampled(S); P.raw_state = edit_state(P.raw_state, {}, SINE, {'partType': 'Sample'}, [i for i in IDS if part_of(i) != 'Sample']); check('a Sample part with a file loads it', rms(play(P)[:, :int(0.9 * SR)]) > 0.5 * loud)
P = build(S); P.raw_state = edit_state(P.raw_state, {**{i: DEF[i] for i in IDS if part_of(i) == 'Synth'}, **S}, SINE, {'partType': 'Synth'}, [i for i in IDS if part_of(i) != 'Synth']); check('a Synth part does not bring in a sample path', rms(play(P)[:, :int(0.9 * SR)]) < 1e-4)
print(f'{sum(rows)}/{len(rows)} passed'); sys.exit(0 if all(rows) else 1)
