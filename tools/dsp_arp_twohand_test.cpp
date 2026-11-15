// 2-HAND low voice: exact event checks (no JUCE). Behaviour measured on a reference instrument, see a local probe script
#include "../Source/NoteEngine.h"
#include <cstdio>
using namespace rf;
using V = std::vector<int>;
static int passed = 0, total = 0;
static void check (const char* name, bool ok) { ++total; passed += ok; std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", name); }

// 1 kHz "sample rate" and 120 BPM: a 1/8 step is 250 samples, a 1/16 step 125. The first step lands 25 samples in.
struct Rig
{
    NoteEngine e; ChordArpSettings s; int64_t t = 0;
    Rig() { s.arpOn = true; s.run = true; s.rate = 7; s.gate = 0.5f; s.twoHand = true; }
    std::vector<NoteEvt> block (int n, std::vector<NoteEvt> in = {})
    {
        for (auto& v : in) v.t += t;
        std::vector<NoteEvt> out; e.process (t, n, in, s, 120, -1, false, 1000, out); t += n; return out;
    }
};
static std::vector<NoteEvt> keys (V notes, bool on = true) { std::vector<NoteEvt> v; for (int n : notes) v.push_back ({ 0, on, n, 0.8f }); return v; }
static std::vector<NoteEvt> lowEvents (const std::vector<NoteEvt>& out, bool on) { std::vector<NoteEvt> v; for (auto& e : out) if (e.on == on && e.note < 60) v.push_back (e); return v; }
static V notesOf (const std::vector<NoteEvt>& v) { V r; for (auto& e : v) r.push_back (e.note); return r; }
static V arpLine (const std::vector<NoteEvt>& out) { V r; for (auto& e : out) if (e.on && e.note >= 60) r.push_back (e.note); return r; }
static bool balanced (const std::vector<NoteEvt>& out)
{
    std::array<int, 128> c {};
    for (auto& e : out) { c[(size_t) e.note] += e.on ? 1 : -1; if (c[(size_t) e.note] < 0) return false; }
    return std::all_of (c.begin(), c.end(), [] (int x) { return x == 0; });
}
static bool spacedBy (const std::vector<NoteEvt>& v, int64_t first, int64_t gap)
{
    for (size_t i = 0; i < v.size(); ++i) if (v[i].t != first + (int64_t) i * gap) return false;
    return ! v.empty();
}

int main()
{
    check ("2-HAND defaults to off (old patches unchanged)", ! ChordArpSettings().twoHand);
    {   // off: nothing below the chord
        Rig r; r.s.twoHand = false; auto out = r.block (4200, keys ({ 60, 64, 67 }));
        check ("off: no low voice", lowEvents (out, true).empty());
    }
    {   // on, Up, C E G: C3 E3 G3 C3 on every 4th step (1000 samples), first on step 0
        Rig on; auto o = on.block (4000, keys ({ 60, 64, 67 })); Rig off; off.s.twoHand = false; auto f = off.block (4000, keys ({ 60, 64, 67 }));
        auto lows = lowEvents (o, true);
        check ("C E G: low voice walks C3 E3 G3 then wraps to C3", notesOf (lows) == V { 48, 52, 55, 48 });
        check ("one hit every 4 steps, starting on the first step", spacedBy (lows, 25, 1000));
        check ("arp line is untouched by 2-HAND", arpLine (o) == arpLine (f) && ! arpLine (o).empty());
        auto offs = lowEvents (o, false);
        check ("low note is held 95% of the 4 steps", offs.size() >= 3 && offs[0].t == 25 + 950 && offs[1].t == 1025 + 950 && offs[2].t == 2025 + 950);
        check ("low voice is about 3 dB under the arp note", std::abs (lows[0].vel - 0.8f * 0.72f) < 1e-4f);
    }
    {   // order follows the chord tones low to high, whatever the arp mode
        const int modes[] = { 1, 2, 3, 5, 7 }; const char* names[] = { "Down", "Up-Down", "Down-Up", "Played", "Waterfall" };
        for (int i = 0; i < 5; ++i)
        {
            Rig r; r.s.mode = modes[i]; auto lows = lowEvents (r.block (4000, keys ({ 67, 60, 64 })), true);
            char label[96]; std::snprintf (label, sizeof (label), "%s: low voice still ascends C3 E3 G3 C3", names[i]);
            check (label, notesOf (lows) == V { 48, 52, 55, 48 });
        }
        Rig r; r.s.octaves = 2; check ("2 octaves: low voice uses the held chord tones only", notesOf (lowEvents (r.block (3100, keys ({ 60, 64, 67 })), true)) == V { 48, 52, 55, 48 });
        Rig q; q.s.order = 4; check ("Random order: low voice still ascends", notesOf (lowEvents (q.block (3100, keys ({ 60, 64, 67 })), true)) == V { 48, 52, 55, 48 });
    }
    {   // the period follows the rate: 4 steps of 1/16 is 500 samples
        Rig r; r.s.rate = 10; auto lows = lowEvents (r.block (2100, keys ({ 60, 64, 67 })), true);
        check ("1/16: one low hit every 4 steps (500 samples)", spacedBy (lows, 25, 500) && notesOf (lows) == V { 48, 52, 55, 48, 52 });
        Rig q; q.s.rate = 4; auto lq = lowEvents (q.block (4100, keys ({ 60, 64, 67 })), true);
        check ("1/4: one low hit every 4 steps (2000 samples)", spacedBy (lq, 25, 2000) && notesOf (lq) == V { 48, 52, 55 });
    }
    {   // chord sizes
        Rig a; check ("four notes: C3 E3 G3 B3 then C3", notesOf (lowEvents (a.block (4500, keys ({ 60, 64, 67, 71 })), true)) == V { 48, 52, 55, 59, 48 });
        Rig b; check ("one note: the same note an octave down every period", notesOf (lowEvents (b.block (3100, keys ({ 64 })), true)) == V { 52, 52, 52, 52 });
        Rig c; check ("two notes alternate", notesOf (lowEvents (c.block (3100, keys ({ 60, 67 })), true)) == V { 48, 55, 48, 55 });
    }
    {   // a rest in the rhythm lane does not cut the low voice
        Rig r; r.s.rhythmOn = true; r.s.rhythmLength = 4; r.s.rhythm[1].type = 1; auto out = r.block (1300, keys ({ 60, 64, 67 }));
        auto offs = lowEvents (out, false);
        check ("a rest does not cut the low note", ! offs.empty() && offs[0].t == 25 + 950);
    }
    {   // releasing every key ends the low voice at once and leaves nothing stuck
        Rig r; auto out = r.block (1500, keys ({ 60, 64, 67 })); auto rel = r.block (200, keys ({ 60, 64, 67 }, false)); out.insert (out.end(), rel.begin(), rel.end());
        auto offs = lowEvents (rel, false);
        check ("releasing the keys cuts the low note and leaves no stuck notes", balanced (out) && r.e.isArpWaiting() && (offs.empty() || offs[0].t == 1500));
        auto later = r.block (3000, {});
        check ("nothing plays after the keys are released", later.empty());
    }
    {   // toggling 2-HAND off mid-note lets the low note finish, then stops adding more
        Rig r; auto out = r.block (500, keys ({ 60, 64, 67 })); r.s.twoHand = false; auto rest = r.block (3000, {}); out.insert (out.end(), rest.begin(), rest.end());
        check ("turning 2-HAND off: current low note ends on schedule, no new ones", notesOf (lowEvents (out, true)) == V { 48 } && lowEvents (out, false).size() == 1 && lowEvents (out, false)[0].t == 975);
    }
    std::printf ("\nDSP RESULT: %d of %d\n", passed, total);
    return passed == total ? 0 : 1;
}
