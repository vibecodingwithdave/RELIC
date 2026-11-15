// Further effects: pitch delay, talk box (vowel formants), spectral gate. Original implementations.
#pragma once
#include "Effects.h"
#include <complex>

namespace rf {

// Granular-style pitch shifter: two crossfaded read heads sweeping through a short delay line.
struct PitchShifter
{
    void prepare (double sr) { winS = (float) (sr * 0.04); size = (int) (winS * 2.0f) + 8; buf.assign ((size_t) size, 0.0f); pos = 0; phase = 0.0f; }
    inline float process (float x, float ratio)
    {
        buf[(size_t) pos] = x;
        phase += (1.0f - ratio) / winS; phase -= std::floor (phase);
        float p2 = phase + 0.5f; if (p2 >= 1.0f) p2 -= 1.0f;
        const float y = tap (phase * winS) * std::sin ((float) kPi * phase) + tap (p2 * winS) * std::sin ((float) kPi * p2);
        if (++pos >= size) pos = 0;
        return y;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); phase = 0.0f; }
private:
    inline float tap (float d) const
    {
        float rp = (float) pos - d; if (rp < 0) rp += (float) size;
        const int i0 = (int) rp; const float f = rp - (float) i0; const int i1 = i0 + 1 >= size ? 0 : i0 + 1;
        return buf[(size_t) i0] * (1.0f - f) + buf[(size_t) i1] * f;
    }
    std::vector<float> buf; int size = 1, pos = 0; float winS = 1000.0f, phase = 0.0f;
};

// Delay whose echoes are pitch-shifted by a fixed interval each time round (feedback runs through the shifter).
struct PitchDelay
{
    void prepare (double sr) { size = (int) (sr * 1.6) + 8; bl.assign ((size_t) size, 0.0f); br.assign ((size_t) size, 0.0f); pos = 0; sl.prepare (sr); sr_.prepare (sr); }
    inline void process (float& l, float& r, float timeMs, float feedback, int semis, float mix, double sr)
    {
        const double d = std::min ((double) (size - 4), std::max (1.0, timeMs * 0.001 * sr));
        float ol = read (bl, d), orr = read (br, d);
        const float ratio = std::exp2 ((float) semis / 12.0f);
        if (semis != 0) { ol = sl.process (ol, ratio); orr = sr_.process (orr, ratio); }     // every echo, the first included, is shifted again
        bl[(size_t) pos] = l + feedback * ol; br[(size_t) pos] = r + feedback * orr;
        if (++pos >= size) pos = 0;
        l = l * (1.0f - mix * 0.5f) + ol * mix; r = r * (1.0f - mix * 0.5f) + orr * mix;
    }
private:
    inline float read (const std::vector<float>& b, double delay) const
    {
        double rp = pos - delay; if (rp < 0) rp += size;
        const int i0 = (int) rp; const float f = (float) (rp - i0); const int i1 = i0 + 1 >= size ? 0 : i0 + 1;
        return b[(size_t) i0] * (1.0f - f) + b[(size_t) i1] * f;
    }
    std::vector<float> bl, br; int size = 1, pos = 0; PitchShifter sl, sr_;
};

// Three parallel band-pass formant filters whose centres morph between the vowels A E I O U.
struct TalkBox
{
    void prepare (double) { for (auto& s : f) for (auto& c : s) c.reset(); n = 0; }
    inline void process (float& l, float& r, float vowel, float reso, float rateHz, float depth, float mix, double sr)
    {
        static const float F[5][3] = { { 800, 1200, 2500 }, { 400, 2000, 2600 }, { 300, 2300, 3000 }, { 500, 900, 2400 }, { 350, 700, 2400 } };   // A E I O U
        if ((n++ & 15) == 0)
        {
            float pos = vowel;
            if (rateHz > 0.0f) { ph += rateHz * 16.0f / (float) sr; ph -= std::floor (ph); pos += depth * 0.5f * std::sin (2.0f * (float) kPi * ph); }
            pos = std::min (1.0f, std::max (0.0f, pos)) * 4.0f; const int i = std::min (3, (int) pos); const float fr = pos - (float) i;
            const float q = 3.0f + reso * 14.0f;
            for (int k = 0; k < 3; ++k) { const float hz = F[i][k] * (1.0f - fr) + F[i + 1][k] * fr; for (int c = 0; c < 2; ++c) f[c][k].setup (hz, q, sr); }
        }
        static const float W[3] = { 1.0f, 0.8f, 0.55f };
        float o[2] = { 0, 0 }, in[2] = { l, r };
        for (int c = 0; c < 2; ++c) for (int k = 0; k < 3; ++k) o[c] += f[c][k].process (in[c], Svf::BandPass) * W[k];
        l = l * (1.0f - mix) + o[0] * 2.4f * mix; r = r * (1.0f - mix) + o[1] * 2.4f * mix;
    }
private:
    Svf f[2][3]; int n = 0; float ph = 0.0f;
};

// Per-bin noise gate: STFT (1024 points, 4x overlap), bins quieter than the threshold are turned down.
struct SpectralGate
{
    static constexpr int N = 1024, Hop = 256, Bins = N / 2 + 1;
    void prepare (double)
    {
        for (int i = 0; i < N; ++i) win[i] = std::sin ((float) kPi * ((float) i + 0.5f) / (float) N);
        for (int k = 0; k < N / 2; ++k) { cs[k] = std::cos (-2.0 * kPi * k / N); sn[k] = std::sin (-2.0 * kPi * k / N); }
        for (int i = 0; i < N; ++i) { int r = 0, x = i; for (int b = 0; (1 << b) < N; ++b) { r = (r << 1) | (x & 1); x >>= 1; } rev[i] = r; }
        for (auto& c : ch) { std::fill (std::begin (c.in), std::end (c.in), 0.0f); std::fill (std::begin (c.out), std::end (c.out), 0.0f); std::fill (std::begin (c.g), std::end (c.g), 1.0f); c.w = 0; c.cnt = 0; }
    }
    static constexpr int latency() { return N; }
    // thresholdDb is relative to the loudest bin in the frame; depth 0..1 maps to 0..-60 dB; release in ms
    inline void process (float& l, float& r, float thresholdDb, float depth, float releaseMs, float mix, double sr)
    {
        float in[2] = { l, r };
        for (int c = 0; c < 2; ++c)
        {
            auto& s = ch[c]; const float dry = s.in[s.w]; s.in[s.w] = in[c];     // dry = input from N samples ago
            float y = s.out[s.w]; s.out[s.w] = 0.0f;
            if (++s.w >= N) s.w = 0;
            if (++s.cnt >= Hop) { s.cnt = 0; frame (s, thresholdDb, depth, releaseMs, sr); }
            in[c] = dry * (1.0f - mix) + y * mix;
        }
        l = in[0]; r = in[1];
    }
private:
    struct Ch { float in[N], out[N], g[Bins]; int w = 0, cnt = 0; };
    void fft (std::complex<float>* x, bool inverse) const
    {
        std::complex<float> t[N]; for (int i = 0; i < N; ++i) t[rev[i]] = x[i];
        for (int len = 2; len <= N; len <<= 1)
        {
            const int step = N / len;
            for (int i = 0; i < N; i += len) for (int j = 0; j < len / 2; ++j)
            {
                const std::complex<float> w (cs[j * step], inverse ? -sn[j * step] : sn[j * step]);
                const auto u = t[i + j], v = t[i + j + len / 2] * w; t[i + j] = u + v; t[i + j + len / 2] = u - v;
            }
        }
        for (int i = 0; i < N; ++i) x[i] = t[i];
    }
    void frame (Ch& s, float thresholdDb, float depth, float releaseMs, double sr)
    {
        std::complex<float> x[N];
        for (int i = 0; i < N; ++i) x[i] = s.in[(s.w + i) % N] * win[i];      // the oldest N samples, w is the next write slot
        fft (x, false);
        float peak = 1.0e-9f; for (int k = 0; k < Bins; ++k) peak = std::max (peak, std::abs (x[k]));
        const float thr = peak * std::pow (10.0f, thresholdDb / 20.0f), floorG = std::pow (10.0f, -60.0f * depth / 20.0f);
        const float rel = std::exp (-(float) Hop / (std::max (1.0f, releaseMs) * 0.001f * (float) sr));
        for (int k = 0; k < Bins; ++k)
        {
            const float target = std::abs (x[k]) >= thr ? 1.0f : floorG;
            s.g[k] = target > s.g[k] ? target : target + (s.g[k] - target) * rel;        // opens at once, closes with the release
            x[k] *= s.g[k]; if (k > 0 && k < N / 2) x[N - k] = std::conj (x[k]);
        }
        x[0] = x[0].real(); x[N / 2] = x[N / 2].real();
        fft (x, true);
        for (int i = 0; i < N; ++i) s.out[(s.w + i) % N] += x[i].real() / (float) N * win[i] * 0.5f;
    }
    float win[N], cs[N / 2], sn[N / 2]; int rev[N]; Ch ch[2];
};

} // namespace rf
