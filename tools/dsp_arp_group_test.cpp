// Arp GROUPING in the plain modes: exact event checks (no JUCE). Relic's own design: each group restarts the pattern and is cut to its length.
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
    Rig() { s.arpOn = true; s.run = true; s.rate = 7; s.gate = 0.5f; }
    std::vector<NoteEvt> block (int n, std::vector<NoteEvt> in = {})
    {
        for (auto& v : in) v.t += t;
        std::vector<NoteEvt> out; e.process (t, n, in, s, 120, -1, false, 1000, out); t += n; return out;
    }
};
static std::vector<NoteEvt> keys (V notes, bool on = true) { std::vector<NoteEvt> v; for (int n : notes) v.push_back ({ 0, on, n, 0.8f }); return v; }
static V seq (const std::vector<NoteEvt>& out, size_t n) { V v; for (auto& e : out) if (e.on && v.size() < n) v.push_back (e.note); return v; }
static V repeat (V one, int times) { V r; for (int i = 0; i < times; ++i) r.insert (r.end(), one.begin(), one.end()); return r; }
static V cat (V a, const V& b) { a.insert (a.end(), b.begin(), b.end()); return a; }
static V run (int mode, int grouping, V held = { 60, 64, 67 }, int octaves = 1, size_t n = 32)
{
    Rig r; r.s.mode = mode; r.s.grouping = grouping; r.s.octaves = octaves; return seq (r.block (250 * (int) n + 200, keys (held)), n);
}
static const V C = { 60 }, E = { 64 }, G = { 67 };

int main()
{
    const V up3 = { 60, 64, 67 };
    {   // Up, C E G, 5-5-6: each group restarts at C
        V one = cat (cat (V { 60, 64, 67, 60, 64 }, V { 60, 64, 67, 60, 64 }), V { 60, 64, 67, 60, 64, 67 });
        check ("Up 5-5-6: every group restarts at C (C E G C E | C E G C E | C E G C E G)", run (0, 1) == cat (one, one));
        V a = repeat ({ 60, 64, 67 }, 4); a = cat (a, { 60, 64, 67, 60 });   // 3-3-3-3-4: C E G x4, then C E G C
        check ("Up 3-3-3-3-4: C E G four times, then C E G C", run (0, 3, up3, 1, 16) == V { 60, 64, 67, 60, 64, 67, 60, 64, 67, 60, 64, 67, 60, 64, 67, 60 });
        check ("Up 4-4-4-4: C E G C each group", run (0, 4, up3, 1, 16) == repeat ({ 60, 64, 67, 60 }, 4));
        check ("Up 7-7-2: C E G C E G C twice, then C E", run (0, 2, up3, 1, 16) == V { 60, 64, 67, 60, 64, 67, 60, 60, 64, 67, 60, 64, 67, 60, 60, 64 });
    }
    {   // Down, Up-Down
        check ("Down 4-4-4-4: G E C G each group", run (1, 4, up3, 1, 16) == repeat ({ 67, 64, 60, 67 }, 4));
        check ("Up-Down 5-5-6: each group restarts the up-and-back (C E G E C)", run (2, 1, up3, 1, 16) == V { 60, 64, 67, 64, 60, 60, 64, 67, 64, 60, 60, 64, 67, 64, 60, 64 });
        check ("Down-Up 4-4-4-4: G E C E each group", run (3, 4, up3, 1, 16) == repeat ({ 67, 64, 60, 64 }, 4));
    }
    {   // octaves: a group never reaches past its length
        check ("Up, 2 octaves, 5-5-6: groups cut the 6-note line at 5 (C E G C' E')", run (0, 1, up3, 2, 10) == V { 60, 64, 67, 72, 76, 60, 64, 67, 72, 76 });
        check ("Up, 2 octaves, 3-3-3-3-4: only the lower octave is ever reached", run (0, 3, up3, 2, 8) == V { 60, 64, 67, 60, 64, 67, 60, 64 });
    }
    {   // the bar repeats every 16 steps; the step stays at the chosen rate
        auto v = run (0, 1, up3, 1, 40);
        check ("the 16-step bar repeats", v.size() == 40 && std::equal (v.begin(), v.begin() + 16, v.begin() + 16) && std::equal (v.begin(), v.begin() + 8, v.begin() + 32));
        Rig r; r.s.grouping = 1; auto out = r.block (3000, keys (up3)); bool ok = true; int k = 0;
        for (auto& e : out) if (e.on) ok &= e.t == 25 + 250 * k++;
        check ("grouping keeps the chosen rate in plain modes (1/8 = 250 samples)", ok && k >= 10);
    }
    {   // unchanged cases
        check ("grouping Pattern: plain Up is untouched", run (0, 0, up3, 1, 9) == repeat (up3, 3));
        Rig r; r.s.mode = 6; r.s.grouping = 1; auto out = r.block (1200, keys (up3)); int hits = 0; for (auto& e : out) hits += e.on && e.note == 60;
        check ("Chord mode ignores grouping (every step is the whole chord)", hits >= 4 && seq (out, 3) == V { 60, 64, 67 });
        Rig q; q.s.mode = 7; q.s.grouping = 1; auto w = seq (q.block (9000, keys (up3)), 16);
        Rig q2; q2.s.mode = 7; q2.s.grouping = 1; q2.s.rate = 10; auto w2 = seq (q2.block (9000, keys (up3)), 16);
        check ("Waterfall grouping is untouched (still locks to 1/16)", w == w2 && w.size() == 16);
    }
    {   // STOP: a grouped pass is one 16-step bar
        Rig r; r.s.run = false; r.s.grouping = 1; auto out = r.block (8000, keys (up3)); int hits = 0; for (auto& e : out) hits += e.on;
        check ("STOP with a grouping: exactly one 16-step bar, then waits", hits == 16 && r.e.isArpWaiting());
        Rig q; q.s.run = false; auto o2 = q.block (8000, keys (up3)); int h2 = 0; for (auto& e : o2) h2 += e.on;
        check ("STOP without a grouping: still one pass of the notes", h2 == 3 && q.e.isArpWaiting());
    }
    {   // rests keep their place inside a group
        Rig r; r.s.grouping = 4; r.s.rhythmOn = true; r.s.rhythmLength = 2; r.s.rhythm[1].type = 1; auto v = seq (r.block (9000, keys (up3)), 8);
        check ("rhythm lane rests do not skip notes in a group (C E G C then restart)", v == V { 60, 64, 67, 60, 60, 64, 67, 60 });
    }
    std::printf ("\nDSP RESULT: %d of %d\n", passed, total);
    return passed == total ? 0 : 1;
}
