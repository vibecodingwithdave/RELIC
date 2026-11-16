// Low-frequency oscillator for the modulation matrix: bipolar output, -1 to +1.
#pragma once
#include <cmath>
#include <algorithm>
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
    // Free, 1/1, 1/2, 1/4, 1/8, 1/16, 1/32, triplets 1/2T..1/16T, dotted 1/2D..1/16D, 2/1, 4/1. The first six are the original LFO list, so saved indexes keep their meaning.
    static const double b[17] = { 0.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 4.0 / 3.0, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0, 3.0, 1.5, 0.75, 0.375, 8.0, 16.0 };
    return b[idx < 0 ? 0 : (idx > 16 ? 16 : idx)];
}

// Rate in Hz of a cycle that lasts the given note value (0 = free: the caller keeps its own rate), limited to a range the effects can use.
inline double syncedHz (int idx, double bpm, double freeHz)
{
    const double b = lfoSyncBeats (idx);
    if (b <= 0.0) return freeHz;
    const double hz = std::max (20.0, bpm) / 60.0 / b; return hz < 0.02 ? 0.02 : (hz > 30.0 ? 30.0 : hz);
}
// Time in milliseconds of the given note value (0 = free), limited to the delay lines' two seconds.
inline double syncedMs (int idx, double bpm, double freeMs)
{
    const double b = lfoSyncBeats (idx);
    if (b <= 0.0) return freeMs;
    const double ms = b * 60000.0 / std::max (20.0, bpm); return ms < 1.0 ? 1.0 : (ms > 2000.0 ? 2000.0 : ms);
}

} // namespace rf
