#include "../Source/Dsp/Mod.h"
#include <cstdio>
static int fails = 0;
static void check (const char* n, bool ok) { std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", n); if (! ok) ++fails; }
int main()
{
    const int N = rf::kLfoPts; float sine[N], tri[N], flat[N], sq[N];
    for (int k = 0; k < N; ++k) { sine[k] = 0.5f + 0.5f * std::sin (2.0f * 3.14159265f * k / N); tri[k] = 1.0f - std::abs (2.0f * k / N - 1.0f); flat[k] = 0.75f; sq[k] = k < N / 2 ? 1.0f : 0.0f; }
    double maxe = 0; for (int i = 0; i < 4000; ++i) { const double ph = i / 4000.0; maxe = std::max (maxe, std::abs ((double) rf::lfoCustomEval (sine, ph) - std::sin (2.0 * 3.14159265358979 * ph))); }
    check ("sine points reproduce a sine closely (error < 0.01)", maxe < 0.01);
    bool through = true; for (int k = 0; k < N; ++k) through = through && std::abs (rf::lfoCustomEval (tri, (double) k / N) - (tri[k] * 2.0f - 1.0f)) < 1e-5; check ("the curve passes exactly through every point", through);
    check ("a flat 0.75 gives +0.5 everywhere", std::abs (rf::lfoCustomEval (flat, 0.31) - 0.5f) < 1e-5 && std::abs (rf::lfoCustomEval (flat, 0.99) - 0.5f) < 1e-5);
    bool wrap = true; float prev = rf::lfoCustomEval (tri, 0.0); for (int i = 1; i <= 20000; ++i) { const float y = rf::lfoCustomEval (tri, i / 20000.0); wrap = wrap && std::abs (y - prev) < 0.02f; prev = y; } check ("continuous over the whole cycle, including across the wrap", wrap);
    check ("phase beyond 1 wraps", std::abs (rf::lfoCustomEval (tri, 1.25) - rf::lfoCustomEval (tri, 0.25)) < 1e-5);
    bool bnd = true; for (int i = 0; i < 2000; ++i) { const float y = rf::lfoCustomEval (sq, i / 2000.0); bnd = bnd && y >= -1.0f && y <= 1.0f; } check ("a square's overshoot is held inside -1..1", bnd);
    rf::ModLfo a, b; double sr = 48000; int zc = 0; float last = 0; for (int i = 0; i < 48000; ++i) { const float y = a.next (5.0, sr, rf::ModLfo::Custom, sine); if (i && last < 0 && y >= 0) ++zc; last = y; }
    check ("Custom runs at the LFO rate (5 Hz gives 5 cycles in a second)", zc == 5 || zc == 4);
    check ("Custom with no table is silent (zero)", b.next (5.0, sr, rf::ModLfo::Custom, nullptr) == 0.0f);
    rf::ModLfo c, d; bool same = true; for (int i = 0; i < 5000; ++i) { same = same && c.next (3.0, sr, rf::ModLfo::Sine) == d.next (3.0, sr, rf::ModLfo::Sine, sine); } check ("the old shapes ignore the table (unchanged)", same);
    std::printf ("%s\n", fails ? "FAILED" : "all pass"); return fails ? 1 : 0;
}
