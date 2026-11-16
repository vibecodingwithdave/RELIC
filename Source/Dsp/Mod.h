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

// Bar lock: the LFO phase (0..1, as the effects keep it just before their next step) that makes a cycle of `beats` beats line up with the host's position
// in quarter notes (ppq) at the first sample of a block. Zero ppq is the start of a cycle.
inline double barLockedPhase (double ppq, double beats, double hz, double sr)
{
    const double x = ppq / beats; double ph = x - std::floor (x) - hz / sr; return ph - std::floor (ph);
}

// A drawn modulation curve: kCurvePts values (0..1) at equal steps of the source's size (0 to 1, the sign is kept by the caller), joined smoothly (Catmull-Rom) and held within 0..1.
constexpr int kCurvePts = 9;
inline float curveEval (const float* pts, float x)
{
    x = x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); const float t = x * (float) (kCurvePts - 1); int i = (int) t; if (i > kCurvePts - 2) i = kCurvePts - 2; const float f = t - (float) i;
    const float y0 = i > 0 ? pts[i - 1] : 2.0f * pts[0] - pts[1], y1 = pts[i], y2 = pts[i + 1], y3 = i + 2 < kCurvePts ? pts[i + 2] : 2.0f * pts[kCurvePts - 1] - pts[kCurvePts - 2];       // the ends continue the line, so straight points stay straight
    const float y = y1 + 0.5f * f * (y2 - y0 + f * (2.0f * y0 - 5.0f * y1 + 4.0f * y2 - y3 + f * (3.0f * (y1 - y2) + y3 - y0)));
    return y < 0.0f ? 0.0f : (y > 1.0f ? 1.0f : y);
}

} // namespace rf
