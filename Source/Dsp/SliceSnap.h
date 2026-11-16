#pragma once
#include <cmath>
#include <algorithm>
#include <cstddef>

// Where a hand-placed slice point lands when it is snapped.
namespace rf::snap
{
    // The nearest rising zero crossing (the sample before is below zero, this one is at or above) within maxFrames of pos (a frame index) in the mix of L and R.
    // Falls back to the nearest crossing of either direction, then to pos itself. Returns a frame index.
    inline double zeroCrossing (const float* L, const float* R, int n, double pos, int maxFrames)
    {
        if (n < 2) return pos;
        pos = std::max (0.0, std::min ((double) (n - 1), pos));
        const int c = std::max (1, std::min (n - 1, (int) std::lround (pos)));
        auto m = [&] (int i) { return L[i] + (R != nullptr ? R[i] : 0.0f); };
        int bestUp = -1, bestAny = -1;
        for (int d = 0; d <= maxFrames; ++d)
            for (int sgn = -1; sgn <= 1; sgn += 2)
            {
                const int i = c + sgn * d; if (i < 1 || i >= n) continue;
                const float a = m (i - 1), b = m (i);
                if (bestUp < 0 && a < 0.0f && b >= 0.0f) bestUp = i;
                if (bestAny < 0 && ((a < 0.0f && b >= 0.0f) || (a > 0.0f && b <= 0.0f))) bestAny = i;
                if (d == 0) break;
            }
        // prefer a rising crossing unless a falling one is much closer
        if (bestUp >= 0 && (bestAny < 0 || std::abs (bestUp - c) <= 2 * std::abs (bestAny - c) + 4)) return (double) bestUp;
        return bestAny >= 0 ? (double) bestAny : pos;
    }

    // The nearest multiple of one grid step (in beats) from the start of the sample, as a 0..1 position. bpm is floored at 20.
    inline double gridPoint (double pos01, double beats, double bpm, double rate, int frames)
    {
        const double step = beats * 60.0 / std::max (20.0, bpm) * rate / (double) std::max (1, frames);
        if (step <= 1e-9) return pos01;
        return std::min (1.0, std::round (pos01 / step) * step);
    }
}
