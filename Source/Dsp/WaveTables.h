// Band-limited oscillator tables, built from the mathematical harmonic series.
// Original implementation: mip-mapped additive tables, one set per octave band,
// each containing only the harmonics that stay below Nyquist for that band.
#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <complex>
#include <memory>
#include <string>
#include <cstdint>
#include <cstring>

namespace rf {

class WaveTables
{
public:
    static constexpr int N = 2048;       // samples per table
    static constexpr int Bands = 11;     // octave bands starting at baseHz
    static constexpr double baseHz = 16.0;
    enum Shape { Saw = 0, Triangle = 1 };
    static constexpr int WtCount = 8;     // wavetable bank: eight original harmonic recipes, morphed by position

    // Harmonic recipe t, harmonic k (1-based): amplitude before normalisation.
    static double wtAmp (int t, int k)
    {
        const double fk = (double) k;
        switch (t)
        {
            case 0: return k == 1 ? 1.0 : 0.0;                                             // sine
            case 1: return 1.0 / fk;                                                       // saw
            case 2: return (k % 2) ? 1.0 / fk : 0.0;                                       // hollow (square-like)
            case 3: return (k % 2) ? ((((k - 1) / 2) % 2) ? -1.0 : 1.0) / (fk * fk) : 0.0;  // soft triangle
            case 4: return 1.0 / fk + 1.6 * std::exp (-((fk - 6.0) / 1.5) * ((fk - 6.0) / 1.5));   // reed: a vocal-ish bump near the 6th harmonic
            case 5: return (k == 1 || k == 2 || k == 3 || k == 4 || k == 6 || k == 8) ? 1.0 / std::sqrt (fk) : 0.0;   // organ
            case 6: return (k % 2 == 0) ? 1.0 / fk : (k == 1 ? 0.35 : 0.0);                  // even-heavy
            default: return (k % 3 == 1) ? 1.0 / std::sqrt (fk) : 0.12 / fk;                // glass
        }
    }

    void prepare (double sampleRate)
    {
        const double nyquist = sampleRate * 0.5;
        const double pi = 3.14159265358979323846;
        for (int s = 0; s < 2; ++s)
            for (int b = 0; b < Bands; ++b)
            {
                const double fmax = baseHz * std::pow (2.0, b + 1);
                int maxH = (int) std::floor (nyquist * 0.97 / fmax);
                maxH = std::max (1, std::min (maxH, N / 2 - 1));
                std::vector<double> w ((size_t) N, 0.0);
                for (int k = 1; k <= maxH; ++k)
                {
                    double amp;
                    if (s == Saw)       amp = (2.0 / pi) / k;                           // falling saw, +1 to -1
                    else if (k % 2 == 0) continue;                                      // triangle: odd harmonics only
                    else                amp = (8.0 / (pi * pi)) * (((k - 1) / 2) % 2 ? -1.0 : 1.0) / ((double) k * k);
                    for (int n = 0; n < N; ++n)
                        w[(size_t) n] += amp * std::sin (2.0 * pi * k * n / N);
                }
                auto& t = table[s][b];
                t.assign ((size_t) N + 3, 0.0f);
                for (int n = 0; n < N; ++n) t[(size_t) n + 1] = (float) w[(size_t) n];
                t[0] = (float) w[N - 1];
                t[(size_t) N + 1] = (float) w[0];
                t[(size_t) N + 2] = (float) w[1];
            }
        // wavetable bank: sine lookup keeps the build fast
        std::vector<double> sinT ((size_t) N); for (int n = 0; n < N; ++n) sinT[(size_t) n] = std::sin (2.0 * pi * n / N);
        for (int tb = 0; tb < WtCount; ++tb)
        {
            double scale = 1.0;
            for (int pass = 0; pass < 2; ++pass)
                for (int b = 0; b < Bands; ++b)
                {
                    if (pass == 0 && b != 4) continue;
                    const double fmax = baseHz * std::pow (2.0, b + 1);
                    int maxH = (int) std::floor (nyquist * 0.97 / fmax); maxH = std::max (1, std::min (maxH, N / 2 - 1));
                    std::vector<double> wv ((size_t) N, 0.0);
                    for (int k = 1; k <= maxH; ++k)
                    {
                        const double a = wtAmp (tb, k); if (a == 0.0) continue;
                        for (int n = 0; n < N; ++n) wv[(size_t) n] += a * sinT[(size_t) (((long long) k * n) & (N - 1))];
                    }
                    if (pass == 0) { double pk = 1e-9, ss = 0; for (double v : wv) { pk = std::max (pk, std::abs (v)); ss += v * v; } const double rms = std::sqrt (ss / (double) N); scale = std::min (0.5 / std::max (rms, 1e-9), 1.0 / pk); continue; }   // about as loud as the saw, never past full scale
                    auto& t = wt[tb][b]; t.assign ((size_t) N + 3, 0.0f);
                    for (int n = 0; n < N; ++n) t[(size_t) n + 1] = (float) (wv[(size_t) n] * scale);
                    t[0] = (float) (wv[N - 1] * scale); t[(size_t) N + 1] = (float) (wv[0] * scale); t[(size_t) N + 2] = (float) (wv[1] * scale);
                }
        }
    }

    inline float lookupWt (int t, int band, double phase) const
    {
        const float* tb = wt[t][band].data();
        const double x = phase * N; const int i = (int) x; const float f = (float) (x - i);
        const float y0 = tb[i], y1 = tb[i + 1], y2 = tb[i + 2], y3 = tb[i + 3];
        return y1 + 0.5f * f * (y2 - y0 + f * (2.f * y0 - 5.f * y1 + 4.f * y2 - y3 + f * (3.f * (y1 - y2) + y3 - y0)));
    }
    // position 0..1 sweeps through the bank
    inline float lookupMorph (double pos01, int band, double phase) const
    {
        const double x = std::min (1.0, std::max (0.0, pos01)) * (WtCount - 1); const int i = std::min (WtCount - 2, (int) x); const float f = (float) (x - i);
        return lookupWt (i, band, phase) * (1.0f - f) + lookupWt (i + 1, band, phase) * f;
    }


    // ---------------- external wavetables ----------------
    // A user bank is a run of single-cycle frames (the same layout most wavetable files use: 2048 samples a frame, frames end to end).
    // Every frame is analysed once and rebuilt per octave band with only the harmonics that stay below Nyquist, like the built-in bank.
    static constexpr int MaxUserFrames = 256;
    struct UserBank
    {
        int frames = 0, frameLen = 0; double sampleRate = 0; std::string name;
        std::vector<float> t;                                         // [frame][band][N + 3], with the guard points the cubic lookup reads
        const float* at (int f, int b) const { return t.data() + ((size_t) f * Bands + (size_t) b) * (size_t) (N + 3); }
    };

    // Frame length of a file: a hint from the file (Serum-style "<!>2048" marker) wins; otherwise 2048 when it divides the file,
    // then the other usual lengths, and a file that is not a whole number of frames is read as one single cycle.
    static int detectFrameLen (size_t length, int hint = 0)
    {
        if (length < 2) return 0;
        auto sane = [] (int v) { return v >= 32 && v <= 8192; };
        if (sane (hint) && length >= (size_t) hint) return hint;
        if (length < 2048) return (int) length;                        // shorter than a standard frame: one cycle
        for (int cand : { 2048, 1024, 4096, 512, 256, 8192, 128 })
            if (length % (size_t) cand == 0 && length >= (size_t) cand) return cand;
        return (int) std::min<size_t> (length, 8192);                 // one cycle (cut to 8192)
    }

    // Reads the frame length marker some wavetable files carry in a "clm " chunk (text such as "<!>2048 10000000 wavetable"). 0 when absent.
    static int parseFrameLenHint (const unsigned char* d, size_t n)
    {
        if (n < 12 || std::memcmp (d, "RIFF", 4) != 0) return 0;
        size_t pos = 12;
        while (pos + 8 <= n)
        {
            const uint32_t sz = (uint32_t) d[pos + 4] | ((uint32_t) d[pos + 5] << 8) | ((uint32_t) d[pos + 6] << 16) | ((uint32_t) d[pos + 7] << 24);
            if (std::memcmp (d + pos, "clm ", 4) == 0)
            {
                const size_t a = pos + 8, e = std::min (n, a + (size_t) sz);
                for (size_t i = a; i + 3 < e; ++i)
                    if (d[i] == '<' && d[i + 1] == '!' && d[i + 2] == '>')
                    {
                        int v = 0; size_t j = i + 3; while (j < e && d[j] >= '0' && d[j] <= '9' && v < 100000) v = v * 10 + (d[j++] - '0');
                        return v;
                    }
                return 0;
            }
            pos += 8 + (size_t) sz + (sz & 1u);
        }
        return 0;
    }

    static void fft (std::vector<std::complex<double>>& a, bool inverse)
    {
        const size_t n = a.size();
        for (size_t i = 1, j = 0; i < n; ++i) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (a[i], a[j]); }
        for (size_t len = 2; len <= n; len <<= 1)
        {
            const double ang = 2.0 * 3.14159265358979323846 / (double) len * (inverse ? 1.0 : -1.0);
            const std::complex<double> wl (std::cos (ang), std::sin (ang));
            for (size_t i = 0; i < n; i += len)
            {
                std::complex<double> w (1.0, 0.0);
                for (size_t k = 0; k < len / 2; ++k) { const auto u = a[i + k], v = a[i + k + len / 2] * w; a[i + k] = u + v; a[i + k + len / 2] = u - v; w *= wl; }
            }
        }
    }

    // Builds a bank from mono samples. frameLen of 0 means detect. Returns null when there is nothing usable.
    static std::shared_ptr<UserBank> buildUser (const std::vector<float>& mono, int frameLen, double sampleRate, const std::string& name = {})
    {
        if (frameLen <= 0) frameLen = detectFrameLen (mono.size());
        if (frameLen < 2 || mono.size() < (size_t) frameLen) return nullptr;
        const int frames = (int) std::min<size_t> ((size_t) MaxUserFrames, mono.size() / (size_t) frameLen);
        auto bank = std::make_shared<UserBank>(); bank->frames = frames; bank->frameLen = frameLen; bank->sampleRate = sampleRate; bank->name = name;
        bank->t.assign ((size_t) frames * Bands * (size_t) (N + 3), 0.0f);
        const double nyquist = sampleRate * 0.5;
        const bool pow2 = (frameLen & (frameLen - 1)) == 0;
        const int P = pow2 ? frameLen : N;                              // analysis size: odd lengths are first resampled to one table
        std::vector<std::complex<double>> spec ((size_t) P), out ((size_t) N);
        std::vector<double> frame ((size_t) P);
        const int Kmax = std::min (P / 2 - 1, N / 2 - 1);
        for (int f = 0; f < frames; ++f)
        {
            const float* src = mono.data() + (size_t) f * (size_t) frameLen;
            if (pow2) for (int n = 0; n < P; ++n) frame[(size_t) n] = src[n];
            else for (int n = 0; n < P; ++n)                            // Catmull-Rom across the cycle, wrapping
            {
                const double x = (double) n * frameLen / P; const int i = (int) x; const double u = x - i;
                const double y0 = src[(i + frameLen - 1) % frameLen], y1 = src[i % frameLen], y2 = src[(i + 1) % frameLen], y3 = src[(i + 2) % frameLen];
                frame[(size_t) n] = y1 + 0.5 * u * (y2 - y0 + u * (2 * y0 - 5 * y1 + 4 * y2 - y3 + u * (3 * (y1 - y2) + y3 - y0)));
            }
            for (int n = 0; n < P; ++n) spec[(size_t) n] = { frame[(size_t) n], 0.0 };
            fft (spec, false);
            for (int b = 0; b < Bands; ++b)
            {
                const double fmax = baseHz * std::pow (2.0, b + 1);
                int maxH = (int) std::floor (nyquist * 0.97 / fmax); maxH = std::max (1, std::min (maxH, Kmax));
                std::fill (out.begin(), out.end(), std::complex<double> (0.0, 0.0));
                const double sc = (double) N / (double) P;              // harmonic k of the file is harmonic k of the table
                for (int k = 1; k <= maxH; ++k) { out[(size_t) k] = spec[(size_t) k] * sc; out[(size_t) (N - k)] = std::conj (spec[(size_t) k]) * sc; }
                fft (out, true);
                float* t = bank->t.data() + ((size_t) f * Bands + (size_t) b) * (size_t) (N + 3);
                for (int n = 0; n < N; ++n) t[n + 1] = (float) (out[(size_t) n].real() / (double) N);
                t[0] = t[N]; t[N + 1] = t[1]; t[N + 2] = t[2];
            }
        }
        // one gain for the whole bank, so frames keep their relative levels: about as loud as the saw, never past full scale
        double pk = 1e-9, sumRms = 0; int nz = 0;
        for (int f = 0; f < frames; ++f)
        {
            const float* t = bank->at (f, 4); double ss = 0;
            for (int n = 1; n <= N; ++n) { pk = std::max (pk, (double) std::abs (t[n])); ss += (double) t[n] * t[n]; }
            const double rms = std::sqrt (ss / N); if (rms > 1e-6) { sumRms += rms; ++nz; }
        }
        double gain = nz > 0 ? std::min (0.5 / (sumRms / nz), 1.0 / pk) : 1.0;
        for (auto& v : bank->t) v = (float) (v * gain);
        return bank;
    }

    inline float lookupUser (const UserBank& u, int frame, int band, double phase) const
    {
        const float* tb = u.at (frame, band);
        const double x = phase * N; const int i = (int) x; const float f = (float) (x - i);
        const float y0 = tb[i], y1 = tb[i + 1], y2 = tb[i + 2], y3 = tb[i + 3];
        return y1 + 0.5f * f * (y2 - y0 + f * (2.f * y0 - 5.f * y1 + 4.f * y2 - y3 + f * (3.f * (y1 - y2) + y3 - y0)));
    }
    inline float lookupUserMorph (const UserBank& u, double pos01, int band, double phase) const
    {
        if (u.frames < 2) return lookupUser (u, 0, band, phase);
        const double x = std::min (1.0, std::max (0.0, pos01)) * (u.frames - 1); const int i = std::min (u.frames - 2, (int) x); const float f = (float) (x - i);
        return lookupUser (u, i, band, phase) * (1.0f - f) + lookupUser (u, i + 1, band, phase) * f;
    }

    static int bandFor (double hz)
    {
        if (hz <= baseHz) return 0;
        return std::min (Bands - 1, (int) std::floor (std::log2 (hz / baseHz)));
    }

    // phase in [0,1)
    inline float lookup (int shape, int band, double phase) const
    {
        const float* t = table[shape][band].data();
        const double x = phase * N;
        const int i = (int) x;
        const float f = (float) (x - i);
        const float y0 = t[i], y1 = t[i + 1], y2 = t[i + 2], y3 = t[i + 3];
        return y1 + 0.5f * f * (y2 - y0 + f * (2.f * y0 - 5.f * y1 + 4.f * y2 - y3 + f * (3.f * (y1 - y2) + y3 - y0)));
    }

private:
    std::vector<float> table[2][Bands];
    std::vector<float> wt[WtCount][Bands];
};

} // namespace rf
