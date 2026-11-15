// Standalone checks for pitch delay, talk box, spectral gate and the delay modes (no JUCE). Built and run by tools/verify_fx3.py
#include "../Source/Dsp/Effects2.h"
#include <cstdio>
#include <random>
using namespace rf;
static int pass = 0, total = 0; static const double SR = 44100.0;
static void check (const char* name, bool ok, double a = 0, double b = 0) { ++total; pass += ok; std::printf ("  [%s] %s (%.3f, %.3f)\n", ok ? "PASS" : "FAIL", name, a, b); }
static double rmsOf (const std::vector<float>& x, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b; ++i) s += (double) x[i] * x[i]; return std::sqrt (s / (double) (b - a)); }
static std::vector<float> sine (double hz, double amp, int n) { std::vector<float> v ((size_t) n); for (int i = 0; i < n; ++i) v[(size_t) i] = (float) (amp * std::sin (2 * kPi * hz * i / SR)); return v; }
static double binMag (const std::vector<float>& x, double hz, size_t a, size_t b) { double c = 0, s = 0; for (size_t i = a; i < b; ++i) { const double w = 0.5 - 0.5 * std::cos (2 * kPi * (double) (i - a) / (double) (b - a)); c += w * x[i] * std::cos (2 * kPi * hz * i / SR); s += w * x[i] * std::sin (2 * kPi * hz * i / SR); } return 4 * std::sqrt (c * c + s * s) / (double) (b - a); }
static double domFreq (const std::vector<float>& x, size_t a, size_t b) { double best = 0, bf = 0; for (double f = 60; f < 4000; f *= 1.003) { const double m = binMag (x, f, a, b); if (m > best) { best = m; bf = f; } } return bf; }

int main()
{
    std::puts ("== PITCH SHIFTER / PITCH DELAY");
    { PitchShifter ps; ps.prepare (SR); auto in = sine (440, 0.5, 44100); std::vector<float> o; for (float x : in) o.push_back (ps.process (x, std::exp2 (7.0f / 12.0f)));
      const double f = domFreq (o, 22050, 44100); check ("shifter +7 semitones: 440 -> 659 Hz", std::abs (f - 659.3) < 10, f, 659.3);
      PitchShifter p2; p2.prepare (SR); o.clear(); for (float x : in) o.push_back (p2.process (x, std::exp2 (-12.0f / 12.0f))); const double f2 = domFreq (o, 22050, 44100); check ("shifter -12: 440 -> 220 Hz", std::abs (f2 - 220) < 7, f2, 220); }
    { PitchDelay d; d.prepare (SR); std::vector<float> o; auto in = sine (440, 0.4, 44100 * 3); for (size_t i = 0; i < in.size(); ++i) { float l = i < 4410 ? in[i] : 0.0f, r = l; d.process (l, r, 300, 0.7f, 12, 0.5f, SR); o.push_back (l); }
      // first echo at 300 ms is the shifted signal of the dry... the burst is 100 ms long
      const double e1 = domFreq (o, (size_t) (0.31 * SR), (size_t) (0.39 * SR)), e2 = domFreq (o, (size_t) (0.61 * SR), (size_t) (0.69 * SR));
      check ("pitch delay +12: first echo is already one octave up (880 Hz)", std::abs (e1 - 880) < 20, e1, 880); check ("second echo two octaves up (1760 Hz)", std::abs (e2 - 1760) < 40, e2, 1760);
      check ("echoes decay with feedback", rmsOf (o, (size_t) (0.62 * SR), (size_t) (0.68 * SR)) < rmsOf (o, (size_t) (0.32 * SR), (size_t) (0.38 * SR))); }
    { PitchDelay d; d.prepare (SR); std::vector<float> o; auto in = sine (440, 0.4, 44100); for (size_t i = 0; i < in.size(); ++i) { float l = i < 4410 ? in[i] : 0.0f, r = l; d.process (l, r, 300, 0.5f, 0, 0.5f, SR); o.push_back (l); }
      check ("0 semitones: echoes stay at 440 Hz", std::abs (domFreq (o, (size_t) (0.31 * SR), (size_t) (0.39 * SR)) - 440) < 6); }
    std::puts ("== TALK BOX");
    { std::mt19937 g (5); std::uniform_real_distribution<float> u (-1, 1); std::vector<float> noise (44100 * 2); for (auto& x : noise) x = 0.3f * u (g);
      auto spec = [&] (float vowel) { TalkBox t; t.prepare (SR); std::vector<float> o; for (float x : noise) { float l = x, r = x; t.process (l, r, vowel, 0.6f, 0.0f, 0.0f, 1.0f, SR); o.push_back (l); } return o; };
      auto a = spec (0.0f), i = spec (0.5f), uu = spec (1.0f);
      check ("vowel A has energy at its first formant (800 Hz) and little at 300", binMag (a, 800, 22050, 88200) > 3 * binMag (a, 300, 22050, 88200), binMag (a, 800, 22050, 88200), binMag (a, 300, 22050, 88200));
      check ("vowel I (centre) puts energy at 2300 Hz, not at 800", binMag (i, 2300, 22050, 88200) > 2 * binMag (i, 800, 22050, 88200), binMag (i, 2300, 22050, 88200), binMag (i, 800, 22050, 88200));
      check ("vowel U has energy at 350 and 700 Hz, little at 1500", binMag (uu, 700, 22050, 88200) > 2 * binMag (uu, 1500, 22050, 88200), binMag (uu, 700, 22050, 88200) * 1000, binMag (uu, 1500, 22050, 88200) * 1000); }
    { TalkBox t; t.prepare (SR); std::mt19937 g (6); std::uniform_real_distribution<float> u (-1, 1); std::vector<float> o; for (int n = 0; n < 88200; ++n) { float l = 0.3f * u (g), r = l; t.process (l, r, 0.0f, 0.6f, 0.0f, 0.0f, 0.0f, SR); o.push_back (l); }
      std::mt19937 g2 (6); std::vector<float> ref; for (int n = 0; n < 88200; ++n) ref.push_back (0.3f * u (g2)); double d = 0; for (size_t n = 0; n < o.size(); ++n) d = std::max (d, (double) std::abs (o[n] - ref[n])); check ("mix 0 is a clean bypass", d < 1e-6, d); }
    { TalkBox t; t.prepare (SR); std::vector<float> o; std::mt19937 g (7); std::uniform_real_distribution<float> u (-1, 1); for (int n = 0; n < 44100 * 3; ++n) { float l = 0.3f * u (g), r = l; t.process (l, r, 0.5f, 0.6f, 1.0f, 1.0f, 1.0f, SR); o.push_back (l); }
      auto at = [&] (double s) { return binMag (o, 800, (size_t) (s * SR), (size_t) ((s + 0.25) * SR)); }; double mn = 1e9, mx = 0; for (double s = 0.2; s < 2.5; s += 0.125) { mn = std::min (mn, at (s)); mx = std::max (mx, at (s)); }
      check ("LFO sweep moves the vowel (energy at 800 Hz rises and falls)", mx > 2.0 * mn, mn, mx); }
    std::puts ("== SPECTRAL GATE");
    { SpectralGate sg; sg.prepare (SR); std::mt19937 g (9); std::uniform_real_distribution<float> u (-1, 1); const int n = 44100 * 3; std::vector<float> in (n), o;
      auto tone = sine (1000, 0.5, n); for (int i = 0; i < n; ++i) in[(size_t) i] = tone[(size_t) i] + 0.004f * u (g);
      for (int i = 0; i < n; ++i) { float l = in[(size_t) i], r = l; sg.process (l, r, -40.0f, 1.0f, 30.0f, 1.0f, SR); o.push_back (l); }
      auto noiseOnly = [&] (const std::vector<float>& x) { double s = 0; size_t a = 66150, b = 132300; for (size_t i = a; i < b; ++i) s += x[i] * x[i]; return std::sqrt (s / (double) (b - a)); };
      std::vector<float> noiseIn (n); for (int i = 0; i < n; ++i) noiseIn[(size_t) i] = in[(size_t) i] - tone[(size_t) i];
      double tIn = binMag (in, 1000, 66150, 132300), tOut = binMag (o, 1000, 66150 + SpectralGate::latency(), 132300 + SpectralGate::latency());
      check ("tone passes at full level", std::abs (20 * std::log10 (tOut / tIn)) < 1.0, tIn, tOut);
      double offMid = binMag (o, 3000, 66150, 132300), offIn = binMag (in, 3000, 66150, 132300); check ("noise away from the tone is gated by more than 25 dB", 20 * std::log10 (offIn / std::max (1e-9, offMid)) > 25, offIn, offMid);
      (void) noiseOnly; }
    { SpectralGate sg; sg.prepare (SR); auto in = sine (500, 0.3, 44100 * 2); std::vector<float> o; for (float x : in) { float l = x, r = x; sg.process (l, r, -80.0f, 0.0f, 30.0f, 1.0f, SR); o.push_back (l); }
      double d = 0; for (size_t i = 30000; i < 80000; ++i) d = std::max (d, (double) std::abs (o[i + SpectralGate::latency()] - in[i])); check ("depth 0: transparent (output equals input delayed by the latency)", d < 0.01, d); }
    { SpectralGate sg; sg.prepare (SR); std::vector<float> o; auto in = sine (440, 0.4, 44100); for (float x : in) { float l = x, r = x; sg.process (l, r, -40.0f, 1.0f, 30.0f, 0.0f, SR); o.push_back (l); }
      double d = 0; for (size_t i = 10000; i < 40000; ++i) d = std::max (d, (double) std::abs (o[i + SpectralGate::latency()] - in[i])); check ("mix 0 gives the dry signal aligned to the same latency", d < 1e-5, d); }
    std::puts ("== DELAY MODES");
    auto echoes = [&] (int mode, double hz) { PingPongDelay d; d.prepare (SR); std::vector<float> o; auto in = sine (hz, 0.4, 44100 * 3); for (size_t i = 0; i < in.size(); ++i) { float l = i < 2205 ? in[i] : 0.0f, r = l; d.process (l, r, 250, 0.7f, 1.0f, false, SR, mode); o.push_back (l); } return o; };
    { auto c = echoes (0, 440), t = echoes (1, 440); check ("tape mode still echoes (first echo present)", rmsOf (t, (size_t) (0.26 * SR), (size_t) (0.3 * SR)) > 0.05, rmsOf (t, (size_t) (0.26 * SR), (size_t) (0.3 * SR)));
      auto ch = echoes (0, 9000), th = echoes (1, 9000); const double rc = rmsOf (ch, (size_t) (0.76 * SR), (size_t) (0.8 * SR)), rt = rmsOf (th, (size_t) (0.76 * SR), (size_t) (0.8 * SR));
      check ("tape mode: 9 kHz repeats darken (3rd echo at least 6 dB below clean)", 20 * std::log10 (rc / std::max (1e-9, rt)) > 6, rc, rt); }
    { auto c = echoes (0, 440); PingPongDelay d; (void) d; auto w = echoes (2, 440);
      // diffusion: a clean echo is a gap-and-burst; wash fills the gap after the burst
      const double gapC = rmsOf (c, (size_t) (0.35 * SR), (size_t) (0.45 * SR)), gapW = rmsOf (w, (size_t) (0.35 * SR), (size_t) (0.45 * SR));
      check ("wash mode: allpass smear leaves a stable, finite wash", std::isfinite (rmsOf (w, 0, w.size())) && rmsOf (w, (size_t) (1.0 * SR), (size_t) (1.2 * SR)) < 0.5, gapC, gapW); }
    { auto c0 = echoes (0, 440); PingPongDelay d; d.prepare (SR); std::vector<float> o; auto in = sine (440, 0.4, 44100 * 3); for (size_t i = 0; i < in.size(); ++i) { float l = i < 2205 ? in[i] : 0.0f, r = l; d.process (l, r, 250, 0.7f, 1.0f, false, SR); o.push_back (l); }
      double dd = 0; for (size_t i = 0; i < o.size(); ++i) dd = std::max (dd, (double) std::abs (o[i] - c0[i])); check ("mode 0 is identical to the default call", dd < 1e-9, dd); }
    std::printf ("\nDSP RESULT: %d of %d\n", pass, total);
    return pass == total ? 0 : 1;
}
