// Arp CHANCE and SPRAY: exact event checks (no JUCE). Relic's own design, not a the reference match (the reference's knobs have no audible effect).
#include "../Source/NoteEngine.h"
#include <cstdio>
#include <set>
using namespace rf;
using V = std::vector<int>;
static int passed = 0, total = 0;
static void check (const char* name, bool ok) { ++total; passed += ok; std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", name); }

// 1 kHz "sample rate", 120 BPM, 1/16 steps = 125 samples; the first step lands at 25.
struct Rig
{
    NoteEngine e; ChordArpSettings s; int64_t t = 0;
    Rig() { s.arpOn = true; s.run = true; s.rate = 10; s.gate = 0.5f; }
    std::vector<NoteEvt> block (int n, std::vector<NoteEvt> in = {})
    {
        for (auto& v : in) v.t += t;
        std::vector<NoteEvt> out; e.process (t, n, in, s, 120, -1, false, 1000, out); t += n; return out;
    }
};
static std::vector<NoteEvt> keys (V notes, bool on = true) { std::vector<NoteEvt> v; for (int n : notes) v.push_back ({ 0, on, n, 0.8f }); return v; }
static std::vector<NoteEvt> ons (const std::vector<NoteEvt>& out, int lo = 0) { std::vector<NoteEvt> v; for (auto& e : out) if (e.on && e.note >= lo) v.push_back (e); return v; }
static bool balanced (const std::vector<NoteEvt>& out)
{
    std::array<int, 128> c {};
    for (auto& e : out) { c[(size_t) e.note] += e.on ? 1 : -1; if (c[(size_t) e.note] < 0) return false; }
    return std::all_of (c.begin(), c.end(), [] (int x) { return x == 0; });
}
static std::vector<NoteEvt> run (float chance, float spray, V held = { 60, 64, 67 }, int n = 20000, bool twoHand = false)
{
    Rig r; r.s.chance = chance; r.s.spray = spray; r.s.twoHand = twoHand; auto out = r.block (n, keys (held));
    auto rel = r.block (400, keys (held, false)); out.insert (out.end(), rel.begin(), rel.end()); return out;
}
static const int kSteps = 160;   // 20000 samples of 125-sample steps, first at 25

int main()
{
    check ("defaults: chance 1, spray 0 (old patches unchanged)", ChordArpSettings().chance == 1.0f && ChordArpSettings().spray == 0.0f);
    {   // chance 1 / spray 0 is the plain arp, C E G repeating on every step
        auto o = ons (run (1.0f, 0.0f)); bool ok = (int) o.size() == kSteps;
        for (size_t i = 0; ok && i < o.size(); ++i) ok = o[i].t == 25 + (int64_t) i * 125 && o[i].note == V { 60, 64, 67 }[i % 3];
        check ("chance 1, spray 0: every step plays the plain Up pattern", ok);
    }
    {
        auto out = run (0.0f, 0.0f);
        check ("chance 0: silent, nothing stuck", ons (out).empty() && balanced (out));
    }
    {   // chance 0.5: about half the steps play, each one keeps its place in the pattern
        auto out = run (0.5f, 0.0f); auto o = ons (out); bool place = true;
        for (auto& e : o) { const int step = (int) ((e.t - 25) / 125); place &= (e.t - 25) % 125 == 0 && e.note == V { 60, 64, 67 }[(size_t) (step % 3)]; }
        check ("chance 0.5: about half the steps play", o.size() > 55 && o.size() < 105);
        check ("chance 0.5: a dropped step keeps its place (each note is the one due on its step)", place);
        check ("chance 0.5: no stuck notes", balanced (out));
        const size_t n25 = ons (run (0.25f, 0.0f)).size(), n50 = o.size(), n75 = ons (run (0.75f, 0.0f)).size();
        check ("more chance, more hits (0.25 < 0.5 < 0.75 < 1)", n25 < n50 && n50 < n75 && n75 < (size_t) kSteps);
    }
    {   // spray 1: every note jumps an octave, both ways
        auto out = run (1.0f, 1.0f); auto o = ons (out); bool all = (int) o.size() == kSteps, up = false, down = false;
        for (auto& e : o) { const int base = V { 60, 64, 67 }[(size_t) (((e.t - 25) / 125) % 3)]; all &= e.note == base + 12 || e.note == base - 12; up |= e.note == base + 12; down |= e.note == base - 12; }
        check ("spray 1: every note is its pattern note an octave up or down", all);
        check ("spray 1: both directions occur", up && down);
        check ("spray 1: no stuck notes", balanced (out));
        auto h = ons (run (1.0f, 0.5f)); int moved = 0;
        for (auto& e : h) { const int base = V { 60, 64, 67 }[(size_t) (((e.t - 25) / 125) % 3)]; moved += e.note != base; }
        check ("spray 0.5: about half the notes jump", moved > 55 && moved < 105 && (int) h.size() == kSteps);
    }
    {   // octave jumps stay inside the MIDI range
        auto lo = ons (run (1.0f, 1.0f, { 2 })), hi = ons (run (1.0f, 1.0f, { 125 })); bool a = ! lo.empty(), b = ! hi.empty();
        for (auto& e : lo) a &= e.note == 14; for (auto& e : hi) b &= e.note == 113;
        check ("spray at the ends of the range flips the jump instead of leaving 0..127", a && b);
    }
    {   // both together: dropped steps are silent, the rest are sprayed
        auto out = run (0.5f, 1.0f); auto o = ons (out); bool ok = true;
        for (auto& e : o) { const int base = V { 60, 64, 67 }[(size_t) (((e.t - 25) / 125) % 3)]; ok &= e.note == base + 12 || e.note == base - 12; }
        check ("chance 0.5 + spray 1: about half play, all sprayed, balanced", o.size() > 55 && o.size() < 105 && ok && balanced (out));
    }
    {   // the settings are repeatable (fixed seed), and a different chance changes the result
        auto a = run (0.5f, 0.5f), b = run (0.5f, 0.5f); bool same = a.size() == b.size();
        for (size_t i = 0; same && i < a.size(); ++i) same = a[i].t == b[i].t && a[i].note == b[i].note && a[i].on == b[i].on;
        check ("same settings give the same events", same);
    }
    {   // STOP: a pass is still one cycle of steps, dropped or not
        Rig r; r.s.run = false; r.s.chance = 0.5f; auto out = r.block (2000, keys ({ 60, 64, 67 }));
        check ("STOP with chance 0.5: still ends after one pass of 3 steps", r.e.isArpWaiting() && ons (out).size() <= 3 && balanced (out));
    }
    {   // the rhythm lane still rules: a rest stays a rest, chance only thins the hits
        Rig r; r.s.rhythmOn = true; r.s.rhythmLength = 2; r.s.rhythm[1].type = 1; r.s.chance = 0.5f; auto out = r.block (20000, keys ({ 60, 64, 67 }));
        bool ok = true; for (auto& e : ons (out)) ok &= ((e.t - 25) / 125) % 2 == 0;
        check ("rhythm lane rests stay silent with chance on", ok && ! ons (out).empty());
    }
    {   // the 2-HAND low voice is neither dropped nor sprayed
        auto out = run (0.0f, 1.0f, { 60, 64, 67 }, 2100, true); auto lows = ons (out); V got; for (auto& e : lows) got.push_back (e.note);
        check ("2-HAND low voice ignores chance and spray (C3 E3 G3 C3 E3 every 500)", got == V { 48, 52, 55, 48, 52 });
    }
    std::printf ("\nDSP RESULT: %d of %d\n", passed, total);
    return passed == total ? 0 : 1;
}
