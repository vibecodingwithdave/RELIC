#include "../Source/Dsp/Mod.h"
#include <cstdio>
static int fails = 0;
static void check (const char* n, bool ok) { std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", n); if (! ok) ++fails; }
int main()
{
    float id[9]; for (int j = 0; j < 9; ++j) id[j] = j / 8.0f;
    bool lin = true; for (int i = 0; i <= 200; ++i) lin = lin && std::abs (rf::curveEval (id, i / 200.0f) - i / 200.0f) < 2e-6;
    check ("the default points (a straight line) give the identity", lin);
    float p[9] = { 0.0f, 0.9f, 0.1f, 0.7f, 0.3f, 0.5f, 0.2f, 1.0f, 0.4f };
    bool through = true; for (int j = 0; j < 9; ++j) through = through && std::abs (rf::curveEval (p, j / 8.0f) - p[j]) < 1e-6;
    check ("the curve passes exactly through every point", through);
    bool cont = true; float prev = rf::curveEval (p, 0.0f); for (int i = 1; i <= 8000; ++i) { const float y = rf::curveEval (p, i / 8000.0f); cont = cont && std::abs (y - prev) < 0.01f; prev = y; }
    check ("it is continuous (no jumps between points)", cont);
    bool bounded = true; for (int i = 0; i <= 1000; ++i) { const float y = rf::curveEval (p, i / 1000.0f); bounded = bounded && y >= 0.0f && y <= 1.0f; }
    check ("overshoot between jagged points is held inside 0..1", bounded);
    check ("inputs outside 0..1 are clamped", std::abs (rf::curveEval (p, -2.0f) - p[0]) < 1e-5 && std::abs (rf::curveEval (p, 5.0f) - p[8]) < 1e-5);
    float flat[9]; for (auto& v : flat) v = 0.5f; check ("a flat curve is flat", std::abs (rf::curveEval (flat, 0.37f) - 0.5f) < 1e-6);
    float sl[9]; for (int j = 0; j < 9; ++j) sl[j] = 1.0f - j / 8.0f; check ("a falling line stays a falling line", std::abs (rf::curveEval (sl, 0.25f) - 0.75f) < 1e-6);
    float st[9] = { 0, 0, 0, 0, 1, 1, 1, 1, 1 }; check ("a step stays within 0..1 and is monotone", [&] { float prev = 0; for (int i = 0; i <= 400; ++i) { const float y = rf::curveEval (st, i / 400.0f); if (y < prev - 1e-6f) return false; prev = y; } return true; }());
    std::printf ("%s\n", fails ? "FAILED" : "all pass"); return fails ? 1 : 0;
}
