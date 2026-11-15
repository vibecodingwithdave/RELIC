// Standalone checks for EQ, tape, widener, wah, lo-fi and OTT (no JUCE). Built and run by tools/verify_fx2.py
#include "../Source/Dsp/Effects.h"
#include <cstdio>
#include <random>
using namespace rf;
static int pass = 0, total = 0; static const double SR = 44100.0;
static void check (const char* name, bool ok, double a = 0, double b = 0) { ++total; pass += ok; std::printf ("  [%s] %s (%.3f, %.3f)\n", ok ? "PASS" : "FAIL", name, a, b); }
static double rmsOf (const std::vector<float>& x, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b; ++i) s += (double) x[i] * x[i]; return std::sqrt (s / (double) (b - a)); }
template <class F> static std::vector<float> run (const std::vector<float>& in, F f) { std::vector<float> o; o.reserve (in.size()); for (float x : in) { float l = x, r = x; f (l, r); o.push_back (l); } return o; }
static std::vector<float> sine (double hz, double amp, int n) { std::vector<float> v ((size_t) n); for (int i = 0; i < n; ++i) v[(size_t) i] = (float) (amp * std::sin (2 * kPi * hz * i / SR)); return v; }
template <class F> static double gainDb (double hz, F f, double amp = 0.1, int n = 44100) { auto in = sine (hz, amp, n); auto o = run (in, f); return 20 * std::log10 (rmsOf (o, n / 2, n) / rmsOf (in, n / 2, n)); }
static double binMag (const std::vector<float>& x, double hz, size_t a, size_t b) { double c = 0, s = 0; for (size_t i = a; i < b; ++i) { c += x[i] * std::cos (2 * kPi * hz * i / SR); s += x[i] * std::sin (2 * kPi * hz * i / SR); } return 2 * std::sqrt (c * c + s * s) / (double) (b - a); }
// peak pitch deviation (cents) from rising zero crossings
static double pitchDev (const std::vector<float>& x, double f0, size_t a)
{
    std::vector<double> t; for (size_t i = a + 1; i < x.size(); ++i) if (x[i - 1] < 0 && x[i] >= 0) t.push_back ((double) (i - 1) + (-x[i - 1]) / (double) (x[i] - x[i - 1]));
    double mx = 0; for (size_t i = 1; i < t.size(); ++i) mx = std::max (mx, std::abs (1200 * std::log2 ((SR / (t[i] - t[i - 1])) / f0))); return mx;
}

int main()
{
    std::puts ("== EQ");
    { Equalizer e; e.prepare (SR); auto f = [&] (float& l, float& r) { e.process (l, r, 0, 1000, 0, 0, SR); };
      bool flat = true; for (double hz : { 40.0, 200.0, 1000.0, 4000.0, 12000.0 }) flat &= std::abs (gainDb (hz, f)) < 0.05; check ("all bands at 0 dB: flat", flat); }
    { Equalizer e; e.prepare (SR); auto f = [&] (float& l, float& r) { e.process (l, r, 9, 1000, 0, 0, SR); }; check ("low shelf +9 dB: +9 at 40 Hz", std::abs (gainDb (40, f) - 9.0) < 0.7, gainDb (40, f), 9); check ("low shelf leaves 4 kHz alone", std::abs (gainDb (4000, f)) < 0.3, gainDb (4000, f)); }
    { Equalizer e; e.prepare (SR); auto f = [&] (float& l, float& r) { e.process (l, r, -9, 1000, 0, 0, SR); }; check ("low shelf -9 dB: -9 at 40 Hz", std::abs (gainDb (40, f) + 9.0) < 0.7, gainDb (40, f), -9); }
    { Equalizer e; e.prepare (SR); auto f = [&] (float& l, float& r) { e.process (l, r, 0, 2500, 8, 0, SR); }; check ("mid +8 dB at 2.5 kHz centre", std::abs (gainDb (2500, f) - 8.0) < 0.3, gainDb (2500, f), 8); check ("mid band is narrow: 100 Hz within 1 dB", std::abs (gainDb (100, f)) < 1.0, gainDb (100, f)); }
    { Equalizer e; e.prepare (SR); auto f = [&] (float& l, float& r) { e.process (l, r, 0, 1000, 0, 6, SR); }; check ("high shelf +6 dB: +6 at 15 kHz", std::abs (gainDb (15000, f) - 6.0) < 0.8, gainDb (15000, f), 6); check ("high shelf leaves 200 Hz alone", std::abs (gainDb (200, f)) < 0.3, gainDb (200, f)); }

    std::puts ("== TAPE");
    { Tape t; t.prepare (SR); auto f = [&] (float& l, float& r) { t.process (l, r, 0.0f, 1.0f, 0.0f, SR); }; check ("drive 0, tone full: near unity at 1 kHz", std::abs (gainDb (1000, f)) < 0.5, gainDb (1000, f)); }
    { Tape t; t.prepare (SR); auto in = sine (1000, 0.3, 44100); auto o = run (in, [&] (float& l, float& r) { t.process (l, r, 1.0f, 1.0f, 0.0f, SR); });
      const double h1 = binMag (o, 1000, 22050, 44100), h2 = binMag (o, 2000, 22050, 44100), h3 = binMag (o, 3000, 22050, 44100);
      check ("drive 1: third harmonic above 5% of the fundamental", h3 / h1 > 0.05, h3 / h1); check ("tanh is odd: second harmonic far below the third", h2 < 0.1 * h3, h2 / h1, h3 / h1); }
    { Tape a, b; a.prepare (SR); b.prepare (SR); auto fa = [&] (float& l, float& r) { a.process (l, r, 0.0f, 0.0f, 0.0f, SR); }; auto fb = [&] (float& l, float& r) { b.process (l, r, 0.0f, 1.0f, 0.0f, SR); };
      check ("tone 0 rolls off 10 kHz (below -10 dB)", gainDb (10000, fa) < -10.0, gainDb (10000, fa)); check ("tone 1 keeps 10 kHz (above -3 dB)", gainDb (10000, fb) > -3.0, gainDb (10000, fb)); }
    { Tape t; t.prepare (SR); auto o = run (sine (1000, 0.1, 88200), [&] (float& l, float& r) { t.process (l, r, 0.0f, 1.0f, 1.0f, SR); }); const double d = pitchDev (o, 1000, 4410);
      Tape u; u.prepare (SR); auto o2 = run (sine (1000, 0.1, 88200), [&] (float& l, float& r) { u.process (l, r, 0.0f, 1.0f, 0.0f, SR); }); const double d0 = pitchDev (o2, 1000, 4410);
      check ("wobble 1: pitch wanders by more than 8 cents", d > 8.0, d); check ("wobble 0: pitch steady (under 1 cent)", d0 < 1.0, d0); }

    std::puts ("== WIDENER");
    { Widener w; float l = 0.8f, r = 0.2f; w.process (l, r, 0.0f); check ("width 0 is mono", std::abs (l - r) < 1e-6 && std::abs (l - 0.5f) < 1e-6);
      l = 0.8f; r = 0.2f; w.process (l, r, 1.0f); check ("width 1 leaves the signal alone", std::abs (l - 0.8f) < 1e-6 && std::abs (r - 0.2f) < 1e-6);
      l = 0.8f; r = 0.2f; w.process (l, r, 2.0f); check ("width 2 doubles the side, keeps the mid", std::abs ((l + r) * 0.5f - 0.5f) < 1e-6 && std::abs ((l - r) * 0.5f - 0.6f) < 1e-6); }

    std::puts ("== WAH");
    { std::vector<double> maxmin;
      for (float depth : { 0.0f, 1.0f })
      { Wah w; w.prepare (SR); auto in = sine (800, 0.2, 132300); auto o = run (in, [&] (float& l, float& r) { w.process (l, r, 0, 1.0f, depth, 0.6f, 1.0f, SR); });
        double lo = 1e9, hi = 0; for (size_t a = 22050; a + 2205 < o.size(); a += 2205) { const double v = rmsOf (o, a, a + 2205); lo = std::min (lo, v); hi = std::max (hi, v); } maxmin.push_back (hi / lo); }
      check ("depth 0: the filter stays put", maxmin[0] < 1.05, maxmin[0]); check ("depth 1: an 800 Hz tone swells and fades as the wah sweeps (>5x)", maxmin[1] > 5.0, maxmin[1]); }
    { auto level = [&] (double amp) { Wah w; w.prepare (SR); auto in = sine (1500, amp, 44100); auto o = run (in, [&] (float& l, float& r) { w.process (l, r, 1, 1.0f, 1.0f, 0.6f, 1.0f, SR); }); return 20 * std::log10 (rmsOf (o, 22050, 44100) / rmsOf (in, 22050, 44100)); };
      check ("envelope mode: louder playing opens the wah (>6 dB more at 1.5 kHz)", level (0.5) - level (0.02) > 6.0, level (0.5), level (0.02)); }
    { Wah w; w.prepare (SR); bool ok = true; std::mt19937 rng (4); std::normal_distribution<float> nd; for (int i = 0; i < 200000; ++i) { float l = nd (rng) * 0.3f, r = l; w.process (l, r, 0, 3.0f, 1.0f, 1.0f, 1.0f, SR); ok &= std::isfinite (l) && std::abs (l) < 20.0f; } check ("noise at max resonance stays finite", ok); }

    std::puts ("== LO-FI");
    { LoFi f; f.prepare (SR); auto fn = [&] (float& l, float& r) { f.process (l, r, 3000.0f, 0.0f, 0.0f, 1.0f, SR); };
      check ("tone 3 kHz: about -6 dB at 3 kHz (two poles)", std::abs (gainDb (3000, fn) + 6.0) < 1.5, gainDb (3000, fn), -6); check ("passband flat near 300 Hz", std::abs (gainDb (300, fn)) < 0.5, gainDb (300, fn)); check ("below 60 Hz is cut (20 Hz under -12 dB)", gainDb (20, fn) < -12.0, gainDb (20, fn)); check ("far above the tone is gone (12 kHz under -20 dB)", gainDb (12000, fn) < -20.0, gainDb (12000, fn)); }
    { LoFi f; f.prepare (SR); std::vector<float> z (88200, 0.0f); auto o = run (z, [&] (float& l, float& r) { f.process (l, r, 6000.0f, 1.0f, 0.0f, 1.0f, SR); }); LoFi g; g.prepare (SR); auto o0 = run (z, [&] (float& l, float& r) { g.process (l, r, 6000.0f, 0.0f, 0.0f, 1.0f, SR); });
      check ("noise 1 on silence: hiss and crackle are audible (-60 to -20 dBFS)", rmsOf (o, 0, o.size()) > 0.001 && rmsOf (o, 0, o.size()) < 0.1, rmsOf (o, 0, o.size())); check ("noise 0 on silence: exactly silent", rmsOf (o0, 0, o0.size()) == 0.0); }
    { LoFi f; f.prepare (SR); std::vector<float> in (4000, 0.0f); in[100] = 1.0f; auto o = run (in, [&] (float& l, float& r) { f.process (l, r, 6000.0f, 0.0f, 0.0f, 0.0f, SR); }); size_t pk = 0; for (size_t i = 0; i < o.size(); ++i) if (std::abs (o[i]) > std::abs (o[pk])) pk = i;
      check ("mix 0: dry comes back delayed 6 ms (264.6 samples) so the paths line up", std::abs ((double) pk - 100.0 - 264.6) < 1.0, (double) pk - 100.0, 264.6); }
    { LoFi f; f.prepare (SR); auto o = run (sine (1000, 0.1, 88200), [&] (float& l, float& r) { f.process (l, r, 12000.0f, 0.0f, 1.0f, 1.0f, SR); }); LoFi g; g.prepare (SR); auto o0 = run (sine (1000, 0.1, 88200), [&] (float& l, float& r) { g.process (l, r, 12000.0f, 0.0f, 0.0f, 1.0f, SR); });
      check ("wobble 1: pitch wanders by more than 10 cents", pitchDev (o, 1000, 6000) > 10.0, pitchDev (o, 1000, 6000)); check ("wobble 0: pitch steady", pitchDev (o0, 1000, 6000) < 1.0, pitchDev (o0, 1000, 6000)); }

    std::puts ("== OTT");
    { Ott o; o.prepare (SR); auto fn = [&] (float& l, float& r) { o.process (l, r, 0.0f, 0.5f, 1.0f, SR); }; bool flat = true; double worst = 0; for (double hz : { 50.0, 120.0, 500.0, 2000.0, 5000.0, 12000.0 }) { const double g = gainDb (hz, fn, 0.1, 88200); worst = std::max (worst, std::abs (g)); flat &= std::abs (g) < 1.5; }
      check ("depth 0: the three bands sum back to flat (within 1.5 dB)", flat, worst); }
    { auto lvl = [&] (double dbIn, float depth) { Ott o; o.prepare (SR); const double amp = std::pow (10.0, dbIn / 20.0); auto in = sine (1000, amp, 88200); auto out = run (in, [&] (float& l, float& r) { o.process (l, r, depth, 0.2f, 1.0f, SR); }); return 20 * std::log10 (rmsOf (out, 44100, 88200)); };
      const double r1 = lvl (-10, 1.0f) - lvl (-50, 1.0f), r0 = lvl (-10, 0.0f) - lvl (-50, 0.0f);
      check ("depth 0: a 40 dB range stays 40 dB", std::abs (r0 - 40.0) < 1.0, r0); check ("depth 1: the 40 dB range squeezes under 22 dB", r1 < 22.0, r1);
      check ("quiet material is lifted (-50 dBFS gains over 8 dB)", lvl (-50, 1.0f) - lvl (-50, 0.0f) > 8.0, lvl (-50, 1.0f) - lvl (-50, 0.0f)); check ("loud material is pushed down (-10 dBFS loses over 6 dB)", lvl (-10, 0.0f) - lvl (-10, 1.0f) > 6.0, lvl (-10, 0.0f) - lvl (-10, 1.0f));
      check ("the upward lift is capped near +12.5 dB", lvl (-70, 1.0f) - lvl (-70, 0.0f) < 14.0, lvl (-70, 1.0f) - lvl (-70, 0.0f)); }
    { Ott o; o.prepare (SR); bool ok = true; std::mt19937 rng (5); std::normal_distribution<float> nd; for (int i = 0; i < 200000; ++i) { float l = nd (rng) * 0.5f, r = l; o.process (l, r, 1.0f, 0.5f, 1.0f, SR); ok &= std::isfinite (l) && std::abs (l) < 30.0f; } check ("loud noise stays finite", ok); }
    std::printf ("\nRESULT: %d of %d effect checks passed\n", pass, total); return pass == total ? 0 : 1;
}
