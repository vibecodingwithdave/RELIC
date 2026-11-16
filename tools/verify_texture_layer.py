"""Texture layer (headless, Pedalboard, synthetic test audio): the texture's own file, pitch, envelope, filter and routing, plus saved-state round trips
and projects saved before these controls existed. Grains of a pure tone sound at the tone's pitch, so pitch and source are easy to check."""
import sys, os, numpy as np
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]; sys.path.insert(0, str(ROOT / 'tools'))
from pedalboard import load_plugin
from state_tools import edit_state
PATH = str(ROOT / 'build/Relic_artefacts/Release/VST3/Relic.vst3'); ASSETS = str(ROOT / 'tools/test_assets'); SR = 44100
SINE = ASSETS + '/sine440.wav'; SINE2 = ASSETS + '/sine660.wav'
rows = []
def check(name, ok, note=''): rows.append(bool(ok)); print(f"  [{'PASS' if ok else 'FAIL'}] {name} {note}", flush=True)
# the A sample is silenced (gain 0) so that only the texture is heard; grains are fixed in place (no spread or jitter) and dense
BASE = dict(synth_mix=0.0, sample_gain=0.0, sample_root=69, sample_octave=0, sample_normalize=0, tex_level=0.8, tex_pos=0.25, tex_spread=0.0, tex_size_ms=120.0, tex_density=40.0, tex_jitter_ct=0.0,
            tex_normalize=0, tex_root=69, attack_ms=0.5, decay_ms=0.0, sustain=1.0, release_ms=30.0, filter_on=0, master=1.0, voices=4, unison_voices=1, chop_on=0)
def plugin(params=None, wav=SINE, tex=None, drop=None):
    P = load_plugin(PATH); P.raw_state = edit_state(P.raw_state, {**BASE, **(params or {})}, wav, {'samplePathT': tex} if tex else None, drop); P.reset(); return P
def render(P, hold=2.2, note=69, dur=2.6):
    return P([(bytes([0x90, note, 127]), 0.0), (bytes([0x80, note, 0]), hold)], duration=dur, sample_rate=SR, num_channels=2).mean(axis=0)
def amp(x, f, a=0.4, b=1.6):
    s = x[int(a * SR):int(b * SR)]; w = np.hanning(len(s)); X = np.abs(np.fft.rfft(s * w, 1 << 17)); fr = np.fft.rfftfreq(1 << 17, 1 / SR)
    return float(2 * X[(fr > f - 12) & (fr < f + 12)].max() / w.sum())
def peakfreq(x, a=0.4, b=1.6):
    s = x[int(a * SR):int(b * SR)]; w = np.hanning(len(s)); X = np.abs(np.fft.rfft(s * w, 1 << 18)); return float(np.fft.rfftfreq(1 << 18, 1 / SR)[np.argmax(X)])
def rms(x, a, b): s = x[int(a * SR):int(b * SR)]; return float(np.sqrt(np.mean(s ** 2)))
OWN = dict(tex_indep=1)
NEW = ['tex_indep', 'tex_root', 'tex_octave', 'tex_tune_ct', 'tex_normalize', 'tex_env_own', 'tex_attack_ms', 'tex_decay_ms', 'tex_sustain', 'tex_release_ms',
       'tex_filter_on', 'tex_filter_type', 'tex_cutoff_hz', 'tex_reso', 'tex_route']

# ---- defaults, saved state, old projects
P0 = load_plugin(PATH)
check('defaults: own file off, root 60, no tuning, own envelope off, filter off, route "With sample"',
      not bool(P0.texture_own_file) and P0.texture_root == 60 and P0.texture_octave == 0 and abs(P0.texture_tune_ct) < 1e-6 and not bool(P0.texture_env_own)
      and not bool(P0.texture_filter_on) and str(P0.texture_route) == 'With sample' and abs(P0.texture_cutoff_hz - 8000) < 5 and bool(P0.texture_normalize))
Pa = plugin(dict(**OWN, tex_root=55, tex_octave=1, tex_tune_ct=-23, tex_env_own=1, tex_attack_ms=111, tex_decay_ms=22, tex_sustain=0.33, tex_release_ms=444,
                 tex_filter_on=1, tex_filter_type=2, tex_cutoff_hz=1234, tex_reso=0.4, tex_route=1), tex=SINE2); Pb = load_plugin(PATH); Pb.raw_state = Pa.raw_state
check('every texture setting survives a save and restore',
      bool(Pb.texture_own_file) and Pb.texture_root == 55 and Pb.texture_octave == 1 and abs(Pb.texture_tune_ct + 23) < 0.1 and bool(Pb.texture_env_own) and abs(Pb.texture_attack_ms - 111) < 1
      and abs(Pb.texture_decay_ms - 22) < 1 and abs(Pb.texture_sustain - 0.33) < 1e-3 and abs(Pb.texture_release_ms - 444) < 2 and bool(Pb.texture_filter_on) and str(Pb.texture_filter_type) == 'BP'
      and abs(Pb.texture_cutoff_hz - 1234) < 3 and abs(Pb.texture_reso - 0.4) < 1e-3 and str(Pb.texture_route) == 'Dry')
Pc = load_plugin(PATH); Pc.raw_state = plugin(dict(**OWN, tex_root=69), tex=SINE2).raw_state
check('the texture file is part of the saved state (a restored instance still plays it, at 660 Hz)', abs(peakfreq(render(Pc)) - 660) < 6, f'({peakfreq(render(Pc)):.0f} Hz)')
x_old = render(plugin({}, drop=NEW)); x_def = render(plugin({}))
check('a project saved before these controls existed renders the same as the defaults', np.allclose(x_old, x_def, atol=1e-6))
x_a = render(plugin({})); x_b = render(plugin(dict(tex_root=30, tex_octave=2, tex_tune_ct=60, tex_normalize=1, tex_env_own=1, tex_attack_ms=900, tex_sustain=0.0)))
check('with Own File off, the own pitch, level and envelope settings do nothing (the classic texture is unchanged)', np.allclose(x_a, x_b, atol=1e-6))

# ---- source
check('classic texture: grains come from sample A (440 Hz)', abs(peakfreq(x_a) - 440) < 6 and amp(x_a, 440) > 0.05, f'({peakfreq(x_a):.0f} Hz)')
x_t = render(plugin(OWN, tex=SINE2))
check('Own File on: grains come from the texture file (660 Hz) and not from sample A', abs(peakfreq(x_t) - 660) < 6 and amp(x_t, 440) < 0.1 * amp(x_t, 660), f'({peakfreq(x_t):.0f} Hz)')
x_n = render(plugin(OWN, wav=None, tex=SINE2))
check('the texture plays on its own when no sample is loaded in slot A', abs(peakfreq(x_n) - 660) < 6 and amp(x_n, 660) > 0.05)
check('Own File on with no texture file loaded is silent (no crash)', np.isfinite(render(plugin(OWN))).all() and rms(render(plugin(OWN)), 0.4, 1.6) < 1e-4)
# ---- pitch
xo = render(plugin({**OWN, 'tex_octave': 1}, tex=SINE2)); xr = render(plugin({**OWN, 'tex_root': 57}, tex=SINE2)); xt = render(plugin({**OWN, 'tex_tune_ct': 100}, tex=SINE2))
check('texture octave +1 and root 57 both play the file an octave up (1320 Hz)', abs(peakfreq(xo) - 1320) < 10 and abs(peakfreq(xr) - 1320) < 10, f'({peakfreq(xo):.0f}, {peakfreq(xr):.0f} Hz)')
check('texture tune +100 cents plays the file a semitone up (699.3 Hz)', abs(peakfreq(xt) - 660 * 2 ** (100 / 1200)) < 6, f'({peakfreq(xt):.1f} Hz)')
xa2 = render(plugin({**OWN, 'sample_root': 57, 'sample_octave': 1}, tex=SINE2)); xc2 = render(plugin({'sample_root': 57}))
check('the texture\'s pitch is independent of sample A\'s root, while the classic texture follows it', abs(peakfreq(xa2) - 660) < 6 and abs(peakfreq(xc2) - 880) < 8, f'(own {peakfreq(xa2):.0f} Hz, classic {peakfreq(xc2):.0f} Hz)')
xn1 = render(plugin({**OWN, 'tex_normalize': 1}, tex=SINE2)); xn0 = render(plugin({**OWN, 'tex_normalize': 0}, tex=SINE2))
check('texture normalize raises the file to full scale (the 0.8-peak tone is 1.25x louder)', abs(amp(xn1, 660) / amp(xn0, 660) - 1.25) < 0.12, f'(x{amp(xn1,660)/amp(xn0,660):.2f})')
# ---- envelope
st = rms(x_t, 0.5, 1.5)
xe = render(plugin({**OWN, 'tex_env_own': 1, 'tex_attack_ms': 500, 'tex_decay_ms': 0, 'tex_sustain': 1.0}, tex=SINE2))
check('texture attack 500 ms: quiet at the start, full level after it', rms(xe, 0.0, 0.1) < 0.3 * rms(xe, 1.0, 1.8) and rms(xe, 1.0, 1.8) > 0.7 * st, f'(start {rms(xe,0,0.1):.3f}, later {rms(xe,1.0,1.8):.3f})')
xe = render(plugin({**OWN, 'tex_env_own': 1, 'tex_attack_ms': 2, 'tex_decay_ms': 60, 'tex_sustain': 0.25}, tex=SINE2))
check('texture decay to sustain 0.25 settles at about a quarter of the level', abs(rms(xe, 1.0, 1.8) / rms(x_t, 1.0, 1.8) - 0.25) < 0.07, f'(x{rms(xe,1.0,1.8)/rms(x_t,1.0,1.8):.2f})')
xe = render(plugin({**OWN, 'tex_env_own': 1, 'tex_attack_ms': 2, 'tex_decay_ms': 0, 'tex_sustain': 1.0, 'tex_release_ms': 60, 'release_ms': 800}, tex=SINE2), hold=1.5, dur=2.4)
xs = render(plugin({**OWN, 'release_ms': 800}, tex=SINE2), hold=1.5, dur=2.4)
check('texture release 60 ms ends quickly after the key is let go, while without it the voice release (800 ms) carries on', rms(xe, 1.75, 1.95) < 0.1 * rms(xe, 0.8, 1.4) and rms(xs, 1.75, 1.95) > 0.3 * rms(xs, 0.8, 1.4))
# ---- own filter
ref = amp(x_t, 660)
def filt(**kw): return amp(render(plugin({**OWN, 'tex_filter_on': 1, **kw}, tex=SINE2)), 660)
check('texture low-pass at 300 Hz removes the 660 Hz tone, at 3 kHz leaves it', filt(tex_filter_type=0, tex_cutoff_hz=300) < 0.35 * ref and filt(tex_filter_type=0, tex_cutoff_hz=3000) > 0.8 * ref)
check('texture high-pass at 3 kHz removes it, at 300 Hz leaves it', filt(tex_filter_type=1, tex_cutoff_hz=3000) < 0.2 * ref and filt(tex_filter_type=1, tex_cutoff_hz=300) > 0.7 * ref)
check('texture band-pass at 660 Hz leaves it, at 3 kHz removes it', filt(tex_filter_type=2, tex_cutoff_hz=660, tex_reso=0.0) > 0.4 * ref and filt(tex_filter_type=2, tex_cutoff_hz=3000, tex_reso=0.0) < 0.4 * ref)
xc = render(plugin({**OWN, 'tex_filter_on': 1, 'tex_filter_type': 0, 'tex_cutoff_hz': 300}))   # classic texture (from A, 440 Hz), filtered
check('the texture filter also works on the classic texture (440 Hz removed by a 300 Hz low-pass)', amp(xc, 440) < 0.5 * amp(x_a, 440))
# ---- routing
vf = dict(filter_on=1, filter_type=0, cutoff_hz=200, resonance=0.0, filter_source=0)
xw = render(plugin({**OWN, **vf, 'tex_route': 0}, tex=SINE2)); xd = render(plugin({**OWN, **vf, 'tex_route': 1}, tex=SINE2))
check('route "With sample": the voice filter (low-pass 200 Hz) removes the texture', amp(xw, 660) < 0.15 * ref, f'(x{amp(xw,660)/ref:.2f})')
check('route "Dry": the texture skips the voice filter and keeps its level', amp(xd, 660) > 0.8 * ref, f'(x{amp(xd,660)/ref:.2f})')
xdf = render(plugin({**OWN, **vf, 'tex_route': 1, 'tex_filter_on': 1, 'tex_filter_type': 0, 'tex_cutoff_hz': 300}, tex=SINE2))
check('routed dry, the texture\'s own filter still applies (low-pass 300 Hz)', amp(xdf, 660) < 0.35 * ref)
# ---- chop and stress
xch = render(plugin({**OWN, 'chop_on': 1, 'chop_mode': 0, 'chop_count': 4}, tex=SINE2), note=36, hold=1.0, dur=1.4)
check('chop pads do not play the texture (it stays out of chop mode, as before)', amp(xch, 660, 0.1, 0.9) < 0.02)
Ps = plugin({**OWN, 'tex_env_own': 1, 'tex_filter_on': 1, 'tex_route': 1, 'voices': 16, 'tex_density': 80.0}, tex=SINE2); ev = []
for i, n in enumerate((48, 52, 55, 59, 62, 66, 69, 72, 76, 79)): ev += [(bytes([0x90, n, 120]), 0.04 * i), (bytes([0x80, n, 0]), 1.4)]
xs = Ps(ev, duration=2.0, sample_rate=SR, num_channels=2)
check('ten notes with envelope, filter and dry routing at high density: finite and never above full scale', np.isfinite(xs).all() and np.abs(xs).max() <= 1.0 + 1e-6, f'(peak {np.abs(xs).max():.3f})')

print(f"\nRESULT: {sum(rows)} of {len(rows)} texture-layer checks passed")
sys.exit(0 if all(rows) else 1)
