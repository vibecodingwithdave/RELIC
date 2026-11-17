// Effects, each specified from measured behavior.
#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include "Svf.h"

namespace rf {

constexpr double kPi = 3.14159265358979323846;

struct Lfo
{
    enum Shape { Sine = 0, Triangle = 1, Square = 2 };
    double phase = 0.0;
    inline float next (double hz, double sr, int shape)
    {
        phase += hz / sr; phase -= std::floor (phase);
        const double s = std::sin (2.0 * kPi * phase);
        if (shape == Triangle) return (float) (2.0 / kPi * std::asin (s));
        if (shape == Square)   return s >= 0.0 ? 1.0f : -1.0f;
        return (float) s;
    }
};

// gain = 1 - depth * (0.5 + 0.5 sin): depth 1.0 reaches silence
struct Tremolo
{
    Lfo lfo; void lockPhase (double p) { lfo.phase = p; }
    inline void process (float& l, float& r, float rateHz, float depth, double sr)
    {
        const float m = 0.5f + 0.5f * lfo.next (rateHz, sr, Lfo::Sine);
        const float g = 1.0f - depth * m;
        l *= g; r *= g;
    }
};

// depth 1.0 sweeps hard left to hard right
struct Panner
{
    Lfo lfo; void lockPhase (double p) { lfo.phase = p; }
    inline void process (float& l, float& r, float rateHz, float depth, int shape, double sr)
    {
        const float p = depth * lfo.next (rateHz, sr, shape);          // -1..+1
        const float a = (p + 1.0f) * (float) (kPi / 4.0);
        const float m = 0.5f * (l + r) * 1.41421356f;
        l = m * std::cos (a); r = m * std::sin (a);
    }
};

// Ping-pong: each echo is feedback x the previous one, alternating left and right.
struct PingPongDelay
{
    void prepare (double sr)
    {
        size = (int) (sr * 2.1) + 8;
        bufL.assign ((size_t) size, 0.0f); bufR.assign ((size_t) size, 0.0f);
        pos = 0; lpL = lpR = 0.0f; wob = 0.0f;
        for (auto& a : ap) { a.size = (int) (sr * 0.004); a.buf.assign ((size_t) a.size + 1, 0.0f); a.pos = 0; }
        ap[0].size = (int) (sr * 0.0032); ap[1].size = (int) (sr * 0.0024); ap[2].size = (int) (sr * 0.0037); ap[3].size = (int) (sr * 0.0029);
    }
    // mode 0 = clean, 1 = tape (dark, saturating, wobbling repeats), 2 = wash (repeats smear into a wash)
    inline void process (float& l, float& r, float timeMs, float feedback, float mix, bool pingPong, double sr, int mode = 0)
    {
        const float in = 0.5f * (l + r);
        double d = std::min ((double) (size - 4), std::max (1.0, timeMs * 0.001 * sr));
        if (mode == 1) { wob += 0.7f / (float) sr; if (wob >= 1.0f) wob -= 1.0f; d *= 1.0 + 0.003 * std::sin (2.0 * kPi * wob) + 0.0012 * std::sin (2.0 * kPi * wob * 7.3); }
        float outL = read (bufL, d), outR = read (bufR, d);
        float fbL = outL, fbR = outR;
        if (mode == 1)
        {
            const float a = std::exp (-2.0f * (float) kPi * 3200.0f / (float) sr);
            lpL = outL * (1.0f - a) + lpL * a; lpR = outR * (1.0f - a) + lpR * a;
            fbL = std::tanh (1.3f * lpL) / 1.3f; fbR = std::tanh (1.3f * lpR) / 1.3f; outL = lpL; outR = lpR;
        }
        else if (mode == 2)
        {
            const float a = std::exp (-2.0f * (float) kPi * 4500.0f / (float) sr);
            lpL = outL * (1.0f - a) + lpL * a; lpR = outR * (1.0f - a) + lpR * a;
            fbL = ap[1].run (ap[0].run (lpL)); fbR = ap[3].run (ap[2].run (lpR)); outL = fbL; outR = fbR;
        }
        if (pingPong)
        {
            // feedback on both legs, so every echo is feedback x the previous one
            bufL[(size_t) pos] = in + feedback * fbR;
            bufR[(size_t) pos] = feedback * fbL;
        }
        else
        {
            bufL[(size_t) pos] = l + feedback * fbL;
            bufR[(size_t) pos] = r + feedback * fbR;
        }
        if (++pos >= size) pos = 0;
        l = l * (1.0f - mix) + outL * mix;
        r = r * (1.0f - mix) + outR * mix;
    }
private:
    struct Ap
    {
        std::vector<float> buf; int size = 1, pos = 0;
        inline float run (float x) { const float z = buf[(size_t) pos]; const float y = -0.65f * x + z; buf[(size_t) pos] = x + 0.65f * y; if (++pos >= size) pos = 0; return y; }
    };
    inline float read (const std::vector<float>& b, double delay) const
    {
        double rp = pos - delay; if (rp < 0) rp += size;
        const int i0 = (int) rp; const float f = (float) (rp - i0);
        const int i1 = i0 + 1 >= size ? 0 : i0 + 1;
        return b[(size_t) i0] * (1.0f - f) + b[(size_t) i1] * f;
    }
    std::vector<float> bufL, bufR; int size = 1, pos = 0; float lpL = 0, lpR = 0, wob = 0; Ap ap[4];
};

// Hard threshold. gain reduction = (1 - 1/ratio) * (level - threshold); detector = RMS + 2 dB.
struct Compressor
{
    inline void process (float& l, float& r, float thresholdDb, float ratio, float attackMs, float releaseMs, float makeupDb, double sr)
    {
        const float x = 0.5f * (l + r);
        const float cr = 1.0f - std::exp (-1.0f / (0.010f * (float) sr));      // 10 ms RMS window
        ms += cr * (x * x - ms);
        const float levelDb = 10.0f * std::log10 (std::max (ms, 1e-12f)) + 2.0f;
        const float over = levelDb - thresholdDb;
        const float targetGr = over > 0.0f ? -(1.0f - 1.0f / ratio) * over : 0.0f;
        const float a = std::exp (-1.0f / (std::max (0.1f, attackMs) * 0.001f * (float) sr));
        const float rl = std::exp (-1.0f / (std::max (1.0f, releaseMs) * 0.001f * (float) sr));
        const float c = targetGr < grDb ? a : rl;                                // more reduction: attack
        grDb = c * grDb + (1.0f - c) * targetGr;
        const float g = std::pow (10.0f, (grDb + makeupDb) / 20.0f);
        l *= g; r *= g;
    }
    float ms = 0.0f, grDb = 0.0f;
};

// Uniform quantizer plus sample-and-hold decimation (no anti-alias filter).
struct Crusher
{
    inline void process (float& l, float& r, float bits, float downsample, float mix)
    {
        counter += 1.0f;
        if (counter >= downsample) { counter -= downsample; heldL = quant (l, bits); heldR = quant (r, bits); }
        l = l * (1.0f - mix) + heldL * mix;
        r = r * (1.0f - mix) + heldR * mix;
    }
private:
    static inline float quant (float x, float bits)
    {
        const float steps = std::pow (2.0f, bits - 1.0f);
        return std::round (std::max (-1.0f, std::min (1.0f, x)) * steps) / steps;
    }
    float counter = 0.0f, heldL = 0.0f, heldR = 0.0f;
};

// Shared stereo fractional delay line (linear interpolation).
struct DelayLine
{
    void prepare (int maxSamples) { size = maxSamples + 4; buf.assign ((size_t) size, 0.0f); pos = 0; }
    inline void write (float x) { buf[(size_t) pos] = x; }
    inline void advance() { if (++pos >= size) pos = 0; }
    inline float read (double delaySamples) const
    {
        double rp = pos - delaySamples; while (rp < 0) rp += size;
        const int i0 = (int) rp; const float f = (float) (rp - i0); const int i1 = i0 + 1 >= size ? 0 : i0 + 1;
        return buf[(size_t) i0] * (1.0f - f) + buf[(size_t) i1] * f;
    }
    std::vector<float> buf; int size = 1, pos = 0;
};

// Chorus: three voices per side, 12 ms base delay swept by +/- 6 ms x depth.
struct Chorus
{
    void prepare (double sr) { for (auto& d : line) d.prepare ((int) (sr * 0.05)); phase = 0.0; }
    inline void process (float& l, float& r, float rateHz, float depth, float mix, double sr)
    {
        phase += rateHz / sr; phase -= std::floor (phase);
        line[0].write (l); line[1].write (r);
        const double base = 0.012 * sr, swing = 0.006 * sr * depth;
        float wl = 0.0f, wr = 0.0f;
        for (int k = 0; k < 3; ++k)
        {
            const double pl = phase + k / 3.0, pr = phase + k / 3.0 + 1.0 / 6.0;
            wl += line[0].read (base + swing * std::sin (2.0 * kPi * pl));
            wr += line[1].read (base + swing * std::sin (2.0 * kPi * pr));
        }
        line[0].advance(); line[1].advance();
        l = l * (1.0f - mix) + wl * (1.0f / 3.0f) * mix;
        r = r * (1.0f - mix) + wr * (1.0f / 3.0f) * mix;
    }
    DelayLine line[2]; double phase = 0.0; void lockPhase (double p) { phase = p; }
};

// Flanger: 0.5 ms plus up to 6 ms swept by the LFO, with feedback of either sign.
struct Flanger
{
    void prepare (double sr) { for (auto& d : line) d.prepare ((int) (sr * 0.012)); phase = 0.0; }
    inline void process (float& l, float& r, float rateHz, float depth, float feedback, float mix, double sr)
    {
        phase += rateHz / sr; phase -= std::floor (phase);
        const double dl = (0.0005 + 0.5 * (1.0 + std::sin (2.0 * kPi * phase)) * 0.006 * depth) * sr;
        const double dr = (0.0005 + 0.5 * (1.0 + std::sin (2.0 * kPi * (phase + 0.25))) * 0.006 * depth) * sr;
        const float wl = line[0].read (dl), wr = line[1].read (dr);
        line[0].write (l + feedback * wl); line[1].write (r + feedback * wr);
        line[0].advance(); line[1].advance();
        l = l * (1.0f - mix) + wl * mix; r = r * (1.0f - mix) + wr * mix;
    }
    DelayLine line[2]; double phase = 0.0; void lockPhase (double p) { phase = p; }
};

// Reverb: eight damped feedback combs and four allpasses per side (Schroeder/Freeverb layout).
// Comb gains are computed from the decay time, so "size" is a real RT60: 0.3 s at 0 up to 6 s at 1.
struct Reverb
{
    static constexpr int kCombs = 8, kAll = 4;
    void prepare (double sr)
    {
        static const int cd[kCombs] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        static const int ad[kAll] = { 556, 441, 341, 225 };
        const double k = sr / 44100.0;
        for (int c = 0; c < 2; ++c)
        {
            for (int i = 0; i < kCombs; ++i) { comb[c][i].len = (int) (cd[i] * k) + c * (int) (23 * k); comb[c][i].buf.assign ((size_t) comb[c][i].len, 0.0f); comb[c][i].pos = 0; comb[c][i].lp = 0.0f; }
            for (int i = 0; i < kAll; ++i)  { all[c][i].len = (int) (ad[i] * k) + c * (int) (23 * k); all[c][i].buf.assign ((size_t) all[c][i].len, 0.0f); all[c][i].pos = 0; }
        }
        pre.prepare ((int) (sr * 0.13)); lastSize = -1.0f; lastDamp = -1.0f;
    }
    static double rt60ForSize (float size) { return 0.3 * std::pow (20.0, (double) size); }
    inline void process (float& l, float& r, float size, float damp, float predelayMs, float mix, double sr)
    {
        if (size != lastSize || damp != lastDamp)
        {
            lastSize = size; lastDamp = damp; const double rt = rt60ForSize (size);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < kCombs; ++i) { comb[c][i].g = (float) std::pow (10.0, -3.0 * comb[c][i].len / (rt * sr)); comb[c][i].damp = damp * 0.85f; }
        }
        pre.write (0.5f * (l + r));
        const float in = pre.read (std::max (1.0, predelayMs * 0.001 * sr)) * 0.03f; pre.advance();
        float out[2];
        for (int c = 0; c < 2; ++c)
        {
            float acc = 0.0f;
            for (int i = 0; i < kCombs; ++i)
            {
                auto& cb = comb[c][i]; const float y = cb.buf[(size_t) cb.pos];
                cb.lp = y * (1.0f - cb.damp) + cb.lp * cb.damp;
                cb.buf[(size_t) cb.pos] = in + cb.lp * cb.g; if (++cb.pos >= cb.len) cb.pos = 0; acc += y;
            }
            for (int i = 0; i < kAll; ++i)
            {
                auto& a = all[c][i]; const float b = a.buf[(size_t) a.pos]; const float y = -acc + b;
                a.buf[(size_t) a.pos] = acc + b * 0.5f; if (++a.pos >= a.len) a.pos = 0; acc = y;
            }
            out[c] = acc * 3.0f;
        }
        l = l * (1.0f - mix) + out[0] * mix; r = r * (1.0f - mix) + out[1] * mix;
    }
private:
    struct Comb { std::vector<float> buf; int len = 1, pos = 0; float lp = 0, g = 0.8f, damp = 0; };
    struct Allp { std::vector<float> buf; int len = 1, pos = 0; };
    Comb comb[2][kCombs]; Allp all[2][kAll]; DelayLine pre; float lastSize = -1.0f, lastDamp = -1.0f;
};

// ---------------------------------------------------------------- second effects batch

// RBJ biquad (direct form I, double precision).
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    void reset() { x1 = x2 = y1 = y2 = 0; }
    inline float process (float xf)
    {
        const double x = xf, y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y; return (float) y;
    }
    void set (double nb0, double nb1, double nb2, double a0, double na1, double na2) { b0 = nb0 / a0; b1 = nb1 / a0; b2 = nb2 / a0; a1 = na1 / a0; a2 = na2 / a0; }
    void lowShelf (double f, double db, double sr)
    {
        const double A = std::pow (10.0, db / 40.0), w = 2 * kPi * f / sr, cs = std::cos (w), al = std::sin (w) / std::sqrt (2.0), sA = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * cs + sA), 2 * A * ((A - 1) - (A + 1) * cs), A * ((A + 1) - (A - 1) * cs - sA), (A + 1) + (A - 1) * cs + sA, -2 * ((A - 1) + (A + 1) * cs), (A + 1) + (A - 1) * cs - sA);
    }
    void highShelf (double f, double db, double sr)
    {
        const double A = std::pow (10.0, db / 40.0), w = 2 * kPi * f / sr, cs = std::cos (w), al = std::sin (w) / std::sqrt (2.0), sA = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * cs + sA), -2 * A * ((A - 1) + (A + 1) * cs), A * ((A + 1) + (A - 1) * cs - sA), (A + 1) - (A - 1) * cs + sA, 2 * ((A - 1) - (A + 1) * cs), (A + 1) - (A - 1) * cs - sA);
    }
    void peak (double f, double db, double q, double sr)
    {
        const double A = std::pow (10.0, db / 40.0), w = 2 * kPi * f / sr, cs = std::cos (w), al = std::sin (w) / (2 * q);
        set (1 + al * A, -2 * cs, 1 - al * A, 1 + al / A, -2 * cs, 1 - al / A);
    }
    void lowpass (double f, double q, double sr)
    {
        const double w = 2 * kPi * std::min (f, sr * 0.49) / sr, cs = std::cos (w), al = std::sin (w) / (2 * q);
        set ((1 - cs) / 2, 1 - cs, (1 - cs) / 2, 1 + al, -2 * cs, 1 - al);
    }
    void highpass (double f, double q, double sr)
    {
        const double w = 2 * kPi * f / sr, cs = std::cos (w), al = std::sin (w) / (2 * q);
        set ((1 + cs) / 2, -(1 + cs), (1 + cs) / 2, 1 + al, -2 * cs, 1 - al);
    }
};

// Three-band equaliser: low shelf at 200 Hz, one peaking band (adjustable frequency, Q 1), high shelf at 4 kHz.
struct Equalizer
{
    void prepare (double) { lastKey = -1e9; for (int c = 0; c < 2; ++c) lo[c].reset(), md[c].reset(), hi[c].reset(); }
    inline void process (float& l, float& r, float lowDb, float midHz, float midDb, float highDb, double sr)
    {
        const double key = lowDb * 1.0 + midHz * 3.0 + midDb * 7.0 + highDb * 13.0;
        if (key != lastKey)
        {
            lastKey = key;
            for (int c = 0; c < 2; ++c) { lo[c].lowShelf (200.0, lowDb, sr); md[c].peak (midHz, midDb, 1.0, sr); hi[c].highShelf (4000.0, highDb, sr); }
        }
        l = hi[0].process (md[0].process (lo[0].process (l))); r = hi[1].process (md[1].process (lo[1].process (r)));
    }
    Biquad lo[2], md[2], hi[2]; double lastKey = -1e9;
};

// Tape: soft saturation (unity gain for small signals), tone roll-off and wow/flutter from a short modulated delay.
struct TapeOpts { int type = 0; float bias = 0.0f, mix = 1.0f; int band = 0; float loHz = 200.0f, hiHz = 2000.0f; };   // band: 0 whole signal, 1 low, 2 mid, 3 high

struct Tape
{
    // Distortion shapes for the Type control. 0 (Tape) is the original soft tanh and is what every older preset uses.
    enum Type { TapeSat = 0, SoftClip, HardClip, Fold, Tube, Diode, Rectify, kTypes };
    using Opts = TapeOpts;

    static inline float shape (int type, float x)
    {
        switch (type)
        {
            case SoftClip: return x / (1.0f + std::abs (x));
            case HardClip: return x > 1.0f ? 1.0f : (x < -1.0f ? -1.0f : x);
            case Fold:     return std::sin (x);                                                         // keeps folding back as the drive rises
            case Tube:     return std::tanh (x);                                                        // the bias (added before) makes it lopsided: even harmonics
            case Diode:    return x >= 0.0f ? 1.0f - std::exp (-x) : -0.5f * (1.0f - std::exp (2.0f * x));      // one side clips hard, the other soft
            case Rectify:  { const float t = std::tanh (x); return 0.6f * t + 0.4f * std::abs (t); }    // part of the wave is flipped: octave-up grit
            default:       return std::tanh (x);
        }
    }
    void prepare (double sr) { for (auto& d : wow) d.prepare ((int) (sr * 0.012)); lp[0] = lp[1] = 0.0f; p1 = p2 = 0.0; for (auto& c : ch) c = Chan(); lastLo = lastHi = -1.0f; }
    inline void process (float& l, float& r, float drive, float tone, float wobble, double sr, const Opts& o = Opts())
    {
        p1 += 0.6 / sr; p1 -= std::floor (p1); p2 += 7.0 / sr; p2 -= std::floor (p2);
        const double d = (0.003 + wobble * (0.0015 * std::sin (2 * kPi * p1) + 0.00015 * std::sin (2 * kPi * p2))) * sr;
        const float g = 1.0f + drive * 7.0f, ref = 0.3f, norm = o.type == TapeSat ? ref / std::tanh (g * ref) : ref / shape (o.type, g * ref);
        const float a = (float) std::exp (-2.0 * kPi * (2000.0 * std::pow (9.0, (double) tone)) / sr);
        const bool bias = o.type == Tube || o.type == Diode, split = o.band > 0;
        if (split && (o.loHz != lastLo || o.hiHz != lastHi)) { setCrossovers (o.loHz, o.hiHz, sr); lastLo = o.loHz; lastHi = o.hiHz; }
        float* io[2] = { &l, &r };
        for (int c = 0; c < 2; ++c)
        {
            wow[c].write (*io[c]); const float x = wow[c].read (d); wow[c].advance();
            float y;
            if (o.type == TapeSat && ! split) y = std::tanh (g * x) * norm;          // the original path, exactly
            else
            {
                auto sh = [&] (float v) { float t = g * v; if (bias) t += o.bias; float z = shape (o.type, t) * norm; if (bias) z = ch[c].dc (z); return z; };
                if (! split) y = sh (x);
                else
                {
                    auto& k = ch[c]; const float low = k.lp1b.process (k.lp1a.process (x)), rest = k.hp1b.process (k.hp1a.process (x));
                    const float mid = k.lp2b.process (k.lp2a.process (rest)), high = k.hp2b.process (k.hp2a.process (rest));
                    const float lowAp = k.lpAb.process (k.lpAa.process (low)) + k.hpAb.process (k.hpAa.process (low));       // the low band goes through the upper split too, so the three still add to a flat sum
                    y = (o.band == 1 ? sh (lowAp) : lowAp) + (o.band == 2 ? sh (mid) : mid) + (o.band == 3 ? sh (high) : high);
                }
            }
            if (o.mix < 0.9999f) y = x + (y - x) * o.mix;
            lp[c] = (1.0f - a) * y + a * lp[c]; *io[c] = lp[c];
        }
    }
    DelayLine wow[2]; float lp[2] = { 0, 0 }; double p1 = 0, p2 = 0;
private:
    struct Chan
    {
        Biquad lp1a, lp1b, hp1a, hp1b, lp2a, lp2b, hp2a, hp2b, lpAa, lpAb, hpAa, hpAb; float dcx = 0, dcy = 0;
        float dc (float v) { const float y = v - dcx + 0.995f * dcy; dcx = v; dcy = y; return y; }       // removes the offset an uneven shape makes
    };
    Chan ch[2]; float lastLo = -1.0f, lastHi = -1.0f;
    void setCrossovers (float lo, float hi, double sr)
    {
        const double q = 0.7071067811865476; if (hi < lo * 1.2f) hi = lo * 1.2f;
        for (auto& k : ch)
        {
            for (auto* b : { &k.lp1a, &k.lp1b }) b->lowpass (lo, q, sr);
            for (auto* b : { &k.hp1a, &k.hp1b }) b->highpass (lo, q, sr);
            for (auto* b : { &k.lp2a, &k.lp2b, &k.lpAa, &k.lpAb }) b->lowpass (hi, q, sr);
            for (auto* b : { &k.hp2a, &k.hp2b, &k.hpAa, &k.hpAb }) b->highpass (hi, q, sr);
        }
    }
};

// Stereo widener: mid/side with a side gain. 0 = mono, 1 = unchanged, 2 = double side.
struct Widener
{
    inline void process (float& l, float& r, float width)
    { const float m = 0.5f * (l + r), s = 0.5f * (l - r) * width; l = m + s; r = m - s; }
};

// Wah: a swept band-pass. Position comes from an LFO or from the input level.
struct Wah
{
    void prepare (double) { phase = 0.0; env = 0.0f; sv[0].reset(); sv[1].reset(); count = 0; pos = 0.5f; }
    inline void process (float& l, float& r, int mode, float rateHz, float depth, float reso, float mix, double sr, float posOff = 0.0f)
    {
        phase += rateHz / sr; phase -= std::floor (phase);
        const float lvl = std::abs (0.5f * (l + r)), ca = std::exp (-1.0f / (0.003f * (float) sr)), cr = std::exp (-1.0f / (0.1f * (float) sr));
        env = lvl > env ? ca * env + (1 - ca) * lvl : cr * env + (1 - cr) * lvl;
        if ((count++ & 3) == 0)
        {
            pos = mode == 0 ? 0.5f + 0.5f * depth * (float) std::sin (2 * kPi * phase) : std::min (1.0f, env * depth * 6.0f);
            if (posOff != 0.0f) pos = std::min (1.0f, std::max (0.0f, pos + posOff));        // the Wah Frequency automation target: shifts the sweep
            const float f = 300.0f * std::pow (2.0f, 3.0f * pos), q = 1.5f + reso * 8.0f;
            sv[0].setup (f, q, sr); sv[1].setup (f, q, sr);
        }
        const float wl = sv[0].process (l, Svf::BandPass) * 1.5f, wr = sv[1].process (r, Svf::BandPass) * 1.5f;
        l = l * (1.0f - mix) + wl * mix; r = r * (1.0f - mix) + wr * mix;
    }
    Svf sv[2]; double phase = 0.0; void lockPhase (double p) { phase = p; } float env = 0.0f, pos = 0.5f; unsigned count = 0;
};

// Lo-fi: band-limit (60 Hz up to the tone), wow, hiss and crackle. The dry path is delayed to match.
struct LoFi
{
    void prepare (double sr)
    {
        for (int c = 0; c < 2; ++c) { wow[c].prepare ((int) (sr * 0.02)); dry[c].prepare ((int) (sr * 0.02)); lpA[c].reset(); lpB[c].reset(); hp[c].reset(); hiss[c] = 0.0f; crack[c] = 0.0f; }
        ph = 0.0; rng = 22222u; lastTone = -1.0f;
    }
    inline float rnd() { rng = rng * 1664525u + 1013904223u; return (float) (rng >> 8) / 8388608.0f - 1.0f; }
    inline void process (float& l, float& r, float toneHz, float noise, float wobble, float mix, double sr)
    {
        if (toneHz != lastTone) { lastTone = toneHz; for (int c = 0; c < 2; ++c) { lpA[c].lowpass (toneHz, 0.7071, sr); lpB[c].lowpass (toneHz, 0.7071, sr); hp[c].highpass (60.0, 0.7071, sr); } }
        ph += 0.5 / sr; ph -= std::floor (ph);
        const double d = (0.006 + wobble * 0.003 * (std::sin (2 * kPi * ph) + 0.3 * std::sin (2 * kPi * ph * 6.3))) * sr, dd = 0.006 * sr;
        float* io[2] = { &l, &r };
        for (int c = 0; c < 2; ++c)
        {
            const float x = *io[c]; wow[c].write (x); dry[c].write (x);
            float y = wow[c].read (d); const float dr = dry[c].read (dd); wow[c].advance (); dry[c].advance();
            y = lpB[c].process (lpA[c].process (hp[c].process (y)));
            hiss[c] += 0.4f * (rnd() - hiss[c]); y += hiss[c] * 0.02f * noise;
            if (std::abs (rnd()) > 1.0f - noise * 12.0f / (float) sr * 2.0f) crack[c] = rnd() * 0.3f * noise; crack[c] *= 0.92f; y += crack[c];
            *io[c] = dr * (1.0f - mix) + y * mix;
        }
    }
    DelayLine wow[2], dry[2]; Biquad lpA[2], lpB[2], hp[2]; float hiss[2] = { 0, 0 }, crack[2] = { 0, 0 }, lastTone = -1.0f; double ph = 0.0; uint32_t rng = 22222u;
};

// OTT-style three-band compressor that also lifts quiet material. depth blends between untouched (0) and full (1).
struct Ott
{
    void prepare (double sr)
    {
        for (int c = 0; c < 2; ++c) for (int i = 0; i < 2; ++i) { lp1[c][i].reset(); hp1[c][i].reset(); lp2[c][i].reset(); hp2[c][i].reset(); }
        for (auto& e : env) for (auto& v : e) v = 0.0f;
        for (int c = 0; c < 2; ++c) for (int i = 0; i < 2; ++i)
        { lp1[c][i].lowpass (88.0, 0.7071, sr); hp1[c][i].highpass (88.0, 0.7071, sr); lp2[c][i].lowpass (2500.0, 0.7071, sr); hp2[c][i].highpass (2500.0, 0.7071, sr); }
    }
    static inline float bandGainDb (float levelDb)
    {
        float g = 0.0f;
        if (levelDb > -22.0f) g = -(1.0f - 1.0f / 5.0f) * (levelDb + 22.0f);
        else if (levelDb < -32.0f) g = std::min (12.5f, (1.0f - 1.0f / 4.0f) * (-32.0f - levelDb));
        return g;
    }
    inline void process (float& l, float& r, float depth, float time, float mix, double sr)
    {
        const float att = std::exp (-1.0f / ((2.0f + 28.0f * time) * 0.001f * (float) sr)), rel = std::exp (-1.0f / ((30.0f + 270.0f * time) * 0.001f * (float) sr));
        float* io[2] = { &l, &r }; float out[2];
        for (int c = 0; c < 2; ++c)
        {
            const float x = *io[c];
            const float lo = lp1[c][1].process (lp1[c][0].process (x)), rest = hp1[c][1].process (hp1[c][0].process (x));
            const float mid = lp2[c][1].process (lp2[c][0].process (rest)), hi = hp2[c][1].process (hp2[c][0].process (rest));
            float bands[3] = { lo, mid, hi }, sum = 0.0f;
            for (int b = 0; b < 3; ++b)
            {
                const float a = std::abs (bands[b]); float& e = env[c][b]; e = a > e ? att * e + (1 - att) * a : rel * e + (1 - rel) * a;
                const float gDb = depth * bandGainDb (20.0f * std::log10 (e + 1e-9f));
                sum += bands[b] * std::pow (10.0f, gDb / 20.0f);
            }
            out[c] = sum;
        }
        l = l * (1.0f - mix) + out[0] * mix; r = r * (1.0f - mix) + out[1] * mix;
    }
    Biquad lp1[2][2], hp1[2][2], lp2[2][2], hp2[2][2]; float env[2][3];
};

} // namespace rf
