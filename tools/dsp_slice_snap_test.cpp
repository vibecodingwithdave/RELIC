#include "../Source/Dsp/SliceSnap.h"
#include <cstdio>
#include <vector>
static int fails = 0;
static void check (const char* n, bool ok) { std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", n); if (! ok) ++fails; }
int main()
{
    const int N = 44100; std::vector<float> L (N), R (N);
    for (int i = 0; i < N; ++i) { L[i] = std::sin (2.0 * M_PI * 100.0 * i / 44100.0 + 0.3); R[i] = L[i]; }     // 100 Hz: a period is 441 frames
    // rising crossings are where sin(2 pi 100 t + 0.3) goes from negative to non-negative: phase = 2 pi k -> t = (k - 0.3/2pi) * 441
    const double ph = 0.3 / (2.0 * M_PI) * 441.0;
    const double z = rf::snap::zeroCrossing (L.data(), R.data(), N, 10000.0, 300);
    check ("lands on a sample where the signal crosses zero upward", L[(int) z - 1] < 0.0f && L[(int) z] >= 0.0f);
    check ("and it is within half a period of the request", std::abs (z - 10000.0) <= 221.0);
    const double expect = std::round ((std::floor ((10000.0 + ph) / 441.0) * 441.0) - ph);
    check ("it is the nearest rising crossing", std::abs (z - 10000.0) <= std::abs (expect + 441.0 - 10000.0) + 1 && std::abs (z - 10000.0) <= std::abs (expect - 10000.0) + 1 + 441);
    std::vector<float> dc (N, 0.5f);
    check ("a signal with no crossing returns the requested position", rf::snap::zeroCrossing (dc.data(), nullptr, N, 5000.0, 400) == 5000.0);
    check ("search is limited to maxFrames", std::abs (rf::snap::zeroCrossing (L.data(), R.data(), N, 10000.0, 20) - 10000.0) <= 21.0);
    check ("positions near the ends are safe", rf::snap::zeroCrossing (L.data(), R.data(), N, -50.0, 100) >= 0.0 && rf::snap::zeroCrossing (L.data(), R.data(), N, 1e9, 100) < N);
    check ("a one-frame sample is returned unchanged", rf::snap::zeroCrossing (L.data(), R.data(), 1, 0.0, 10) == 0.0);
    // grid: 120 BPM, 1/4 note = 0.5 s; sample 4 s at 44100
    const int F = 4 * 44100;
    check ("grid: 0.13 of a 4 s sample is nearest to 0.125 (a 1/4 grid is 0.125 here)", std::abs (rf::snap::gridPoint (0.13, 1.0, 120.0, 44100.0, F) - 0.125) < 1e-9);
    check ("grid: 1/8 grid snaps 0.30 to 0.3125", std::abs (rf::snap::gridPoint (0.30, 0.5, 120.0, 44100.0, F) - 0.3125) < 1e-9);
    check ("grid: never past the end", rf::snap::gridPoint (0.999, 1.0, 120.0, 44100.0, F) <= 1.0);
    check ("grid: a slower tempo makes bigger steps", std::abs (rf::snap::gridPoint (0.3, 1.0, 60.0, 44100.0, F) - 0.25) < 1e-9);
    check ("grid: tempo is floored at 20 BPM", std::abs (rf::snap::gridPoint (0.2, 1.0, 1.0, 44100.0, F) - rf::snap::gridPoint (0.2, 1.0, 20.0, 44100.0, F)) < 1e-12);
    std::printf ("%s\n", fails ? "FAILED" : "all pass"); return fails ? 1 : 0;
}
