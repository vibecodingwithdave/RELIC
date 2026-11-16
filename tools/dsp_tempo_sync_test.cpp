// Tempo sync note values: exact checks on the beat table and the Hz / ms conversions (no JUCE).
#include "../Source/Dsp/Mod.h"
#include <cstdio>
using namespace rf;
static int passed = 0, total = 0;
static void check (const char* name, bool ok) { ++total; passed += ok; std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", name); }
static bool near (double a, double b, double tol = 1e-9) { return std::abs (a - b) <= tol; }
int main()
{
    check ("the original LFO list keeps its meaning (Free, 1/1, 1/2, 1/4, 1/8, 1/16 = 0, 4, 2, 1, 0.5, 0.25 beats)", lfoSyncBeats (0) == 0.0 && lfoSyncBeats (1) == 4.0 && lfoSyncBeats (2) == 2.0 && lfoSyncBeats (3) == 1.0 && lfoSyncBeats (4) == 0.5 && lfoSyncBeats (5) == 0.25);
    check ("1/32 is an eighth of a beat", near (lfoSyncBeats (6), 0.125));
    check ("triplets are two thirds of the plain value (1/2T, 1/4T, 1/8T, 1/16T)", near (lfoSyncBeats (7), 2.0 * 2 / 3) && near (lfoSyncBeats (8), 2.0 / 3) && near (lfoSyncBeats (9), 1.0 / 3) && near (lfoSyncBeats (10), 1.0 / 6));
    check ("dotted values are one and a half times the plain value (1/2D, 1/4D, 1/8D, 1/16D)", near (lfoSyncBeats (11), 3.0) && near (lfoSyncBeats (12), 1.5) && near (lfoSyncBeats (13), 0.75) && near (lfoSyncBeats (14), 0.375));
    check ("2/1 and 4/1 are two and four whole notes", lfoSyncBeats (15) == 8.0 && lfoSyncBeats (16) == 16.0);
    check ("out-of-range indexes are clamped", lfoSyncBeats (-3) == 0.0 && lfoSyncBeats (99) == 16.0);
    check ("Hz: 1/4 at 120 BPM is 2 Hz, 1/8 is 4 Hz, 1/1 is 0.5 Hz", near (syncedHz (3, 120, 7), 2.0) && near (syncedHz (4, 120, 7), 4.0) && near (syncedHz (1, 120, 7), 0.5));
    check ("Hz: Free returns the knob's own value", syncedHz (0, 120, 3.7) == 3.7);
    check ("Hz: follows the tempo (1/4 at 90 BPM is 1.5 Hz)", near (syncedHz (3, 90, 7), 1.5));
    check ("Hz: limited to 0.02 to 30 Hz (4/1 at 20 BPM; 1/32 at 300 BPM)", syncedHz (16, 20, 1) >= 0.02 && near (syncedHz (6, 300, 1), 30.0));
    check ("Hz: a silly tempo (0, negative) is held at 20 BPM", near (syncedHz (3, 0, 1), 20.0 / 60.0) && near (syncedHz (3, -5, 1), 20.0 / 60.0));
    check ("ms: 1/8 at 120 BPM is 250 ms, 1/4 is 500 ms, 1/8D is 375 ms", near (syncedMs (4, 120, 1), 250.0) && near (syncedMs (3, 120, 1), 500.0) && near (syncedMs (13, 120, 1), 375.0));
    check ("ms: 1/8T at 120 BPM is a third of 500 ms", near (syncedMs (9, 120, 1), 500.0 / 3.0, 1e-6));
    check ("ms: Free returns the knob's own value", syncedMs (0, 120, 321.0) == 321.0);
    check ("ms: limited to the delay lines' 2 seconds, and at least 1 ms", syncedMs (1, 60, 1) == 2000.0 && syncedMs (6, 10000, 1) == 1.0);
    std::printf ("%d of %d checks passed\n", passed, total);
    return passed == total ? 0 : 1;
}
