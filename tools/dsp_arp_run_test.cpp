// Exact event checks without the host's parameter-discovery notes.
#include "../Source/NoteEngine.h"
#include <cstdio>
using namespace rf;
using V = std::vector<int>;
static int passed = 0, total = 0;
static void check (const char* name, bool ok)
{
    ++total; passed += ok; std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", name);
}
struct Rig
{
    NoteEngine e; ChordArpSettings s; int64_t t = 0;
    Rig() { s.arpOn = true; s.run = false; s.rate = 7; s.gate = 0.5f; }
    std::vector<NoteEvt> block (int n, std::vector<NoteEvt> in = {}, bool playing = false, double ppq = -1)
    {
        for (auto& v : in) v.t += t;
        std::vector<NoteEvt> out;
        e.process (t, n, in, s, 120, ppq, playing, 1000, out); t += n; return out;
    }
};
static std::vector<NoteEvt> keys (bool on = true) { return { { 0, on, 60, 0.8f }, { 0, on, 64, 0.7f }, { 0, on, 67, 0.6f } }; }
static V notes (const std::vector<NoteEvt>& out) { V v; for (auto& e : out) if (e.on) v.push_back (e.note); return v; }
static bool balanced (const std::vector<NoteEvt>& out)
{
    std::array<int, 128> count {};
    for (auto& e : out) { auto& c = count[(size_t) e.note]; c += e.on ? 1 : -1; if (c < 0) return false; }
    return std::all_of (count.begin(), count.end(), [] (int c) { return c == 0; });
}
int main()
{
    check ("old patches default to GO", ChordArpSettings().run);
    const V expected[] = { {60,64,67}, {67,64,60}, {60,64,67,64}, {67,64,60,64}, {}, {60,64,67}, {60,64,67}, {60,64,67,60,64,60,67,64} };
    for (int mode = 0; mode < 8; ++mode)
    {
        Rig r; r.s.mode = mode; auto out = r.block (5000, keys()); auto ns = notes (out);
        const bool sequence = mode == 4 ? (ns.size() == 3 && std::all_of (ns.begin(), ns.end(), [] (int n) { return n == 60 || n == 64 || n == 67; })) : ns == expected[mode];
        char label[128]; std::snprintf (label, sizeof (label), "STOP mode %d: exactly one pass, then waits with all note-offs", mode);
        check (label, sequence && balanced (out) && r.e.isArpWaiting() && r.block (2000).empty());
        if (mode == 6) check ("Chord mode is one simultaneous hit", out.size() == 6 && out[0].t == 25 && out[1].t == 25 && out[2].t == 25);
    }
    for (int mode : {0,1,2,3})
    {
        Rig r; r.s.mode = mode; r.s.octaves = 2; auto out = r.block (5000, keys()); auto ns = notes (out);
        char label[128]; std::snprintf (label, sizeof (label), "STOP mode %d includes the octave-expanded pattern", mode);
        check (label, ns.size() == (mode < 2 ? 6u : 10u) && balanced (out) && r.e.isArpWaiting());
    }
    for (int group = 1; group <= 4; ++group)
    {
        Rig r; r.s.mode = 7; r.s.grouping = group; auto out = r.block (5000, keys());
        check ("grouped Waterfall ends after one 16-step bar", notes (out).size() == 16 && balanced (out) && r.e.isArpWaiting());
    }
    for (int order = 0; order <= 4; ++order)
    {
        Rig r; r.s.order = order; auto out = r.block (3000, keys());
        check ("order preserves the one-pass note count", notes (out).size() == 3 && balanced (out) && r.e.isArpWaiting());
    }
    {
        Rig r; auto out = r.block (3000, {{0,true,60,0.8f},{350,true,64,0.7f},{350,true,67,0.6f}});
        check ("adding keys after completion does not re-arm STOP", notes (out) == V{60} && r.e.isArpWaiting());
        check ("partial release and repress does not re-arm", r.block (1000, {{0,false,67},{200,true,67,0.6f}}).empty());
        r.block (100, keys (false)); out = r.block (2000, keys());
        check ("release all then press re-arms exactly one pass", notes (out) == V{60,64,67} && balanced (out) && r.e.isArpWaiting());
        r.s.chordOn = true; check ("editing chord settings while waiting does not re-arm", r.block (1000).empty());
    }
    {
        Rig r; auto in = keys(); in.push_back ({350,true,72,0.8f}); auto out = r.block (3000, in);
        check ("adding a key during the pass extends it without restarting", notes (out) == V{60,64,67,72} && balanced (out));
    }
    {
        Rig r; auto in = keys(); auto off = keys (false); for (auto& e : off) e.t = 100; in.insert (in.end(), off.begin(), off.end());
        auto out = r.block (2000, in);
        check ("early physical release cuts the pass and sends note-offs", notes (out) == V{60} && balanced (out) && r.e.isArpWaiting());
    }
    {
        Rig r; r.s.gate = 1; auto out = r.block (526, keys());
        check ("last step is not waiting while its tied gate is pending", notes (out).size() == 3 && ! r.e.isArpWaiting() && ! r.e.isArpRunning());
        auto tail = r.block (500); out.insert (out.end(), tail.begin(), tail.end());
        check ("last tied note gets its scheduled off before waiting", balanced (out) && r.e.isArpWaiting());
        r.s.run = true; auto go = r.block (1100);
        check ("GO resumes held keys at step zero and loops", notes (go) == V{60,64,67,60,64} && go[0].t == 1051);
    }
    {
        Rig r; r.s.run = true; auto out = r.block (100, keys()); r.s.run = false;
        auto rest = r.block (2000); out.insert (out.end(), rest.begin(), rest.end());
        check ("GO to STOP finishes its current pass without an immediate cut", notes (out) == V{60,64,67} && balanced (out) && r.e.isArpWaiting());
    }
    {
        Rig r; r.s.run = true; r.block (600, keys()); r.s.run = false;
        auto tail = r.block (2000);
        check ("GO to STOP at a completed pass does not start another", notes (tail).empty() && r.e.isArpWaiting());
    }
    {
        Rig r; r.s.hold = true; auto in = keys(); auto off = keys (false); for (auto& e : off) e.t = 100; in.insert (in.end(), off.begin(), off.end());
        auto out = r.block (2000, in);
        check ("HOLD finishes one cycle after physical release", notes (out) == V{60,64,67} && balanced (out) && r.e.heldCount() == 3 && r.e.isArpWaiting());
        out = r.block (1000, {{0,true,72,0.8f}});
        check ("new HOLD gesture replaces latched keys and re-arms", notes (out) == V{72} && balanced (out) && r.e.heldCount() == 1);
        r.s.run = true; out = r.block (600);
        check ("GO resumes the HOLD buffer and loops", notes (out) == V{72,72,72});
    }
    {
        Rig r; r.s.hold = true; r.block (100, keys()); r.block (100, keys (false));
        auto out = r.block (2000, {{0,true,72,0.8f}});
        check ("fresh HOLD gesture replaces a still-playing pass safely", notes (out) == V{72} && r.e.isArpWaiting());
    }
    {
        Rig r; r.block (100, keys()); std::vector<NoteEvt> out; r.e.reset (r.t, out);
        check ("reset releases notes and clears the gesture", out.size() == 1 && ! out[0].on && r.block (2000).empty() && r.e.heldCount() == 0);
        check ("fresh chord after reset plays another pass", notes (r.block (2000, keys())) == V{60,64,67});
    }
    {
        Rig r; r.s.arpOn = false; r.s.chordOn = true; auto out = r.block (100, {{0,true,60,0.8f}}); r.s.run = true;
        check ("GO/STOP leaves bypassed chord playback alone", notes (out) == V{60,64,67} && r.block (100).empty());
        auto off = r.block (100, {{0,false,60}}); out.insert (out.end(), off.begin(), off.end());
        check ("bypassed chords release normally", balanced (out));
    }
    {
        Rig r; r.block (2000, keys()); r.s.run = true; auto out = r.block (200, {}, true, 0.2);
        check ("GO resumes on the host beat grid", notes (out) == V{60} && out[0].t == 2150);
    }
    {
        Rig r; r.block (2000, {{0,true,127,0.8f}}); r.s.octaves = 4; r.block (100, {{0,false,127}});
        auto out = r.block (1000, {{0,true,127,0.8f}});
        check ("MIDI ceiling counts only notes actually in the pattern", notes (out) == V{127} && balanced (out));
    }
    std::printf ("DSP RESULT: %d of %d arp run checks passed\n", passed, total);
    return passed == total ? 0 : 1;
}
