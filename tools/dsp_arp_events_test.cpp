// Arp EVENTS mode (arp_mode 9): each step is an event with its own notes (a mask), length and velocity. Exact event checks (no JUCE).
#include "../Source/NoteEngine.h"
#include <cstdio>
using namespace rf;
static int passed = 0, total = 0;
static void check (const char* name, bool ok) { ++total; passed += ok; std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", name); }
// 1 kHz "sample rate", 120 BPM, 1/8 steps = 250 samples; the first step lands at 25.
struct Rig
{
    NoteEngine e; ChordArpSettings s; int64_t t = 0;
    Rig() { s.arpOn = true; s.run = true; s.rate = 7; s.gate = 0.5f; s.mode = 9; s.evLength = 16; for (int i = 0; i < 64; ++i) { s.evMask[(size_t) i] = 0; s.evLen[(size_t) i] = 0; s.evVel[(size_t) i] = 100; } }
    void ev (int step, int mask, int len = 0, int vel = 100) { s.evMask[(size_t) step] = mask; s.evLen[(size_t) step] = len; s.evVel[(size_t) step] = vel; }
    std::vector<NoteEvt> block (int n, std::vector<NoteEvt> in = {})
    {
        for (auto& v : in) v.t += t;
        std::vector<NoteEvt> out; e.process (t, n, in, s, 120, -1, false, 1000, out); t += n; return out;
    }
};
static std::vector<NoteEvt> keys (std::vector<int> notes) { std::vector<NoteEvt> v; for (int n : notes) v.push_back ({ 0, true, n, 0.8f }); return v; }
static int count (const std::vector<NoteEvt>& o, bool on, int note, int64_t a = -1, int64_t b = INT64_MAX) { int c = 0; for (auto& e : o) if (e.on == on && e.note == note && e.t >= a && e.t < b) ++c; return c; }
static int64_t firstT (const std::vector<NoteEvt>& o, bool on, int note, int64_t after = -1) { for (auto& e : o) if (e.on == on && e.note == note && e.t > after) return e.t; return -1; }
int main()
{
    const std::vector<int> cEG = { 60, 64, 67 };
    {   Rig r; r.ev (0, 0b101); r.ev (1, 0); r.ev (2, 0b010); r.s.evLength = 4; auto o = r.block (250 * 8, keys (cEG));
        check ("a step with bits 0 and 2 sounds C and G together at the same time", count (o, true, 60, 0, 100) == 1 && count (o, true, 67, 0, 100) == 1 && firstT (o, true, 60) == firstT (o, true, 67));
        check ("the same step does not sound E", count (o, true, 64, 0, 200) == 0);
        check ("a rest (mask 0) plays nothing; step 2 plays E on the third step", count (o, true, 64, 400, 600) == 1 && count (o, true, 60, 250, 500) == 0); }
    {   Rig r; r.ev (0, 0b001, 4); r.ev (1, 0b010, 0); r.ev (2, 0b100, 1); r.s.evLength = 4; auto o = r.block (250 * 4, keys (cEG));
        const auto offC = firstT (o, false, 60), offE = firstT (o, false, 64), offG = firstT (o, false, 67);
        check ("length 4 holds the note across the next three steps (about 4 steps minus the gate)", offC > 25 + 3 * 250 && offC < 25 + 4 * 250);
        check ("length 0 uses the normal gate (half a step)", offE > 25 + 250 && offE <= 25 + 250 + 130);
        check ("length 1 also ends within its own step", offG > 25 + 500 && offG < 25 + 750);
        check ("a long event rings under the following events (C still on while E and G start)", firstT (o, true, 64) < offC && firstT (o, true, 67) < offC); }
    {   Rig r; r.ev (0, 0b001, 0, 127); r.ev (1, 0b001, 0, 40); r.s.evLength = 2; auto o = r.block (250 * 2 + 50, keys (cEG)); std::vector<float> v; for (auto& e : o) if (e.on) v.push_back (e.vel);
        check ("each event has its own velocity (127 then 40)", v.size() >= 2 && std::abs (v[0] - 1.0f) < 0.02f && std::abs (v[1] - 40.0f / 127.0f) < 0.02f); }
    {   Rig r; r.s.evLength = 32; for (int i = 0; i < 32; ++i) r.ev (i, i < 16 ? 0b001 : 0b100); auto o = r.block (250 * 40, keys (cEG)); int c1 = 0, g1 = 0, c2 = 0, g2 = 0;
        for (auto& e : o) if (e.on) { const int step = (int) ((e.t - 25) / 250); const bool bar2 = (step % 32) >= 16; if (e.note == 60) (bar2 ? c2 : c1)++; if (e.note == 67) (bar2 ? g2 : g1)++; }
        check ("a 32-step pattern: bar 1 only on C, bar 2 only on G, and it loops", c1 >= 16 && g2 >= 16 && c2 == 0 && g1 == 0); }
    {   Rig r; r.ev (0, 0b1001); auto o = r.block (250 * 2, keys ({ 60 }));   // one held note: number 3 is 3 octaves... number 0 and 3 are C and C three octaves up
        check ("with one held note, numbers go up in octaves (0 and 3 = C and C three octaves up)", count (o, true, 60, 0, 100) == 1 && count (o, true, 96, 0, 100) == 1); }
    {   Rig r; r.ev (0, 0b001); r.ev (1, 0b001); r.s.evLength = 2; r.s.rhythmOn = true; r.s.rhythm[0].type = 1; auto o = r.block (250 * 2 + 50, keys (cEG));
        check ("the rhythm lane is ignored in Events mode", count (o, true, 60) >= 3); }
    {   Rig r; r.ev (0, 0b011); auto o = r.block (250 * 3, keys (cEG)); r.s.mode = 8; r.s.patLength = 1; r.s.pattern[0] = 0; Rig q; q.s.mode = 8; q.s.patLength = 1; q.s.pattern[0] = 0; auto o2 = q.block (250 * 3, keys (cEG));
        check ("CUSTOM mode still plays one note per step (unchanged)", count (o2, true, 60) >= 3 && count (o2, true, 64) == 0); }
    {   Rig r; r.s.chance = 0.0f; r.ev (0, 1); auto o = r.block (250 * 3, keys (cEG)); check ("Chance 0 silences event steps too", count (o, true, 60) == 0); }
    std::printf ("%d/%d passed\n", passed, total); return passed == total ? 0 : 1;
}
