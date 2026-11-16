// External wavetable banks: exact checks on the analysis and lookup (no JUCE).
#include "../Source/Dsp/WaveTables.h"
#include <cstdio>
using namespace rf;
static int passed = 0, total = 0;
static void check (const char* name, bool ok) { ++total; passed += ok; std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", name); }
static const double PI = 3.14159265358979323846;
static std::vector<float> cycle (int len, int harmonic, double amp = 0.5, double phase = 0.0) { std::vector<float> v ((size_t) len); for (int n = 0; n < len; ++n) v[(size_t) n] = (float) (amp * std::sin (2 * PI * harmonic * n / len + phase)); return v; }
static void append (std::vector<float>& a, const std::vector<float>& b) { a.insert (a.end(), b.begin(), b.end()); }
// amplitude of harmonic k in table frame f, band b
static double harm (const WaveTables::UserBank& u, int f, int b, int k)
{
    const float* t = u.at (f, b) + 1; double re = 0, im = 0;
    for (int n = 0; n < WaveTables::N; ++n) { re += t[n] * std::cos (2 * PI * k * n / WaveTables::N); im += t[n] * std::sin (2 * PI * k * n / WaveTables::N); }
    return 2.0 * std::sqrt (re * re + im * im) / WaveTables::N;
}
int main()
{
    const double sr = 44100;
    WaveTables wt;
    {   // frame length detection
        check ("detect: 2048 samples is one 2048 frame", WaveTables::detectFrameLen (2048) == 2048);
        check ("detect: 8 x 2048 samples is 2048", WaveTables::detectFrameLen (8 * 2048) == 2048);
        check ("detect: 600 samples is a single 600-sample cycle", WaveTables::detectFrameLen (600) == 600);
        check ("detect: 256 x 1024 samples picks 2048 (it divides evenly)", WaveTables::detectFrameLen (256 * 1024) == 2048);
        check ("detect: 3 x 1024 samples picks 1024", WaveTables::detectFrameLen (3072) == 1024);
        check ("detect: a hint of 1024 wins over the length", WaveTables::detectFrameLen (4096, 1024) == 1024);
        check ("detect: a silly hint is ignored", WaveTables::detectFrameLen (4096, 7) == 2048);
        check ("detect: 5000 samples (no whole frames) is one cycle", WaveTables::detectFrameLen (5000) == 5000);
        check ("detect: fewer than 2 samples is nothing", WaveTables::detectFrameLen (1) == 0);
    }
    {   // the "clm " chunk
        std::vector<unsigned char> f { 'R','I','F','F', 0,0,0,0, 'W','A','V','E', 'f','m','t',' ', 2,0,0,0, 1,2, 'c','l','m',' ', 16,0,0,0 };
        const char* txt = "<!>1024 10000000";
        for (const char* p = txt; *p; ++p) f.push_back ((unsigned char) *p);
        check ("clm chunk: reads <!>1024", WaveTables::parseFrameLenHint (f.data(), f.size()) == 1024);
        std::vector<unsigned char> g { 'R','I','F','F', 0,0,0,0, 'W','A','V','E', 'd','a','t','a', 4,0,0,0, 1,2,3,4 };
        check ("clm chunk: a file without one gives 0", WaveTables::parseFrameLenHint (g.data(), g.size()) == 0);
        std::vector<unsigned char> h { 'X','Y' };
        check ("clm chunk: a tiny non-wave buffer gives 0 without reading past the end", WaveTables::parseFrameLenHint (h.data(), h.size()) == 0);
    }
    {   // one sine frame reproduces a sine, at every band
        auto b = WaveTables::buildUser (cycle (2048, 1), 0, sr, "sine");
        check ("one frame: bank has 1 frame", b && b->frames == 1 && b->frameLen == 2048);
        double h1 = harm (*b, 0, 0, 1), h2 = harm (*b, 0, 0, 2), h3 = harm (*b, 0, 10, 1);
        check ("one frame: a sine stays a pure sine (harmonic 2 is silent)", h1 > 0.3 && h2 < 1e-4 * h1);
        check ("one frame: the sine survives in the highest band too", std::abs (h3 - h1) < 1e-4);
        check ("one frame: gain keeps it about as loud as the saw (rms near 0.5, peak below 1)", [&] { const float* t = b->at (0, 4); double ss = 0, pk = 0; for (int n = 1; n <= 2048; ++n) { ss += t[n] * t[n]; pk = std::max (pk, (double) std::abs (t[n])); } return std::sqrt (ss / 2048) > 0.35 && pk <= 1.0; }());
    }
    {   // band limiting
        std::vector<float> saw (2048); for (int n = 0; n < 2048; ++n) saw[(size_t) n] = (float) (1.0 - 2.0 * n / 2048);
        auto b = WaveTables::buildUser (saw, 0, sr);
        const int band = 5; const double fmax = WaveTables::baseHz * std::pow (2.0, band + 1); const int maxH = (int) std::floor (sr * 0.5 * 0.97 / fmax);
        check ("band limit: the top band keeps only the harmonics below Nyquist", harm (*b, 0, band, maxH) > 0.01 && harm (*b, 0, band, maxH + 3) < 1e-4);
        check ("band limit: the lowest band keeps many harmonics", harm (*b, 0, 0, 200) > 0.001);
        check ("band limit: a table is cyclic (guard points)", [&] { const float* t = b->at (0, 3); return t[0] == t[2048] && t[2049] == t[1] && t[2050] == t[2]; }());
    }
    {   // frames, morphing and one shared gain
        std::vector<float> d; append (d, cycle (2048, 1, 0.5)); append (d, cycle (2048, 2, 0.5)); append (d, cycle (2048, 3, 0.25));
        auto b = WaveTables::buildUser (d, 0, sr);
        check ("frames: 3 frames found", b->frames == 3);
        const double a1 = harm (*b, 0, 4, 1), a2 = harm (*b, 1, 4, 2), a3 = harm (*b, 2, 4, 3);
        check ("frames: each frame keeps its own harmonic", a1 > 0.1 && a2 > 0.1 && a3 > 0.05 && harm (*b, 0, 4, 2) < 1e-4 && harm (*b, 1, 4, 1) < 1e-4);
        check ("frames: relative levels survive (the quieter frame stays half as loud)", std::abs (a3 / a2 - 0.5) < 0.01 && std::abs (a1 / a2 - 1.0) < 0.01);
        const double ph = 0.1234;
        check ("morph: position 0 is frame 0, position 1 is the last frame", std::abs (wt.lookupUserMorph (*b, 0.0, 4, ph) - wt.lookupUser (*b, 0, 4, ph)) < 1e-7 && std::abs (wt.lookupUserMorph (*b, 1.0, 4, ph) - wt.lookupUser (*b, 2, 4, ph)) < 1e-7);
        check ("morph: position 0.25 is halfway between frames 0 and 1", std::abs (wt.lookupUserMorph (*b, 0.25, 4, ph) - 0.5f * (wt.lookupUser (*b, 0, 4, ph) + wt.lookupUser (*b, 1, 4, ph))) < 1e-6);
        check ("morph: out-of-range positions are clamped", std::abs (wt.lookupUserMorph (*b, -3.0, 4, ph) - wt.lookupUser (*b, 0, 4, ph)) < 1e-7 && std::abs (wt.lookupUserMorph (*b, 9.0, 4, ph) - wt.lookupUser (*b, 2, 4, ph)) < 1e-7);
    }
    {   // odd lengths are resampled; a short file is one cycle
        auto b = WaveTables::buildUser (cycle (600, 1), 0, sr);
        check ("odd length: a 600-sample sine becomes one clean frame", b && b->frames == 1 && harm (*b, 0, 2, 1) > 0.3 && harm (*b, 0, 2, 2) < 0.01 * harm (*b, 0, 2, 1));
        auto c = WaveTables::buildUser (cycle (256, 5), 0, sr);
        check ("short power-of-two file: harmonic 5 stays harmonic 5", c && harm (*c, 0, 2, 5) > 0.3 && harm (*c, 0, 2, 1) < 1e-4);
        std::vector<float> d; append (d, cycle (1024, 1)); append (d, cycle (1024, 4));
        auto e = WaveTables::buildUser (d, 1024, sr);
        check ("1024-sample frames: two frames, harmonics 1 and 4", e && e->frames == 2 && harm (*e, 0, 2, 1) > 0.3 && harm (*e, 1, 2, 4) > 0.3);
    }
    {   // DC, silence, caps and rejects
        auto dc = cycle (2048, 1); for (auto& v : dc) v += 0.3f;
        auto b = WaveTables::buildUser (dc, 0, sr); const float* t = b->at (0, 4) + 1; double m = 0; for (int n = 0; n < 2048; ++n) m += t[n];
        check ("DC offset is removed", std::abs (m / 2048) < 1e-5);
        auto z = WaveTables::buildUser (std::vector<float> (4096, 0.0f), 0, sr);
        bool fin = true; for (float v : z->t) fin &= std::isfinite (v) && v == 0.0f;
        check ("silence stays silence (no NaN from the gain)", z && fin);
        std::vector<float> big; for (int i = 0; i < 300; ++i) append (big, cycle (2048, 1 + i % 5));
        auto c = WaveTables::buildUser (big, 0, sr);
        check ("more than 256 frames: the first 256 are kept", c && c->frames == 256);
        check ("too short: null", WaveTables::buildUser ({ 0.1f }, 0, sr) == nullptr && WaveTables::buildUser ({}, 0, sr) == nullptr);
        check ("frame longer than the file: null", WaveTables::buildUser (cycle (512, 1), 2048, sr) == nullptr);
        std::vector<float> loud (2048); for (int n = 0; n < 2048; ++n) loud[(size_t) n] = n < 20 ? 8.0f : 0.0f;
        auto l = WaveTables::buildUser (loud, 0, sr); double pk = 0; for (int n = 1; n <= 2048; ++n) pk = std::max (pk, (double) std::abs (l->at (0, 4)[n]));
        check ("an impulse frame is scaled to stay within full scale", pk <= 1.0001);
    }
    {   // sample rates
        auto lo = WaveTables::buildUser (cycle (2048, 1), 0, 22050), hi = WaveTables::buildUser (cycle (2048, 1), 0, 96000);
        std::vector<float> saw (2048); for (int n = 0; n < 2048; ++n) saw[(size_t) n] = (float) (1.0 - 2.0 * n / 2048);
        auto a = WaveTables::buildUser (saw, 0, 22050), c = WaveTables::buildUser (saw, 0, 96000);
        check ("sample rate: a lower rate keeps fewer harmonics in the top band", harm (*a, 0, 5, 20) < 1e-4 && harm (*c, 0, 5, 20) > 0.003);
        check ("sample rate: both banks are still built", lo && hi && lo->sampleRate == 22050 && hi->sampleRate == 96000);
    }
    {   // smooth (cubic) morph across four frames
        std::vector<float> d; for (int k = 1; k <= 5; ++k) append (d, cycle (2048, k, 0.5));
        auto b = WaveTables::buildUser (d, 0, sr); const double ph = 0.1;
        auto lin = [&] (double x) { return wt.lookupUserMorph (*b, x, 4, ph, false); }; auto smo = [&] (double x) { return wt.lookupUserMorph (*b, x, 4, ph, true); };
        bool atFrames = true; for (int f = 0; f < 5; ++f) atFrames = atFrames && std::abs (smo (f / 4.0) - wt.lookupUser (*b, f, 4, ph)) < 1e-6;
        check ("smooth morph: passes exactly through every frame", atFrames);
        check ("smooth morph: differs from the linear blend between frames", std::abs (smo (0.375) - lin (0.375)) > 0.01);
        check ("smooth morph off (default argument): the linear blend", std::abs (wt.lookupUserMorph (*b, 0.375, 4, ph) - lin (0.375)) < 1e-9);
        const double knot = 0.5, e = 1e-4; auto slope = [&] (auto fn, double a, double c) { return (fn (c) - fn (a)) / (c - a); };
        const double ls = slope (lin, knot - 3 * e, knot - e) - slope (lin, knot + e, knot + 3 * e), ss = slope (smo, knot - 3 * e, knot - e) - slope (smo, knot + e, knot + 3 * e);
        check ("smooth morph: no corner at a frame (the slope matches on both sides), unlike linear", std::abs (ss) < 0.02 && std::abs (ls) > 0.2);
        bool bounded = true; for (int i = 0; i <= 400; ++i) bounded = bounded && std::abs (smo (i / 400.0)) < 3.0f; check ("smooth morph: stays bounded over the whole sweep", bounded);
        auto two = WaveTables::buildUser ([&] { std::vector<float> t; append (t, cycle (2048, 1, 0.5)); append (t, cycle (2048, 2, 0.5)); return t; }(), 0, sr);
        check ("smooth morph: with two frames it is the linear blend", std::abs (wt.lookupUserMorph (*two, 0.3, 4, ph, true) - wt.lookupUserMorph (*two, 0.3, 4, ph, false)) < 1e-9);
        auto one = WaveTables::buildUser (cycle (2048, 1, 0.5), 0, sr); check ("smooth morph: a one-frame table is unaffected", std::abs (wt.lookupUserMorph (*one, 0.7, 4, ph, true) - wt.lookupUser (*one, 0, 4, ph)) < 1e-9);
    }
    std::printf ("%d of %d checks passed\n", passed, total);
    return passed == total ? 0 : 1;
}
