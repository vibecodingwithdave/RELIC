"""Sampler layers (headless, Pedalboard, synthetic test audio): sample End and tuning for slot A, plus (as it grows) independent slot B controls and
per-slot envelopes. Checks playback boundaries, tuning, saved-state round trips and that projects saved before these controls existed still work."""
import sys, os, numpy as np
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]; sys.path.insert(0, str(ROOT / 'tools'))
from pedalboard import load_plugin
from state_tools import edit_state
PATH = str(ROOT / 'build/Relic_artefacts/Release/VST3/Relic.vst3'); ASSETS = str(ROOT / 'tools/test_assets'); SR = 44100
SINE = ASSETS + '/sine440.wav'; SINE2 = ASSETS + '/sine660.wav'; RAMP = ASSETS + '/ramp.wav'
rows = []
def check(name, ok, note=''): rows.append(bool(ok)); print(f"  [{'PASS' if ok else 'FAIL'}] {name} {note}", flush=True)
BASE = dict(synth_mix=0.0, sample_gain=1.0, sample_root=69, sample_octave=0, sample_loop=0, sample_reverse=0, sample_normalize=0, sample_start=0.0, loop_start=0.0, loop_end=1.0,
            loop_xfade=0, sample_morph=0.0, attack_ms=0.5, decay_ms=0.0, sustain=1.0, release_ms=30.0, filter_on=0, master=1.0, voices=4, unison_voices=1, chop_on=0, tex_level=0.0)
def plugin(params=None, wav=SINE, attrs=None, drop=None):
    P = load_plugin(PATH); P.raw_state = edit_state(P.raw_state, {**BASE, **(params or {})}, wav, attrs, drop); P.reset(); return P
def render(P, hold=2.4, note=69, dur=2.8):
    return P([(bytes([0x90, note, 127]), 0.0), (bytes([0x80, note, 0]), hold)], duration=dur, sample_rate=SR, num_channels=2).mean(axis=0)
def rms(x, a, b): s = x[int(a * SR):int(b * SR)]; return float(np.sqrt(np.mean(s ** 2)))
def freq(x, a=0.3, b=1.3):
    s = x[int(a * SR):int(b * SR)] * np.hanning(int((b - a) * SR)); X = np.abs(np.fft.rfft(s, 1 << 19)); return float(np.fft.rfftfreq(1 << 19, 1 / SR)[np.argmax(X)])
SILENT = 1e-3; LOUD = 0.05

# ---- defaults and saved state
P0 = load_plugin(PATH)
check('Sample End defaults to 1 (full length) and Sample Tune to 0', abs(P0.sample_end - 1.0) < 1e-6 and abs(P0.sample_tune_ct) < 1e-6)
P1 = plugin(dict(sample_end=0.5, sample_tune_ct=37.0)); st = P1.raw_state
P2 = load_plugin(PATH); P2.raw_state = st
check('End and Tune survive a save and restore into a new instance', abs(P2.sample_end - 0.5) < 1e-4 and abs(P2.sample_tune_ct - 37.0) < 0.1, f'(end {P2.sample_end:.3f}, tune {P2.sample_tune_ct:.1f})')
Pold = plugin({}, drop=['sample_end', 'sample_tune_ct'])
check('A project saved before these controls existed loads with End = 1 and Tune = 0', abs(Pold.sample_end - 1.0) < 1e-6 and abs(Pold.sample_tune_ct) < 1e-6)
xa = render(plugin({})); xb = render(Pold)
check('and renders identically to the defaults', np.allclose(xa, xb, atol=1e-6))

# ---- sample end
x = render(plugin(dict(sample_end=0.5)))     # the file is 2 s long, so End 0.5 stops at 1 s
check('End 0.5: the sample plays until about 1 s, then stops', rms(x, 0.2, 0.9) > LOUD and rms(x, 1.15, 2.2) < SILENT, f'(before {rms(x,0.2,0.9):.3f}, after {rms(x,1.15,2.2):.5f})')
x = render(plugin(dict(sample_start=0.25, sample_end=0.5)))
check('Start 0.25 / End 0.5: plays 0.5 s of the file (until about 0.5 s) then stops', rms(x, 0.1, 0.4) > LOUD and rms(x, 0.65, 2.2) < SILENT)
x = render(plugin({}))
check('End at its default: the whole 2 s file plays', rms(x, 1.5, 1.9) > LOUD and rms(x, 2.15, 2.35) < SILENT)
x = render(plugin(dict(sample_end=0.5, sample_loop=1, loop_start=0.0, loop_end=1.0)))
check('a loop stays inside the End point (keeps sounding well past 1 s)', rms(x, 1.3, 2.2) > LOUD)
x = render(plugin(dict(sample_end=0.1, sample_loop=1, loop_start=0.5, loop_end=1.0)))
check('a loop that starts after End falls back to looping the played region (still sounds, no crash)', np.isfinite(x).all() and rms(x, 0.5, 2.2) > LOUD)
# reverse
x = render(plugin(dict(sample_reverse=1, sample_end=0.5)))
check('reverse with End 0.5 plays from the End back to the start (about 1 s)', rms(x, 0.1, 0.9) > LOUD and rms(x, 1.15, 2.2) < SILENT)
x = render(plugin(dict(sample_reverse=1, sample_start=0.25, sample_end=0.75)))
check('reverse with Start and End set stops at Start (about 1 s of the 2 s file)', rms(x, 0.1, 0.9) > LOUD and rms(x, 1.15, 2.2) < SILENT)
x = render(plugin(dict(sample_reverse=1, sample_start=0.3)))
check('reverse with Start but End at its default behaves as before (Start ignored, plays back to the beginning)', rms(x, 1.6, 1.9) > LOUD)

# ---- tuning
f0, fp, fm = freq(render(plugin({}))), freq(render(plugin(dict(sample_tune_ct=100)))), freq(render(plugin(dict(sample_tune_ct=-100))))
check('Tune 0 leaves the pitch alone (440 Hz)', abs(f0 - 440.0) < 0.6, f'({f0:.2f} Hz)')
check('Tune +100 cents raises it by a semitone (466.2 Hz)', abs(fp - 440 * 2 ** (100 / 1200)) < 0.7, f'({fp:.2f} Hz)')
check('Tune -100 cents lowers it by a semitone (415.3 Hz)', abs(fm - 440 * 2 ** (-100 / 1200)) < 0.7, f'({fm:.2f} Hz)')
f50 = freq(render(plugin(dict(sample_tune_ct=50)))); check('Tune +50 cents sits halfway (452.9 Hz)', abs(f50 - 440 * 2 ** (50 / 1200)) < 0.7, f'({f50:.2f} Hz)')


# =====================  slot B as an independent layer  =====================
def amp(x, f, a=0.3, b=1.3):
    s = x[int(a * SR):int(b * SR)]; w = np.hanning(len(s)); X = np.abs(np.fft.rfft(s * w, 1 << 17)); fr = np.fft.rfftfreq(1 << 17, 1 / SR)
    return float(2 * X[(fr > f - 6) & (fr < f + 6)].max() / w.sum())
def layers(extra=None, A=SINE, B=SINE2, drop=None):
    # B's root is set to 69 so the 660 Hz test tone plays at its own pitch; gains are 0.5 so the two layers together stay below the output limiter's knee
    return plugin({**dict(sampleb_indep=1, sample_morph=0.5, sampleb_normalize=0, sampleb_root=69, sample_gain=0.5, sampleb_gain=0.5), **(extra or {})}, A, {'samplePathB': B}, drop)
NEWB = ['sampleb_indep', 'sampleb_gain', 'sampleb_root', 'sampleb_octave', 'sampleb_tune_ct', 'sampleb_loop', 'sampleb_reverse', 'sampleb_normalize', 'sampleb_start', 'sampleb_end',
        'sampleb_loop_start', 'sampleb_loop_end', 'sampleb_xfade']
check('Slot B defaults: independent off, gain 1, root 60, full region, no loop, no reverse',
      not bool(P0.sample_b_independent) and abs(P0.sample_b_gain - 1.0) < 1e-6 and P0.sample_b_root == 60 and abs(P0.sample_b_end - 1.0) < 1e-6 and abs(P0.sample_b_start) < 1e-6 and not bool(P0.sample_b_loop) and not bool(P0.sample_b_reverse))
# saved state
PB = layers(dict(sampleb_gain=0.7, sampleb_root=55, sampleb_octave=1, sampleb_tune_ct=-23, sampleb_start=0.1, sampleb_end=0.8, sampleb_loop=1, sampleb_reverse=1, sampleb_loop_start=0.2, sampleb_loop_end=0.6, sampleb_xfade=3))
PB2 = load_plugin(PATH); PB2.raw_state = PB.raw_state
check('every slot B setting survives a save and restore into a new instance',
      bool(PB2.sample_b_independent) and abs(PB2.sample_b_gain - 0.7) < 1e-3 and PB2.sample_b_root == 55 and PB2.sample_b_octave == 1 and abs(PB2.sample_b_tune_ct + 23) < 0.1 and abs(PB2.sample_b_start - 0.1) < 1e-3
      and abs(PB2.sample_b_end - 0.8) < 1e-3 and bool(PB2.sample_b_loop) and bool(PB2.sample_b_reverse) and abs(PB2.sample_b_loop_start - 0.2) < 1e-3 and abs(PB2.sample_b_loop_end - 0.6) < 1e-3)
# classic morph unchanged when B is not independent
legacy = dict(sample_morph=0.5)
xa = render(plugin(legacy, SINE, {'samplePathB': SINE2}))
xb = render(plugin({**legacy, **dict(sampleb_gain=0.1, sampleb_root=30, sampleb_octave=2, sampleb_tune_ct=60, sampleb_start=0.5, sampleb_end=0.6, sampleb_loop=1, sampleb_reverse=1, sampleb_xfade=4)}, SINE, {'samplePathB': SINE2}))
check('with Independent off, slot B ignores its own settings (the classic A/B morph is unchanged)', np.allclose(xa, xb, atol=1e-6))
xc = render(plugin(legacy, SINE, {'samplePathB': SINE2}, drop=NEWB))
check('a project saved before slot B had its own controls loads as the classic morph and renders the same', np.allclose(xa, xc, atol=1e-6))
check('classic morph at 0.5 blends both samples', amp(xa, 440) > 0.05 and amp(xa, 660) > 0.05)

# independence
a440, b660 = amp(render(layers(dict(sample_morph=0.0))), 440), amp(render(layers(dict(sample_morph=1.0))), 660)
check('independent: morph 0 is layer A alone, morph 1 is layer B alone', amp(render(layers(dict(sample_morph=0.0))), 660) < 0.01 * a440 and amp(render(layers(dict(sample_morph=1.0))), 440) < 0.01 * b660, f'(A {a440:.3f}, B {b660:.3f})')
xm = render(layers())
check('independent: in the middle both layers play at full level', abs(amp(xm, 440) / a440 - 1) < 0.08 and abs(amp(xm, 660) / b660 - 1) < 0.08, f'(A x{amp(xm,440)/a440:.2f}, B x{amp(xm,660)/b660:.2f})')
xg = render(layers(dict(sampleb_gain=0.25)))
check('B gain changes only layer B (-6 dB) and leaves A alone', abs(amp(xg, 660) / b660 - 0.5) < 0.05 and abs(amp(xg, 440) / a440 - 1) < 0.08)
xg = render(layers(dict(sample_gain=0.25)))
check('A gain changes only layer A and leaves B alone', abs(amp(xg, 440) / a440 - 0.5) < 0.05 and abs(amp(xg, 660) / b660 - 1) < 0.08)
xo = render(layers(dict(sampleb_octave=1)))
check('B octave +1 plays B an octave up (1320 Hz) while A stays at 440 Hz', amp(xo, 1320) > 0.5 * b660 and amp(xo, 660) < 0.05 * b660 and abs(amp(xo, 440) / a440 - 1) < 0.08)
xr = render(layers(dict(sampleb_root=57)))
check('B root 57 (a lower root note) plays B an octave up while A is untouched', amp(xr, 1320) > 0.5 * b660 and abs(amp(xr, 440) / a440 - 1) < 0.08)
xt = render(layers(dict(sampleb_tune_ct=100))); f660 = 660 * 2 ** (100 / 1200)
check('B tune +100 cents shifts only B (699.3 Hz)', amp(xt, f660) > 0.5 * b660 and amp(xt, 660) < 0.1 * b660 and abs(amp(xt, 440) / a440 - 1) < 0.08)
xt = render(layers(dict(sample_tune_ct=100))); f440 = 440 * 2 ** (100 / 1200)
check('A tune +100 cents shifts only A (466.2 Hz) and leaves B at 660 Hz', amp(xt, f440) > 0.5 * a440 and amp(xt, 440) < 0.1 * a440 and abs(amp(xt, 660) / b660 - 1) < 0.08)
# regions
xe = render(layers(dict(sampleb_end=0.25)))
check('B End 0.25 stops B at about 0.5 s while A keeps playing', amp(xe, 660, 0.1, 0.4) > 0.5 * b660 and amp(xe, 660, 0.8, 1.6) < 0.02 * b660 and amp(xe, 440, 0.8, 1.6) > 0.5 * a440)
xe = render(layers(dict(sample_end=0.25)))
check('A End 0.25 stops A at about 0.5 s while B keeps playing', amp(xe, 440, 0.1, 0.4) > 0.5 * a440 and amp(xe, 440, 0.8, 1.6) < 0.02 * a440 and amp(xe, 660, 0.8, 1.6) > 0.5 * b660)
xs = render(layers(dict(sampleb_start=0.5, sampleb_end=0.75)))
check('B Start 0.5 / End 0.75 plays a 0.5 s region of B and then stops', amp(xs, 660, 0.05, 0.45) > 0.5 * b660 and amp(xs, 660, 0.7, 1.6) < 0.02 * b660)
# loops
xl = render(layers(dict(sampleb_loop=1, sampleb_loop_start=0.0, sampleb_loop_end=0.5)), hold=2.6, dur=3.0)
check('B loop keeps B sounding past the end of the file while A (no loop) ends', amp(xl, 660, 2.2, 2.55) > 0.5 * b660 and amp(xl, 440, 2.2, 2.55) < 0.02 * a440)
xl = render(layers(dict(sample_loop=1, loop_start=0.0, loop_end=0.5)), hold=2.6, dur=3.0)
check('A loop keeps A sounding past the end of the file while B (no loop) ends', amp(xl, 440, 2.2, 2.55) > 0.5 * a440 and amp(xl, 660, 2.2, 2.55) < 0.02 * b660)
# reverse (ramp file: rises 0 -> 0.8 over 1 s)
def slope(x, a=0.05, b=0.9):
    s = x[int(a * SR):int(b * SR)]; return float(np.polyfit(np.arange(len(s)) / SR, s, 1)[0])
xf_ = render(layers(dict(sample_morph=1.0), B=RAMP)); xr_ = render(layers(dict(sample_morph=1.0, sampleb_reverse=1), B=RAMP))
check('B plays forward by default (the ramp rises) and backwards with B Reverse (it falls)', slope(xf_) > 0.3 and slope(xr_) < -0.3, f'(slopes {slope(xf_):+.2f} and {slope(xr_):+.2f})')
xr_ = render(layers(dict(sample_morph=0.0, sample_reverse=1), A=RAMP, B=SINE2))
xf_ = render(layers(dict(sample_morph=0.0), A=RAMP, B=SINE2))
check('A Reverse affects only A', slope(xr_) < -0.3 and slope(xf_) > 0.3)
# B alone, chop, split
xo = render(layers(dict(sample_morph=0.5), A=None))
check('B plays on its own when no sample is loaded in slot A', amp(xo, 660) > 0.5 * b660 and np.isfinite(xo).all())
xc = render(layers(dict(chop_on=1, chop_mode=0, chop_count=4), A=SINE, B=SINE2), note=36, hold=1.0, dur=1.4)
check('chop pads play the A sample only (B stays out of chop mode)', amp(xc, 660, 0.1, 0.9) < 0.02 * b660 and amp(xc, 440, 0.1, 0.9) > 0.5 * a440)
# stress: many notes, loops and reverse at once, no blow-ups
Ps = layers(dict(sampleb_loop=1, sampleb_reverse=1, sampleb_xfade=3, sampleb_loop_start=0.1, sampleb_loop_end=0.3, sample_loop=1, loop_xfade=2, voices=16))
ev = []
for i, n in enumerate((48, 52, 55, 59, 62, 66, 69, 72, 76, 79)): ev += [(bytes([0x90, n, 120]), 0.05 * i), (bytes([0x80, n, 0]), 1.5)]
xs = Ps(ev, duration=2.0, sample_rate=SR, num_channels=2)
check('ten notes with both layers looping, one reversed: finite and never above full scale', np.isfinite(xs).all() and np.abs(xs).max() <= 1.0 + 1e-6, f'(peak {np.abs(xs).max():.3f})')

print(f"\nRESULT: {sum(rows)} of {len(rows)} sample-layer checks passed")
sys.exit(0 if all(rows) else 1)
