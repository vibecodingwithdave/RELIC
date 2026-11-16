// Tape / distortion algorithms and band routing (no JUCE).
#include "../Source/Dsp/Effects.h"
#include <cstdio>
#include <vector>
using namespace rf;
static int passed = 0, total = 0;
static void check (const char* n, bool ok) { ++total; passed += ok; std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", n); }
static const double SR = 48000.0;
static std::vector<float> run (float freq, float amp, float drive, const Tape::Opts& o, float tone = 1.0f, float wobble = 0.0f, int n = 48000)
{
    Tape t; t.prepare (SR); std::vector<float> out; for (int i = 0; i < n; ++i) { float l = amp * (float) std::sin (2 * 3.14159265358979 * freq * i / SR), r = l; t.process (l, r, drive, tone, wobble, SR, o); out.push_back (l); } return out;
}
static double harm (const std::vector<float>& x, double f, int k)      // amplitude of the k-th harmonic, last 0.5 s
{
    double re = 0, im = 0; const int n0 = (int) x.size() / 2, n = (int) x.size() - n0; for (int i = 0; i < n; ++i) { const double ph = 2 * 3.14159265358979 * f * k * (i + n0) / SR; re += x[(size_t) (i + n0)] * std::cos (ph); im += x[(size_t) (i + n0)] * std::sin (ph); }
    return 2.0 * std::sqrt (re * re + im * im) / n;
}
static double rms (const std::vector<float>& x) { double s = 0; for (size_t i = x.size() / 2; i < x.size(); ++i) s += x[i] * x[i]; return std::sqrt (s / (x.size() / 2)); }
static double mean (const std::vector<float>& x) { double s = 0; for (size_t i = x.size() / 2; i < x.size(); ++i) s += x[i]; return s / (x.size() / 2); }
int main()
{
    Tape::Opts def;
    {   // the original path, written out here: tanh, same drive law, same delay and tone
        Tape t; t.prepare (SR); DelayLine w; w.prepare ((int) (SR * 0.012)); float lp = 0; double p1 = 0, p2 = 0; bool same = true; const float drive = 0.6f, tone = 0.5f, wob = 0.3f;
        for (int i = 0; i < 20000; ++i)
        {
            float l = 0.4f * (float) std::sin (2 * 3.14159265358979 * 220.0 * i / SR), r = l; const float in = l; t.process (l, r, drive, tone, wob, SR, def);
            p1 += 0.6 / SR; p1 -= std::floor (p1); p2 += 7.0 / SR; p2 -= std::floor (p2);
            const double d = (0.003 + wob * (0.0015 * std::sin (2 * kPi * p1) + 0.00015 * std::sin (2 * kPi * p2))) * SR; const float g = 1.0f + drive * 7.0f, ref = 0.3f, norm = ref / std::tanh (g * ref);
            const float a = (float) std::exp (-2.0 * kPi * (2000.0 * std::pow (9.0, (double) tone)) / SR);
            w.write (in); const float y = std::tanh (g * w.read (d)) * norm; w.advance(); lp = (1.0f - a) * y + a * lp; same = same && l == lp;
        }
        check ("default options (Tape, whole signal, mix 1) are bit-identical to the original tape", same);
    }
    for (int ty = 0; ty < Tape::kTypes; ++ty)
    {
        Tape::Opts o; o.type = ty; o.bias = 0.3f; auto y = run (220.0f, 0.8f, 1.0f, o); double pk = 0; bool fin = true; for (float v : y) { pk = std::max (pk, (double) std::abs (v)); fin = fin && std::isfinite (v); }
        char nm[80]; std::snprintf (nm, sizeof nm, "type %d at full drive: finite, audible and bounded (peak %.2f)", ty, pk); check (nm, fin && pk > 0.1 && pk < 2.0);
    }
    {   Tape::Opts soft, hard; soft.type = Tape::SoftClip; hard.type = Tape::HardClip; auto a = run (220.0f, 0.5f, 0.8f, soft), b = run (220.0f, 0.5f, 0.8f, hard);
        check ("hard clip adds more 3rd harmonic than soft clip at the same drive", harm (b, 220, 3) > 1.3 * harm (a, 220, 3)); check ("a symmetric shape has almost no 2nd harmonic (hard clip)", harm (b, 220, 2) < 0.05 * harm (b, 220, 1)); }
    {   Tape::Opts t0, t1; t0.type = t1.type = Tape::Tube; t1.bias = 0.6f; auto a = run (220.0f, 0.5f, 0.6f, t0), b = run (220.0f, 0.5f, 0.6f, t1);
        check ("Tube bias makes even harmonics (2nd harmonic far above the unbiased one)", harm (b, 220, 2) > 8.0 * harm (a, 220, 2) && harm (b, 220, 2) > 0.02 * harm (b, 220, 1));
        check ("and the offset it creates is removed (average near zero)", std::abs (mean (b)) < 0.01); }
    {   Tape::Opts f, s; f.type = Tape::Fold; s.type = Tape::SoftClip; auto a = run (220.0f, 0.8f, 1.0f, f), b = run (220.0f, 0.8f, 1.0f, s);
        check ("wavefold at full drive is much richer than soft clipping (5th+7th harmonic)", harm (a, 220, 5) + harm (a, 220, 7) > 3.0 * (harm (b, 220, 5) + harm (b, 220, 7))); }
    {   Tape::Opts r; r.type = Tape::Rectify; auto a = run (220.0f, 0.5f, 0.6f, r); check ("Rectify adds a strong 2nd harmonic", harm (a, 220, 2) > 0.05 * harm (a, 220, 1)); }
    {   Tape::Opts o; o.type = Tape::HardClip; auto full = run (1000.0f, 0.05f, 0.0f, o), low = run (1000.0f, 0.05f, 0.0f, [&] { auto q = o; q.band = 1; return q; }()),
            mid = run (1000.0f, 0.05f, 0.0f, [&] { auto q = o; q.band = 2; return q; }()), high = run (1000.0f, 0.05f, 0.0f, [&] { auto q = o; q.band = 3; return q; }());
        const double r0 = rms (full); auto db = [&] (const std::vector<float>& v) { return 20 * std::log10 (rms (v) / r0); };
        check ("band split is level-flat for a signal in each band (below the clip, 1 kHz: low/mid/high within 0.3 dB)", std::abs (db (low)) < 0.3 && std::abs (db (mid)) < 0.3 && std::abs (db (high)) < 0.3);
        for (double f : { 60.0, 5000.0 }) { auto a = run ((float) f, 0.05f, 0.0f, o), b = run ((float) f, 0.05f, 0.0f, [&] { auto q = o; q.band = 2; return q; }()); char nm[80]; std::snprintf (nm, sizeof nm, "flat at %.0f Hz too (within 0.3 dB)", f); check (nm, std::abs (20 * std::log10 (rms (b) / rms (a))) < 0.3); } }
    {   Tape::Opts hi; hi.type = Tape::HardClip; hi.band = 1; auto onlyLow = run (5000.0f, 0.5f, 1.0f, hi); hi.band = 0; auto whole = run (5000.0f, 0.5f, 1.0f, hi);
        check ("Low band only: a 5 kHz tone is left clean (3rd harmonic far below the full-range distortion)", harm (onlyLow, 5000, 3) < 0.1 * harm (whole, 5000, 3));
        Tape::Opts lo; lo.type = Tape::HardClip; lo.band = 1; auto lowTone = run (100.0f, 0.5f, 1.0f, lo); check ("Low band only: a 100 Hz tone is distorted (3rd harmonic present)", harm (lowTone, 100, 3) > 0.03);
        Tape::Opts hb; hb.type = Tape::HardClip; hb.band = 3; auto hClean = run (100.0f, 0.5f, 1.0f, hb); check ("High band only: a 100 Hz tone is left clean", harm (hClean, 100, 3) < 0.01); }
    {   Tape::Opts m; m.type = Tape::HardClip; auto wet = run (220.0f, 0.5f, 1.0f, m); m.mix = 0.0f; auto dry = run (220.0f, 0.5f, 1.0f, m); m.mix = 0.5f; auto half = run (220.0f, 0.5f, 1.0f, m);
        check ("Mix 0 gives no distortion (3rd harmonic gone), Mix 0.5 halves it", harm (dry, 220, 3) < 0.02 * harm (wet, 220, 3) && std::abs (harm (half, 220, 3) / harm (wet, 220, 3) - 0.5) < 0.12); }
    std::printf ("%d/%d passed\n", passed, total); return passed == total ? 0 : 1;
}
