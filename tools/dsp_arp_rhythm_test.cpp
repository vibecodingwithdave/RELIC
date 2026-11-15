// Rhythm semantics are based on reference help; exact boundaries/law are Relic choices.
#include "../Source/NoteEngine.h"
#include <cstdio>
using namespace rf;
using E = std::vector<NoteEvt>;
using V = std::vector<int>;
static int pass = 0, total = 0;
static void check (const char* name, bool ok) { ++total; pass += ok; std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", name); }
static E keys (bool on = true) { return {{0,on,60,.8f},{0,on,64,.6f},{0,on,67,.4f}}; }
static V notes (const E& out) { V v; for (auto& e : out) if (e.on) v.push_back (e.note); return v; }
static bool balanced (const E& out)
{
    std::array<int,128> c {};
    for (auto& e : out) { auto& n = c[(size_t) e.note]; n += e.on ? 1 : -1; if (n < 0) return false; }
    return std::all_of (c.begin(),c.end(),[] (int n) { return n == 0; });
}
static bool equal (const E& a, const E& b)
{
    if (a.size() != b.size()) return false;
    for (size_t i=0;i<a.size();++i) if (a[i].t!=b[i].t || a[i].on!=b[i].on || a[i].note!=b[i].note || a[i].vel!=b[i].vel) return false;
    return true;
}
struct Rig
{
    NoteEngine engine; ChordArpSettings s; int64_t t=0;
    Rig() { s.arpOn=true; s.rhythmOn=true; s.rate=7; s.gate=.5f; }
    void lane (std::initializer_list<int> types) { s.rhythmLength=(int)types.size(); int i=0; for(int v:types) s.rhythm[(size_t)i++].type=v; }
    E block (int n,E input={})
    {
        for(auto& e:input) e.t+=t;
        E out; engine.process(t,n,input,s,120,-1,false,1000,out); t+=n; return out;
    }
};
int main()
{
    check("default lane is off, 16 full-velocity hits", !ChordArpSettings().rhythmOn && ChordArpSettings().rhythmLength==16 && ChordArpSettings().rhythm[15].velocity==1);
    for(int mode=0;mode<8;++mode)
    {
        Rig a,b; a.s.mode=b.s.mode=mode; a.s.human=b.s.human=.8f; a.s.run=b.s.run=false;
        b.s.rhythmOn=false;
        check("all-hit lane preserves the original event stream in every mode", equal(a.block(6000,keys()),b.block(6000,keys())));
    }
    {
        Rig a,b; a.s.rhythmOn=b.s.rhythmOn=false; a.lane({1,2,1}); a.s.rhythm[0].velocity=0;
        check("disabled lane ignores cells and velocities",equal(a.block(2000,keys()),b.block(2000,keys())));
    }
    {
        Rig r; r.lane({0,1}); auto out=r.block(2000,keys());
        check("rests retain the next pitch and lane wraps independently",notes(out)==V{60,64,67,60} && balanced(out));
    }
    {
        Rig r; r.lane({0,2,2,0,2,2}); r.s.octaves=2; auto out=r.block(1500,keys());
        check("ties sustain original pitch without retriggers or lost pattern notes", notes(out)==V{60,64} && balanced(out));
        check("ordinary tie ends at the gate point of its last tied cell",out.size()==4 && out[0].t==25 && out[1].t==650 && !out[1].on);
    }
    {
        Rig r; r.lane({0,2,1}); auto out=r.block(750,keys());
        check("ordinary tie releases before the rest at its gate point",out.size()==2 && out[1].t==400 && balanced(out));
    }
    {
        Rig r; r.lane({2,0,1}); auto out=r.block(750,keys());
        check("orphan tie is silent and retains the first pattern note",notes(out)==V{60} && balanced(out));
    }
    for(int type:{1,2})
    {
        Rig r; r.lane({type}); r.s.run=false;
        check("all-rest and all-tie STOP remain silent and armed without consuming notes",r.block(2000,keys()).empty() && r.engine.isArpRunning() && r.engine.currentStep()==8);
    }
    {
        Rig r; r.lane({0,2,2}); r.s.run=false; auto out=r.block(600,keys());
        check("final STOP tie remains active through its full cell",out.size()==1 && !r.engine.isArpWaiting());
        auto tail=r.block(2000); out.insert(out.end(),tail.begin(),tail.end());
        check("STOP counts three hits and closes their trailing ties",notes(out)==V{60,64,67} && out.back().t==2275 && balanced(out) && r.engine.isArpWaiting());
        r.block(100,keys(false));
        check("fresh gesture resets rhythm phase and re-arms STOP",notes(r.block(100,keys()))==V{60});
    }
    {
        Rig r; r.lane({0,0,0,2}); r.s.run=false; auto out=r.block(2000,keys());
        check("STOP extends its final hit through the following tie",notes(out)==V{60,64,67} && out.back().t==1025 && balanced(out));
    }
    {
        Rig r; r.lane({0,2,2}); auto out=r.block(2000,keys()); r.s.run=false;
        auto tail=r.block(1000); out.insert(out.end(),tail.begin(),tail.end());
        check("GO to STOP at a completed tied pass schedules the final release",notes(out)==V{60,64,67} && out.back().t==2275 && balanced(out) && r.engine.isArpWaiting());
    }
    {
        Rig r; r.lane({0,1}); r.s.gate=1; auto out=r.block(500,keys());
        check("rest cuts the overlap of gate 100 percent",out.size()==2 && out[1].t==275 && balanced(out));
    }
    {
        Rig r; r.lane({0,2,0}); r.s.swing=1; auto out=r.block(800,keys());
        check("tie uses the gate point of its swung final cell",notes(out)==V{60,64} && out[1].t==525 && balanced(out));
    }
    {
        Rig r; r.lane({0,2}); auto out=r.block(100,keys()); r.s.rhythmOn=false; auto tail=r.block(100); out.insert(out.end(),tail.begin(),tail.end());
        check("disabling a lane releases its held tie at the block boundary",out.size()==2 && out[1].t==100 && balanced(out));
    }
    {
        Rig r; r.lane({0,2}); auto out=r.block(2000,{{0,true,60,.8f},{100,false,60,0}});
        check("physical release cancels all tie ownership and notes",out.size()==2 && out[1].t==100 && balanced(out) && r.engine.isArpWaiting());
    }
    {
        Rig r; r.lane({0,2}); r.s.hold=true; r.s.run=false; auto out=r.block(2000,{{0,true,60,.8f},{100,false,60,0}});
        check("HOLD preserves buffer and STOP completes its final tie",out.size()==2 && balanced(out) && r.engine.heldCount()==1 && r.engine.isArpWaiting());
    }
    {
        Rig r; r.lane({0,2,1}); r.s.mode=6; auto out=r.block(1000,keys());
        check("tie sustains and releases every chord-mode voice",notes(out)==V{60,64,67,60,64,67} && out[3].t==400);
        // End the second hit to check the entire output stream for stuck notes.
        auto tail=r.block(100,keys(false)); out.insert(out.end(),tail.begin(),tail.end());
        check("chord-mode tie cleanup balances all voices",balanced(out));
    }
    {
        Rig r; r.lane({0}); r.s.run=false; r.s.rhythm[0].velocity=.5f; auto out=r.block(2000,keys());
        check("velocity multiplies each incoming note's level",out.size()==6 && out[0].vel==.4f && out[2].vel==.3f && out[4].vel==.2f);
        r.block(100,keys(false)); r.s.rhythm[0].velocity=0;
        auto quiet=r.block(2000,keys());
        check("stored zero velocity remains a quiet hit at the six-percent floor",notes(quiet)==V{60,64,67} && std::abs(quiet[0].vel-.048f)<1e-6f && r.engine.isArpWaiting());
    }
    {
        Rig r; r.lane({0,2}); r.block(100,keys()); E off; r.engine.reset(r.t,off);
        check("reset releases ties and clears pending ownership",off.size()==1 && !off[0].on && r.block(2000).empty() && r.engine.isArpWaiting());
    }
    {
        Rig r; r.s.arpOn=false; r.lane({1}); auto out=r.block(100,keys());
        auto off=r.block(100,keys(false)); out.insert(out.end(),off.begin(),off.end());
        check("ARP off ignores an all-rest lane",notes(out)==V{60,64,67} && balanced(out));
    }
    for(int length:{1,16})
    {
        Rig r; r.s.rhythmLength=length; r.s.run=false; r.s.mode=7;
        auto out=r.block(6000,keys());
        check("lane length bounds preserve Waterfall's eight-step STOP cycle",notes(out).size()==8 && balanced(out));
    }
    {
        Rig r; r.s.run=false; r.lane({0,1}); auto out=r.block(3000,keys());
        check("STOP completes its full pitch pass despite rests",notes(out)==V{60,64,67} && out[4].t==1025 && balanced(out) && r.engine.isArpWaiting());
    }
    {
        Rig r; r.lane({0,0}); r.s.rhythm[0].velocity=0; auto out=r.block(1500,keys());
        check("quiet floor hits still consume the next pitch",notes(out)==V{60,64,67,60,64,67} && out[0].vel<.05f && out[2].note==64 && balanced(out));
    }
    for(int group:{0,1})
    {
        Rig r; r.s.run=false; r.s.mode=7; r.s.grouping=group; r.lane({0,1}); auto out=r.block(6000,keys());
        check("Waterfall STOP counts eight or sixteen emitted hits through rests",notes(out).size()==(group==0?8u:16u) && balanced(out) && r.engine.isArpWaiting());
    }
    for (auto test : { std::pair<float,int>{0.0f,56}, {.3f,188}, {.5f,423}, {1.0f,1525} })
    {
        Rig r; r.s.mode=7; r.s.gate=test.first; r.s.rhythmLength=16;
        for (int i=1;i<16;++i) r.s.rhythm[(size_t)i].type=1;
        auto out=r.block(2000,{{0,true,60,.8f}});
        check("measured Waterfall gate curve rings through rests",out.size()==2 && out[1].t==test.second && balanced(out));
    }
    {
        Rig r; r.s.mode=7; r.s.grouping=1; r.s.gate=.3f; r.s.rhythmLength=16;
        for (int i=1;i<16;++i) r.s.rhythm[(size_t)i].type=1;
        auto out=r.block(2000,{{0,true,60,.8f}});
        check("grouped Waterfall scales the gate curve to its own step length",out.size()==2 && out[1].t==107 && balanced(out));
    }
    std::printf("DSP RESULT: %d of %d arp rhythm checks passed\n",pass,total);
    return pass==total ? 0 : 1;
}
