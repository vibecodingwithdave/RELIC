// Custom chord shapes per scale degree: exact checks on the chord engine (no JUCE).
#include "../Source/NoteEngine.h"
#include <cstdio>
using namespace rf;
using V = std::vector<int>;
static int passed = 0, total = 0;
static void check (const char* name, bool ok) { ++total; passed += ok; std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", name); }
static int mask (V iv) { int m = 0; for (int i : iv) m |= 1 << i; return m; }
static ChordArpSettings cs (int scale = 0, int key = 0) { ChordArpSettings s; s.chordOn = true; s.chordMode = 2; s.scale = scale; s.key = key; return s; }
int main()
{
    {   // defaults are the major scale's own triads
        auto s = cs (); check ("default shapes: C major gives C E G, and D gives D F A (D minor)", chordNotes (60, s) == V { 60, 64, 67 } && chordNotes (62, s) == V { 62, 65, 69 });
        check ("default shapes: B gives B D F (diminished)", chordNotes (71, s) == V { 71, 74, 77 });
        auto o = s; o.chordMode = 0; bool same = true; for (int n : { 60, 62, 64, 65, 67, 69, 71 }) same &= chordNotes (n, s) == chordNotes (n, o);
        check ("default Custom equals Scale mode triads on every degree of C major", same);
    }
    {   // each degree has its own shape
        auto s = cs (); s.degShape[0] = mask ({ 0, 4, 7, 11 }); s.degShape[4] = mask ({ 0, 4, 7, 10, 14 }); s.degShape[1] = mask ({ 0, 7, 12 });
        check ("degree I: maj7 (C E G B)", chordNotes (60, s) == V { 60, 64, 67, 71 });
        check ("degree V: dominant 9th (G B D F A)", chordNotes (67, s) == V { 67, 71, 74, 77, 81 });
        check ("degree II: root, fifth, octave (D A D)", chordNotes (62, s) == V { 62, 69, 74 });
        check ("other degrees keep their own shapes (IV: F A C)", chordNotes (65, s) == V { 65, 69, 72 });
    }
    {   // keys and scales: the shape is built on the scale note
        auto s = cs (0, 7); check ("key of G: the key G gives G B D (degree I)", chordNotes (67, s) == V { 67, 71, 74 });
        auto m = cs (1, 0); m.degShape[2] = mask ({ 0, 4, 7 }); check ("C minor, degree III (Eb): the custom major shape builds Eb G Bb", chordNotes (63, m) == V { 63, 67, 70 });
        auto t = cs (); t.degShape[3] = mask ({ 0, 5, 7 }); check ("a sus4 shape on degree IV (F Bb C)", chordNotes (65, t) == V { 65, 70, 72 });
    }
    {   // scale lock and out-of-scale keys
        auto s = cs (); s.degShape[0] = mask ({ 0, 4, 7, 11 }); s.lock = true;
        check ("scale lock: C# snaps to the C chord, built on C", chordNotes (61, s) == V { 60, 64, 67, 71 });
        s.lock = false; check ("lock off: C# outside the scale gets a plain major chord on the key", chordNotes (61, s) == V { 61, 65, 68 });
    }
    {   // range, inversion, voicing and empty shape
        auto s = cs (); s.degShape[0] = mask ({ 0, 12, 21 }); check ("shape bits reach 21 semitones (C C' A')", chordNotes (60, s) == V { 60, 72, 81 });
        s.degShape[0] = mask ({ 0, 4, 7 }); s.inversion = 1; check ("inversion 1 applies to a custom shape (E G C')", chordNotes (60, s) == V { 64, 67, 72 });
        s.inversion = 0; s.voicing = 1; check ("Open voicing applies (C G E')", chordNotes (60, s) == V { 60, 67, 76 });
        s.voicing = 0; s.degShape[0] = 0; check ("an empty shape plays nothing", chordNotes (60, s).empty());
        s.degShape[0] = mask ({ 0, 21 }); check ("notes stay inside 0-127", [&] { for (int n : chordNotes (120, s)) if (n < 0 || n > 127) return false; return true; }());
    }
    {   // names
        auto s = cs (); s.degShape[0] = mask ({ 0, 4, 7, 11 }); s.degShape[1] = mask ({ 0, 3, 7, 10 }); s.degShape[4] = mask ({ 0, 4, 7, 10 }); s.degShape[2] = mask ({ 0, 5, 7 });
        check ("names: Cmaj7, Dm7, G7, Esus4, and the default Bdim", chordName (0, s) == "Cmaj7" && chordName (1, s) == "Dm7" && chordName (4, s) == "G7" && chordName (2, s) == "Esus4" && chordName (6, s) == "Bdim");
    }
    {   // a held chord restarts when its shape is edited (engine-level)
        NoteEngine e; auto s = cs (); s.arpOn = false; std::vector<NoteEvt> out; std::vector<NoteEvt> in { { 0, true, 60, 0.8f } };
        e.process (0, 100, in, s, 120, -1, false, 1000, out); int on1 = 0; for (auto& v : out) on1 += v.on;
        s.degShape[0] = mask ({ 0, 4, 7, 11 }); out.clear(); e.process (100, 100, {}, s, 120, -1, false, 1000, out); int on2 = 0, off2 = 0; for (auto& v : out) { on2 += v.on; off2 += ! v.on; }
        check ("editing the shape of a held chord re-sounds it (3 notes on, then 3 off and 4 on)", on1 == 3 && on2 == 4 && off2 == 3);
    }
    std::printf ("\nDSP RESULT: %d of %d\n", passed, total);
    return passed == total ? 0 : 1;
}
