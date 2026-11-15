// Band-limited oscillator tables, built from the mathematical harmonic series.
// Original implementation: mip-mapped additive tables, one set per octave band,
// each containing only the harmonics that stay below Nyquist for that band.
#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

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
