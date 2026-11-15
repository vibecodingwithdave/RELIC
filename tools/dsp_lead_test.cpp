// Voice-leading checks on the chord engine alone (no plugin host, so nothing else plays first). Built and run by tools/verify_arp_chords.py
#include "../Source/NoteEngine.h"
#include <cstdio>
using namespace rf;
static int pass = 0, total = 0;
static void check (const char* name, bool ok) { ++total; pass += ok; std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", name); }
struct Rig
{
    NoteEngine e; ChordArpSettings s; int64_t base = 0;
    Rig() { s.chordOn = true; s.type = 3; s.voiceLead = true; }
    std::vector<int> chord (int note)
    {
        base += 200000; std::vector<NoteEvt> out; std::vector<NoteEvt> on = { { base + 100, true, note, 0.8f } };
        e.process (base, 512, on, s, 120.0, -1.0, false, 44100.0, out); std::vector<int> v; for (auto& o : out) if (o.on) v.push_back (o.note); std::sort (v.begin(), v.end());
        std::vector<NoteEvt> off = { { base + 20000, false, note, 0.0f } }; for (int b = 1; b < 60; ++b) { std::vector<NoteEvt> none; if (b == 40) none = off; out.clear(); e.process (base + b * 512, 512, b == 40 ? off : none, s, 120.0, -1.0, false, 44100.0, out); }
        return v;
    }
    std::vector<std::vector<int>> run (std::vector<int> keys) { std::vector<std::vector<int>> r; for (int k : keys) r.push_back (chord (k)); return r; }
};
using V = std::vector<int>; using VV = std::vector<V>;
int main()
{
    { Rig r; check ("fresh start: the first chord is plain root position (C5: 72 76 79 83)", r.chord (72) == V { 72, 76, 79, 83 }); }
    { Rig r; check ("C F G C leads like the reference: C E G B | E F A C | F G B D | E G B C", r.run ({ 60, 65, 67, 60 }) == VV { { 60, 64, 67, 71 }, { 64, 65, 69, 72 }, { 65, 67, 71, 74 }, { 64, 67, 71, 72 } }); }
    { Rig r; check ("C A D G C leads like the reference: smallest moves each time", r.run ({ 60, 57, 62, 55, 60 }) == VV { { 60, 64, 67, 71 }, { 60, 64, 67, 69 }, { 60, 62, 65, 69 }, { 59, 62, 65, 67 }, { 59, 60, 64, 67 } }); }
    { Rig a, b; auto x = a.run ({ 60, 65 }), y = b.run ({ 72, 77 }); bool same = true; for (size_t i = 0; i < x.size(); ++i) for (size_t k = 0; k < x[i].size(); ++k) same &= (x[i][k] + 12 == y[i][k]); check ("an octave up, the same progression an octave up", same); }
    { Rig r; r.run ({ 60, 65 }); r.s.voiceLead = false; r.chord (60); r.s.voiceLead = true; check ("switching voice lead off and on starts the progression over", r.chord (72) == V { 72, 76, 79, 83 }); }
    { Rig r; r.run ({ 60, 65 }); r.s.key = 2; check ("changing the key starts the progression over", r.chord (74) == V { 74, 78, 81, 85 }); }
    { Rig r; r.s.voiceLead = false; check ("voice lead off: root position chords every time", r.run ({ 60, 65 }) == VV { { 60, 64, 67, 71 }, { 65, 69, 72, 76 } }); }
    std::printf ("\nDSP RESULT: %d of %d\n", pass, total); return pass == total ? 0 : 1;
}
