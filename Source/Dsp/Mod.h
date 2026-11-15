// Low-frequency oscillator for the modulation matrix: bipolar output, -1 to +1.
#pragma once
#include <cmath>
#include <cstdint>

namespace rf {

struct ModLfo
{
    enum Shape { Sine = 0, Triangle, Saw, Square, Random };
    double phase = 0.0; float held = 0.0f; uint32_t rng = 0x1234567u;
    void reset() { phase = 0.0; held = 0.0f; rng = 0x1234567u; }
    inline float next (double hz, double sr, int shape)
    {
        const double prev = phase;
        phase += hz / sr;
        if (phase >= 1.0) phase -= std::floor (phase);
        if (phase < prev || shape == Random && held == 0.0f)           // wrapped: new random value
        { rng = rng * 1664525u + 1013904223u; held = (float) ((rng >> 8) / 8388608.0 - 1.0); if (held == 0.0f) held = 1e-4f; }
        switch (shape)
        {
            case Triangle: return (float) (phase < 0.5 ? 4.0 * phase - 1.0 : 3.0 - 4.0 * phase);
            case Saw:      return (float) (2.0 * phase - 1.0);
            case Square:   return phase < 0.5 ? 1.0f : -1.0f;
            case Random:   return held;
            default:       return (float) std::sin (2.0 * 3.14159265358979323846 * phase);
        }
    }
};

// Beats per LFO cycle for the sync choices (index 0 = free running).
inline double lfoSyncBeats (int idx)
{
    static const double b[6] = { 0.0, 4.0, 2.0, 1.0, 0.5, 0.25 };
    return b[idx < 0 ? 0 : (idx > 5 ? 5 : idx)];
}

} // namespace rf
