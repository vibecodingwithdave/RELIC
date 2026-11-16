"""Manual chop slicing (headless, Pedalboard): slices cut at hand-placed points. Synthetic 4 s file of four 1 s tone bursts (220/330/440/660 Hz)."""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
exec(open(os.path.join(os.path.dirname(__file__), 'verify_chop_kit.py')).read().split("print('== DEFAULTS")[0])
PT = lambda *v: {f'chop_pt_{i + 1}': x for i, x in enumerate(v)}
MAN = dict(chop_mode=3, chop_count=4)
print('== MANUAL SLICES ==')
x = render([(36, 0.0, 1.5)], **MAN, **PT(0.125, 0.5, 0.875)); check('pad 1 is the file start up to the first point (0.5 s of 220 Hz, then it ends)', near(freq(x, 0.05, 0.4), 220) and rms(x, 0.6, 1.4) < 1e-3 * rms(x, 0.05, 0.4))
x = render([(37, 0.0, 1.8)], **MAN, **PT(0.125, 0.5, 0.875), dur=2.2); check('pad 2 starts at the first point and runs to the second (220 Hz then 330 Hz)', near(freq(x, 0.05, 0.3), 220) and near(freq(x, 1.1, 1.4), 330) and rms(x, 1.7, 2.1) < 1e-3 * rms(x, 0.05, 0.3), f'({freq(x, 0.05, 0.3):.0f}, {freq(x, 1.1, 1.4):.0f} Hz)')
x = render([(39, 0.0, 1.0)], **MAN, **PT(0.125, 0.5, 0.875)); check('the last pad starts at the last point (3.5 s, which is inside the 660 Hz burst)', near(freq(x, 0.05, 0.3), 660), f'({freq(x, 0.05, 0.3):.0f} Hz)')
y = render([(37, 0.0, 1.8)], **MAN, **PT(0.5, 0.125, 0.875), dur=2.2)
check('points given out of order cut the same way (they are used in order of position)', np.allclose(render([(37, 0.0, 1.8)], **MAN, **PT(0.125, 0.5, 0.875), dur=2.2), y, atol=1e-6))
x = render([(37, 0.0, 1.0)], chop_mode=3, chop_count=2, **PT(0.5, 0.75, 0.9)); check('with 2 slices only the first point is used (pad 2 is 2.0 s onward: 440 Hz)', near(freq(x, 0.05, 0.4), 440), f'({freq(x, 0.05, 0.4):.0f} Hz)')
x = render([(38, 0.0, 1.0)], **MAN, **PT(0.125, 0.5, 0.875)); check('pad 3 plays the slice from the second point (2.0 s: 440 Hz)', near(freq(x, 0.05, 0.4), 440))
a = render([(36, 0.0, 1.0)], chop_mode=0, chop_count=4); b = render([(36, 0.0, 1.0)], chop_mode=3, chop_count=4, **PT(0.25, 0.5, 0.75))
check('manual points on an even grid play the same as Even', np.allclose(a, b, atol=1e-6))
x = render([(39, 0.0, 1.0)], **MAN, **PT(0.5, 0.5, 0.5)); check('points on top of each other are held a hair apart: the last pad still plays from there (440 Hz)', np.isfinite(x).all() and near(freq(x, 0.05, 0.3), 440), f'({freq(x, 0.05, 0.3):.0f} Hz)')
print('== KIT, STATE ==')
x = render([(36, 0.0, 1.0)], **MAN, **PT(0.125, 0.5, 0.875), **K(1, slice=2), dur=2.0); check('a kit slice assignment uses the manual slice', near(freq(x, 0.05, 0.3), 220) and near(freq(x, 0.7, 1.0), 330))
ref = render([(37, 0.0, 1.8)], **MAN, **PT(0.2, 0.4, 0.9), dur=2.2); raw = P.raw_state
check('manual points are kept in the saved state', np.array_equal(render([(37, 0.0, 1.8)], raw=raw, dur=2.2), ref) and rms(ref, 0.05, 0.4) > 0.005)
d = render([(36, 0.0, 1.0)], drop=['chop_pt_%d' % i for i in range(1, 16)], chop_mode=0); e = render([(36, 0.0, 1.0)], chop_mode=0)
check('a project without the new points renders the same in the other modes', np.array_equal(d, e))
print(f'{sum(rows)}/{len(rows)} passed'); sys.exit(0 if all(rows) else 1)
