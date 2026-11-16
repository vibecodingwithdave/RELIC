// Arp CUSTOM mode (a hand-made pattern of up to 64 steps): exact event checks (no JUCE).
#include "../Source/NoteEngine.h"
#include <cstdio>
using namespace rf;
using V = std::vector<int>;
static int passed = 0, total = 0;
static void check (const char* name, bool ok) { ++total; passed += ok; std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", name); }
// 1 kHz "sample rate", 120 BPM, 1/8 steps = 250 samples; the first step lands at 25.
struct Rig
{
    NoteEngine e; ChordArpSettings s; int64_t t = 0;
    Rig() { s.arpOn = true; s.run = true; s.rate = 7; s.gate = 0.5f; s.mode = 8; }
    void pat (V p) { s.patLength = (int) p.size(); for (size_t i = 0; i < p.size(); ++i) s.pattern[i] = p[i]; }
    std::vector<NoteEvt> block (int n, std::vector<NoteEvt> in = {})
    {
        for (auto& v : in) v.t += t;
        std::vector<NoteEvt> out; e.process (t, n, in, s, 120, -1, false, 1000, out); t += n; return out;
    }
};
static std::vector<NoteEvt> keys (V notes, bool on = true) { std::vector<NoteEvt> v; for (int n : notes) v.push_back ({ 0, on, n, 0.8f }); return v; }
static V seq (const std::vector<NoteEvt>& out, size_t n) { V v; for (auto& e : out) if (e.on && v.size() < n) v.push_back (e.note); return v; }
static V run (V pattern, V held, size_t n, int order = 0) { Rig r; r.pat (pattern); r.s.order = order; return seq (r.block (250 * (int) n + 200, keys (held)), n); }

int main()
{
    const V cEG = { 60, 64, 67 };
    check ("pattern 0 1 2 1 plays C E G E and repeats", run ({ 0, 1, 2, 1 }, cEG, 8) == V { 60, 64, 67, 64, 60, 64, 67, 64 });
    check ("pattern 2 2 0 1 picks notes by number, in any order", run ({ 2, 2, 0, 1 }, cEG, 4) == V { 67, 67, 60, 64 });
    check ("numbers past the last held note go up an octave (3 4 5 6 = C E G above, then C two up)", run ({ 3, 4, 5, 6 }, cEG, 4) == V { 72, 76, 79, 84 });
    check ("negative numbers go down (-1 -2 -3 -4 = G E C below, then G two down)", run ({ -1, -2, -3, -4 }, cEG, 4) == V { 55, 52, 48, 43 });
    check ("with one held note every number is that note, octaves apart (0 1 -1 2)", run ({ 0, 1, -1, 2 }, { 60 }, 4) == V { 60, 72, 48, 84 });
    check ("a 1-step pattern repeats one note", run ({ 1 }, cEG, 5) == V { 64, 64, 64, 64, 64 });
    {   // multi-bar: 32 steps do not repeat inside the first 32
        V p; for (int i = 0; i < 32; ++i) p.push_back (i < 16 ? 0 : 2);
        auto v = run (p, cEG, 40);
        bool ok = v.size() == 40; for (int i = 0; i < 40 && ok; ++i) ok = v[(size_t) i] == (i % 32 < 16 ? 60 : 67);
        check ("a 32-step (two-bar) pattern: bar 1 on C, bar 2 on G, then it loops", ok);
    }
    {   // the longest pattern, 64 steps: every step distinct note numbers
        V p; for (int i = 0; i < 64; ++i) p.push_back (i % 3); auto v = run (p, cEG, 70);
        bool ok = v.size() == 70; for (int i = 0; i < 70 && ok; ++i) ok = v[(size_t) i] == cEG[(size_t) ((i % 64) % 3)];
        check ("a 64-step pattern runs to its end and wraps", ok);
    }
    check ("notes are ordered by pitch whatever the playing order", run ({ 0, 1, 2 }, { 67, 60, 64 }, 3) == V { 60, 64, 67 });
    check ("Order 'As Played' takes the held notes in the order they were pressed", run ({ 0, 1, 2 }, { 67, 60, 64 }, 3, 1) == V { 67, 60, 64 });
    check ("octave range is ignored in Custom (note numbers set the octave)", [&] { Rig r; r.pat ({ 0, 1, 2, 3 }); r.s.octaves = 3; return seq (r.block (1200, keys ({ 60, 64, 67 })), 4) == V { 60, 64, 67, 72 }; }());
    check ("notes never leave 0-127 (high note, big numbers)", [&] { auto v = run ({ 10, 11, 12 }, { 120 }, 3); for (int n : v) if (n < 0 || n > 127) return false; return true; }());
    {   // rhythm lane still applies: a rest keeps its place in the pattern clock of the rhythm lane
        Rig r; r.pat ({ 0, 1, 2 }); r.s.rhythmOn = true; r.s.rhythmLength = 2; r.s.rhythm[1].type = 1;
        check ("a rest in the rhythm lane skips that step and the pattern resumes", seq (r.block (4000, keys (cEG)), 6) == V { 60, 64, 67, 60, 64, 67 });
    }
    {   // timing as the other modes: 250-sample steps from 25
        Rig r; r.pat ({ 0, 1 }); auto out = r.block (3000, keys (cEG)); bool ok = true; int k = 0;
        for (auto& e : out) if (e.on) ok &= e.t == 25 + 250 * k++;
        check ("Custom keeps the chosen rate (1/8 = 250 samples)", ok && k >= 10);
    }
    {   // STOP: finishes the whole custom pattern, then waits
        Rig r; r.pat ({ 0, 1, 2, 1, 0 }); r.s.run = false; auto out = r.block (8000, keys (cEG)); int hits = 0; for (auto& e : out) hits += e.on;
        check ("STOP plays exactly one pass of a 5-step pattern, then waits", hits == 5 && r.e.isArpWaiting());
    }
    {   // other modes untouched by the new fields
        Rig r; r.s.mode = 0; r.pat ({ 2, 2, 2 }); check ("Up mode ignores the custom pattern", seq (r.block (1200, keys (cEG)), 3) == V { 60, 64, 67 });
    }
    std::printf ("\nDSP RESULT: %d of %d\n", passed, total);
    return passed == total ? 0 : 1;
}
