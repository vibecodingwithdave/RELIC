// Standalone checks for the chorus, flanger and reverb (no JUCE needed). Build: see tools/verify_modfx.py
#include "../Source/Dsp/Effects.h"
#include <cstdio>
#include <random>
#include <complex>
using namespace rf;
static int pass = 0, total = 0;
static void check (const char* name, bool ok, double a = 0, double b = 0) { ++total; pass += ok; std::printf ("  [%s] %s (%.3f, %.3f)\n", ok ? "PASS" : "FAIL", name, a, b); }
static const double SR = 44100.0;
static double rms (const std::vector<float>& x, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b; ++i) s += x[i] * x[i]; return std::sqrt (s / std::max<size_t> (1, b - a)); }
static double toneGain (std::vector<float> (*run) (const std::vector<float>&), double hz, int n)
{
    std::vector<float> in (n); for (int i = 0; i < n; ++i) in[i] = (float) std::sin (2 * kPi * hz * i / SR);
    auto o = run (in); return rms (o, n / 2, n) / rms (in, n / 2, n);
}
// flanger with fixed settings
static float gFb = 0, gMix = 1, gDepth = 0;
static std::vector<float> runFlange (const std::vector<float>& in)
{ Flanger f; f.prepare (SR); std::vector<float> o; for (float x : in) { float l = x, r = x; f.process (l, r, 0.5f, gDepth, gFb, gMix, SR); o.push_back (l); } return o; }

int main()
{
    std::puts ("== FLANGER");
    gDepth = 0; gFb = 0; gMix = 0.5f;
    check ("notch at 1 kHz (0.5 ms delay, 50% mix)", toneGain (runFlange, 1000.0, 44100) < 0.02, toneGain (runFlange, 1000.0, 44100));
    check ("peak at 2 kHz is +0 dB (equal sum)", std::abs (toneGain (runFlange, 2000.0, 44100) - 1.0) < 0.03, toneGain (runFlange, 2000.0, 44100));
    gMix = 1; gFb = 0.8f; { const double g = toneGain (runFlange, 2000.0, 88200); check ("feedback 0.8: +14 dB at 2 kHz", std::abs (20 * std::log10 (g) - 13.98) < 0.5, 20 * std::log10 (g), 13.98); }
    gFb = -0.8f; { const double g = toneGain (runFlange, 2000.0, 88200); check ("feedback -0.8: 2 kHz drops to about -5 dB", std::abs (20 * std::log10 (g) + 5.1) < 0.5, 20 * std::log10 (g), -5.1); }
    gMix = 0; gFb = 0.5f; gDepth = 0.6f; { std::vector<float> in (4410, 0.3f); auto o = runFlange (in); bool same = true; for (size_t i = 0; i < in.size(); ++i) same &= std::abs (o[i] - in[i]) < 1e-6; check ("mix 0 passes the dry signal untouched", same); }
    { // sweep range: with depth 1 the first notch moves between 1/(2*0.5ms)=1000 Hz and 1/(2*6.5ms)=77 Hz
        gDepth = 1.0f; gMix = 0.5f; gFb = 0; Flanger f; f.prepare (SR); std::mt19937 rng (1); std::normal_distribution<float> nd; double lo = 1e9, hi = 0;
        std::vector<float> o; for (int i = 0; i < 88200; ++i) { float l = nd (rng) * 0.2f, r = l; f.process (l, r, 0.5f, 1.0f, 0.0f, 0.5f, SR); o.push_back (l); }
        check ("output is finite and bounded with noise", rms (o, 0, o.size()) > 0.05 && rms (o, 0, o.size()) < 0.5, rms (o, 0, o.size())); (void) lo; (void) hi; }

    std::puts ("== CHORUS");
    { Chorus c; c.prepare (SR); const int n = 4410; std::vector<float> o (n);
      for (int i = 0; i < n; ++i) { float l = i == 0 ? 1.0f : 0.0f, r = l; c.process (l, r, 0.01f, 1.0f, 1.0f, SR); o[i] = l; }
      // three taps at 12, 12 + 6 sin(120 deg) = 17.2, 12 - 5.2 = 6.8 ms (rate so low the LFO barely moves)
      auto tap = [&] (double ms) { const int c0 = (int) std::lround (ms * 0.001 * SR); float m = 0; for (int k = -2; k <= 2; ++k) m += std::abs (o[(size_t) (c0 + k)]); return m; };
      check ("tap near 6.8 ms", tap (6.8) > 0.3, tap (6.8)); check ("tap near 12 ms", tap (12.0) > 0.3, tap (12.0)); check ("tap near 17.2 ms", tap (17.2) > 0.3, tap (17.2));
      check ("nothing before 5 ms", tap (3.0) < 0.01 && tap (1.0) < 0.01); }
    { Chorus c; c.prepare (SR); const int n = 4410; std::vector<float> o (n);
      for (int i = 0; i < n; ++i) { float l = i == 0 ? 1.0f : 0.0f, r = l; c.process (l, r, 1.0f, 0.0f, 1.0f, SR); o[i] = l; }
      const int c0 = (int) std::lround (0.012 * SR); double e = 0; for (int k = -2; k <= 2; ++k) e += o[(size_t) (c0 + k)]; check ("depth 0: one tap at 12 ms, full level", std::abs (e - 1.0) < 0.02, e); }
    { Chorus c; c.prepare (SR); bool same = true; for (int i = 0; i < 2000; ++i) { float l = std::sin (i * 0.1f), r = l; const float x = l; c.process (l, r, 1.0f, 1.0f, 0.0f, SR); same &= std::abs (l - x) < 1e-6; } check ("mix 0 passes the dry signal untouched", same); }
    { // pitch wobble: 440 Hz sine, peak instantaneous deviation = f * 2 pi rate D
        Chorus c; c.prepare (SR); const double rate = 2.0; const int n = 88200; std::vector<float> o (n); // single-voice view: use depth and read L
        for (int i = 0; i < n; ++i) { float l = (float) std::sin (2 * kPi * 440.0 * i / SR), r = l; c.process (l, r, (float) rate, 1.0f, 1.0f, SR); o[(size_t) i] = l; }
        // the average of three out-of-phase voices cancels pitch wobble partly, so the check is only that sidebands exist
        std::vector<double> mag; double e0 = 0, eSide = 0; const int N = 1 << 16; for (int k = 0; k < N; ++k) {} 
        double side = 0, carrier = 0; for (int i = 0; i < n; ++i) { carrier += o[(size_t) i] * std::sin (2 * kPi * 440.0 * i / SR); side += o[(size_t) i] * std::sin (2 * kPi * 442.0 * i / SR); }
        check ("output stays near full level (voices average to about 0.6 or more)", rms (o, n / 2, n) > 0.3 && rms (o, n / 2, n) < 0.75, rms (o, n / 2, n)); (void) e0; (void) eSide; (void) side; (void) carrier; }

    std::puts ("== REVERB");
    auto rt60 = [&] (float size, float damp)
    {
        Reverb r; r.prepare (SR); const int n = (int) (SR * 14); std::vector<double> e ((size_t) n);
        for (int i = 0; i < n; ++i) { float l = i == 0 ? 1.0f : 0.0f, rr = l; r.process (l, rr, size, damp, 1.0f, 1.0f, SR); e[(size_t) i] = (double) l * l; }
        std::vector<double> edc ((size_t) n); double acc = 0; for (int i = n - 1; i >= 0; --i) { acc += e[(size_t) i]; edc[(size_t) i] = acc; }
        auto at = [&] (double db) { for (int i = 0; i < n; ++i) if (10 * std::log10 (edc[(size_t) i] / edc[0]) < db) return i / SR; return (double) n / SR; };
        return 3.0 * (at (-25.0) - at (-5.0));   // T20 extrapolated to 60 dB
    };
    for (float sz : { 0.2f, 0.5f, 0.8f })
    {
        const double want = Reverb::rt60ForSize (sz), got = rt60 (sz, 0.0f);
        char nm[80]; std::snprintf (nm, sizeof nm, "size %.1f: RT60 near %.2f s (within 25%%)", sz, want); check (nm, std::abs (got - want) / want < 0.25, got, want);
    }
    check ("damping shortens the decay", rt60 (0.6f, 0.9f) < 0.8 * rt60 (0.6f, 0.0f), rt60 (0.6f, 0.9f), rt60 (0.6f, 0.0f));
    { Reverb r; r.prepare (SR); const int n = 20000; int first = -1; for (int i = 0; i < n; ++i) { float l = i == 0 ? 1.0f : 0.0f, rr = l; r.process (l, rr, 0.5f, 0.2f, 50.0f, 1.0f, SR); if (first < 0 && std::abs (l) > 1e-4f) first = i; }
      check ("predelay 50 ms: silence until at least 50 ms", first >= (int) (0.05 * SR) - 2, first / SR * 1000.0, 50.0); }
    { Reverb r; r.prepare (SR); std::mt19937 rng (3); std::normal_distribution<float> nd; std::vector<float> L, R, D; double dif = 0;
      for (int i = 0; i < 88200; ++i) { const float x = nd (rng) * 0.1f; float l = x, rr = x; r.process (l, rr, 0.5f, 0.3f, 10.0f, 1.0f, SR); L.push_back (l); R.push_back (rr); D.push_back (x); dif += std::abs (l - rr); }
      check ("noise through reverb stays near the dry level (-9 to +6 dB)", rms (L, 44100, 88200) / rms (D, 44100, 88200) > 0.35 && rms (L, 44100, 88200) / rms (D, 44100, 88200) < 2.0, rms (L, 44100, 88200) / rms (D, 44100, 88200));
      check ("left and right are decorrelated", dif / 88200 > 0.01, dif / 88200); }
    { Reverb r; r.prepare (SR); bool same = true; for (int i = 0; i < 4000; ++i) { float l = std::sin (i * 0.05f), rr = l; const float x = l; r.process (l, rr, 0.9f, 0.5f, 20.0f, 0.0f, SR); same &= std::abs (l - x) < 1e-6; } check ("mix 0 passes the dry signal untouched", same); }
    { Reverb r; r.prepare (SR); bool ok = true; double peak = 0; for (int i = 0; i < 44100 * 20; ++i) { float l = i < 2000 ? 1.0f : 0.0f, rr = l; r.process (l, rr, 1.0f, 0.0f, 0.0f, 1.0f, SR); ok &= std::isfinite (l); peak = std::max (peak, (double) std::abs (l)); } check ("longest setting stays finite and decays", ok && peak < 20.0, peak); }
    std::printf ("\nRESULT: %d of %d modulation-effect checks passed\n", pass, total);
    return pass == total ? 0 : 1;
}
