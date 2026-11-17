// Header-only checks of the automation lanes (no JUCE): timing, shapes, drawn cycle sign, envelope, depth modulation, fan-out, determinism.
#include "../Source/Dsp/Automation.h"
#include "../Source/Dsp/Effects2.h"
#include <cstdio>
#include <cstring>

static int pass = 0, fail = 0;
static void check (const char* name, bool ok, const char* note = "") { std::printf ("  [%s] %s %s\n", ok ? "PASS" : "FAIL", name, note); ok ? ++pass : ++fail; }

// Measure the frequency of lane 0 from the spacing of positive-going zero crossings (first to last), which is exact to a sample.
static double measureHz (rf::AutoLaneCfg c, double bpm, double secs = 8.0, double sr = 44100.0)
{
    rf::AutoBank b; b.prepare(); rf::AutoLaneCfg cfg[8]; cfg[0] = c; cfg[0].on = true; cfg[0].dst = rf::AtPitch; cfg[0].depth = 1.0f; rf::LegacyCfg lg;
    long first = -1, last = -1; int crossings = 0; float prev = 0.0f; const long total = (long) (secs * sr);
    for (long done = 0; done < total; done += 4096)
    {
        const int n = (int) std::min<long> (4096, total - done); b.fill (cfg, lg, 1.0f, bpm, sr, 0, n);
        for (int i = 0; i < n; ++i) { const float v = b.at (rf::AtPitch, i); if (prev < 0.0f && v >= 0.0f) { if (first < 0) first = done + i; last = done + i; ++crossings; } prev = v; }
    }
    return crossings > 1 ? (crossings - 1) * sr / (double) (last - first) : 0.0;
}

int main()
{
    std::printf ("Automation lanes\n");
    const double expect[7] = { 0.5, 1.0, 2.0, 4.0, 8.0, 0.25, 0.125 };
    char note[160] = "";
    for (int d = 0; d < 7; ++d)
    {
        rf::AutoLaneCfg c; c.shape = rf::AsSine; c.div = d; const double hz = measureHz (c, 120.0, d == 6 ? 80.0 : 20.0), e = expect[d];
        std::snprintf (note, sizeof note, "measured %.4f Hz (expect %.4f)", hz, e);
        check (d == 0 ? "1 bar at 120 BPM = 0.5 Hz" : d == 1 ? "1/2 = 1 Hz" : d == 2 ? "1/4 = 2 Hz" : d == 3 ? "1/8 = 4 Hz" : d == 4 ? "1/16 = 8 Hz" : d == 5 ? "2 bars = 0.25 Hz" : "4 bars = 0.125 Hz", std::abs (hz - e) < 0.002 * e, note);
    }
    { rf::AutoLaneCfg c; c.div = 2; c.feel = 0; const double a = measureHz (c, 120.0); c.feel = 1; const double b = measureHz (c, 120.0); c.feel = 2; const double t = measureHz (c, 120.0);
      std::snprintf (note, sizeof note, "%.3f / %.3f / %.3f Hz", a, b, t);
      check ("1/4 straight, dotted, triplet = 2, 1.333, 3 Hz", std::abs (a - 2.0) < 0.005 && std::abs (b - 4.0 / 3.0) < 0.005 && std::abs (t - 3.0) < 0.005, note); }
    { rf::AutoLaneCfg c; c.div = 7; c.hz = 3.3f; const double h = measureHz (c, 77.0); std::snprintf (note, sizeof note, "%.3f Hz", h); check ("free rate ignores tempo", std::abs (h - 3.3) < 0.01, note); }
    { rf::AutoLaneCfg c; c.div = 2; const double a = measureHz (c, 60.0), b = measureHz (c, 180.0); check ("synced lanes follow the tempo", std::abs (a - 1.0) < 0.003 && std::abs (b - 3.0) < 0.01); }

    // Bipolar drawn cycle: a flat +1 and a flat -1 curve give opposite signs
    {
        float up[32], dn[32]; for (int i = 0; i < 32; ++i) { up[i] = 1.0f; dn[i] = 0.0f; }
        rf::AutoBank b; b.prepare(); rf::AutoLaneCfg cfg[8]; rf::LegacyCfg lg;
        cfg[0].on = true; cfg[0].shape = rf::AsDraw; cfg[0].dst = rf::AtPitch; cfg[0].depth = 0.25f; cfg[0].pts = up;
        cfg[1].on = true; cfg[1].shape = rf::AsDraw; cfg[1].dst = rf::AtPan; cfg[1].depth = 0.25f; cfg[1].pts = dn;
        b.fill (cfg, lg, 1.0f, 120.0, 44100.0, 0, 1000);
        check ("flat +1 DRAW: pitch +0.25 (x12 = 3 semitones)", std::abs (b.at (rf::AtPitch, 500) * 12.0f - 3.0f) < 1e-4f);
        check ("flat -1 DRAW: pan -0.25 (opposite sign)", std::abs (b.at (rf::AtPan, 500) + 0.25f) < 1e-4f);
        b.fill (cfg, lg, 0.5f, 120.0, 44100.0, 0, 1000); check ("intensity 0.5 halves the pitch move (1.5 semitones)", std::abs (b.at (rf::AtPitch, 500) * 12.0f - 1.5f) < 1e-4f);
        b.fill (cfg, lg, 0.0f, 120.0, 44100.0, 0, 1000); check ("intensity 0 removes it", b.at (rf::AtPitch, 500) == 0.0f);
        cfg[0].dst = rf::AtGroupPitch; b.fill (cfg, lg, 1.0f, 120.0, 44100.0, 0, 1000); check ("group pitch depth .25 = +6 semitones", std::abs (b.at (rf::AtGroupPitch, 10) * 12.0f - 6.0f) < 1e-4f);
        cfg[0].dst = rf::AtGroupDetune; b.fill (cfg, lg, 1.0f, 120.0, 44100.0, 0, 1000); check ("group detune depth .25 = 12.5 cents", std::abs (b.at (rf::AtGroupDetune, 10) * 50.0f - 12.5f) < 1e-4f);
        cfg[0].dst = rf::AtMaster; cfg[0].depth = 0.1f; b.fill (cfg, lg, 1.0f, 120.0, 44100.0, 0, 1000); check ("master depth .1 = +0.15 gain", std::abs (b.at (rf::AtMaster, 10) - 0.15f) < 1e-5f);
        cfg[0].dst = rf::AtSynthVol; b.fill (cfg, lg, 1.0f, 120.0, 44100.0, 0, 1000); check ("synth volume depth .1 = +0.23 gain", std::abs (b.at (rf::AtSynthVol, 10) - 0.23f) < 1e-5f);
    }
    // Independent generators: eight lanes at eight different rates do not share one
    {
        rf::AutoBank b; b.prepare(); rf::AutoLaneCfg cfg[8]; rf::LegacyCfg lg; const int dsts[8] = { rf::AtCutoff, rf::AtReso, rf::AtDelayMix, rf::AtReverbMix, rf::AtChorusMix, rf::AtFlangerMix, rf::AtWahFreq, rf::AtWidth };
        for (int k = 0; k < 8; ++k) { cfg[k].on = true; cfg[k].shape = rf::AsSine; cfg[k].div = 7; cfg[k].hz = 0.5f * (float) (k + 1); cfg[k].dst = dsts[k]; cfg[k].depth = 1.0f; }
        int cross[8] = {}; float prev[8] = {}; for (int done = 0; done < 8 * 44100; done += 4096) { b.fill (cfg, lg, 1.0f, 120.0, 44100.0, 0, 4096); for (int i = 0; i < 4096; ++i) for (int k = 0; k < 8; ++k) { const float v = b.at (dsts[k], i); if (prev[k] < 0 && v >= 0) ++cross[k]; prev[k] = v; } }
        bool ok = true; for (int k = 0; k < 8; ++k) if (std::abs (cross[k] / 8.0 - 0.5 * (k + 1)) > 0.2) ok = false;
        check ("eight lanes run at eight independent rates", ok);
    }
    // Shapes
    {
        rf::AutoBank b; b.prepare(); rf::AutoLaneCfg cfg[8]; rf::LegacyCfg lg; cfg[0].on = true; cfg[0].dst = rf::AtPitch; cfg[0].depth = 1.0f; cfg[0].div = 7; cfg[0].hz = 1.0f;
        float mn = 9, mx = -9; int hold = 0, changes = 0; float last = -2;
        cfg[0].shape = rf::AsSquare; b.fill (cfg, lg, 1.0f, 120.0, 1000.0, 0, 1000); for (int i = 0; i < 1000; ++i) { mn = std::min (mn, b.at (rf::AtPitch, i)); mx = std::max (mx, b.at (rf::AtPitch, i)); }
        check ("square swings -1..+1", mn == -1.0f && mx == 1.0f);
        b.reset(); cfg[0].shape = rf::AsSampleHold; b.fill (cfg, lg, 1.0f, 120.0, 1000.0, 0, 3000); for (int i = 0; i < 3000; ++i) { const float v = b.at (rf::AtPitch, i); if (v != last) { ++changes; last = v; } ++hold; }
        check ("sample and hold holds for whole cycles (new value each cycle)", changes >= 2 && changes <= 4, "");
        b.reset(); cfg[0].shape = rf::AsTri; b.fill (cfg, lg, 1.0f, 120.0, 1000.0, 0, 1000); mn = 9; mx = -9; for (int i = 0; i < 1000; ++i) { mn = std::min (mn, b.at (rf::AtPitch, i)); mx = std::max (mx, b.at (rf::AtPitch, i)); }
        check ("triangle swings about -1..+1", mn < -0.99f && mx > 0.99f);
    }
    // Envelope: attack-decay, unipolar, one note-triggered shot
    {
        rf::AutoBank b; b.prepare(); rf::AutoLaneCfg cfg[8]; rf::LegacyCfg lg; cfg[0].on = true; cfg[0].shape = rf::AsEnv; cfg[0].dst = rf::AtCutoff; cfg[0].depth = 1.0f; cfg[0].trig = true; cfg[0].envA = 100.0f; cfg[0].envD = 200.0f; cfg[0].div = 7; cfg[0].hz = 0.05f;
        const double sr = 10000.0; b.retrigger (cfg); b.fill (cfg, lg, 1.0f, 120.0, sr, 0, 4000);
        const float atPeak = b.at (rf::AtCutoff, 1000), mid = b.at (rf::AtCutoff, 500), after = b.at (rf::AtCutoff, 3900), early = b.at (rf::AtCutoff, 500);
        check ("envelope rises over the attack, peaks at 1", std::abs (atPeak - 1.0f) < 0.02f && std::abs (mid - 0.5f) < 0.02f, "");
        check ("envelope falls back to 0 after the decay and stays (no negative output)", after == 0.0f && early >= 0.0f);
    }
    // Depth modulation: lane 1's depth is driven by lane 5; zero base depth survives
    {
        rf::AutoBank b; b.prepare(); rf::AutoLaneCfg cfg[8]; rf::LegacyCfg lg; float up[32]; for (int i = 0; i < 32; ++i) up[i] = 1.0f;
        cfg[0].on = true; cfg[0].shape = rf::AsDraw; cfg[0].pts = up; cfg[0].dst = rf::AtPitch; cfg[0].depth = 0.0f;
        cfg[4].on = true; cfg[4].shape = rf::AsDraw; cfg[4].pts = up; cfg[4].dst = rf::AtLane1Depth; cfg[4].depth = 0.5f;
        b.fill (cfg, lg, 1.0f, 120.0, 44100.0, 0, 100); check ("zero-depth lane becomes active through another lane's depth modulation", std::abs (b.at (rf::AtPitch, 50) - 0.5f) < 1e-4f);
        cfg[4].on = false; b.fill (cfg, lg, 1.0f, 120.0, 44100.0, 0, 100); check ("without the modulator it stays silent", b.at (rf::AtPitch, 50) == 0.0f);
        // cycle: lanes 1 and 2 modulate each other's depth; both use base depth, nothing runs away
        rf::AutoLaneCfg cy[8]; for (int k = 0; k < 2; ++k) { cy[k].on = true; cy[k].shape = rf::AsDraw; cy[k].pts = up; cy[k].dst = rf::AtLane1Depth + (1 - k); cy[k].depth = 1.0f; }
        cy[2].on = true; cy[2].shape = rf::AsDraw; cy[2].pts = up; cy[2].dst = rf::AtPitch; cy[2].depth = 0.0f;
        b.fill (cy, lg, 1.0f, 120.0, 44100.0, 0, 100); check ("a depth-modulation cycle is deterministic and finite", std::isfinite (b.at (rf::AtPitch, 50)) && b.at (rf::AtPitch, 50) == 0.0f);
    }
    // Fan-out generator: all eleven bits at once, alongside the eight lanes
    {
        rf::AutoBank b; b.prepare(); rf::AutoLaneCfg cfg[8]; rf::LegacyCfg lg; lg.on = true; lg.shape = 3; lg.hz = 2.0; lg.depth = 1.0f; for (int k = 0; k < 11; ++k) { lg.bit[k] = true; lg.amt[k] = 0.1f * (float) (k + 1); }
        for (int k = 0; k < 8; ++k) { cfg[k].on = true; cfg[k].shape = rf::AsSquare; cfg[k].dst = rf::AtPitch; cfg[k].depth = 0.01f; }
        b.fill (cfg, lg, 1.0f, 120.0, 44100.0, 0, 100);
        bool ok = true; for (int k = 0; k < 11; ++k) { const int t = rf::legacyTarget (k); if (t == rf::AtCutoff + 100) continue; const float v = b.at (t, 10); if (v == 0.0f) ok = false; }
        check ("all eleven fan-out bits drive their targets with eight lanes also running", ok);
        float cutoff = b.at (rf::AtCutoff, 10), delay = b.at (rf::AtDelayMix, 10); check ("each bit uses its own amount", std::abs (delay / cutoff - 3.0f) < 1e-4f);
        lg.bit[2] = false; b.fill (cfg, lg, 1.0f, 120.0, 44100.0, 0, 100); check ("a cleared bit stops driving its target", b.at (rf::AtDelayMix, 10) == 0.0f);
    }
    // Determinism and block-size independence
    {
        auto run = [&] (int blk) { rf::AutoBank b; b.prepare(); rf::AutoLaneCfg cfg[8]; rf::LegacyCfg lg; cfg[0].on = true; cfg[0].shape = rf::AsSampleHold; cfg[0].div = 7; cfg[0].hz = 3.0f; cfg[0].dst = rf::AtPitch; cfg[0].depth = 1.0f;
                  std::vector<float> o; for (int done = 0; done < 20000; done += blk) { const int n = std::min (blk, 20000 - done); b.fill (cfg, lg, 1.0f, 120.0, 44100.0, 0, n); for (int i = 0; i < n; ++i) o.push_back (b.at (rf::AtPitch, i)); } return o; };
        check ("output does not depend on block size", run (64) == run (4096));
    }
    // Rhythmic gate
    {
        auto run = [&] (int shape, float depth, float smooth, float rate) { rf::RhythmGate g; g.prepare (44100.0); std::vector<float> o; for (int i = 0; i < 44100; ++i) { float l = 1.0f, r = 1.0f; g.process (l, r, rate, depth, shape, smooth, 44100.0); o.push_back (l); } return o; };
        auto sq = run (0, 1.0f, 0.0f, 4.0f); float mn = 9, mx = -9; for (float v : sq) { mn = std::min (mn, v); mx = std::max (mx, v); }
        check ("gate: square at depth 1 shuts completely and opens fully", mn == 0.0f && mx == 1.0f);
        int shut = 0; for (float v : sq) if (v == 0.0f) ++shut; check ("gate: square is shut half the time", std::abs (shut / 44100.0 - 0.5) < 0.01);
        auto half = run (0, 0.5f, 0.0f, 4.0f); mn = 9; for (float v : half) mn = std::min (mn, v); check ("gate: depth .5 closes to half level", std::abs (mn - 0.5f) < 1e-6f);
        auto none = run (0, 0.0f, 0.0f, 4.0f); bool one = true; for (float v : none) if (v != 1.0f) one = false; check ("gate: depth 0 is transparent", one);
        auto soft = run (0, 1.0f, 1.0f, 4.0f), hard = run (0, 1.0f, 0.0f, 4.0f); float ms = 0, mh = 0; for (size_t i = 1; i < soft.size(); ++i) { ms = std::max (ms, std::abs (soft[i] - soft[i - 1])); mh = std::max (mh, std::abs (hard[i] - hard[i - 1])); }
        check ("gate: Smooth softens the edges (largest step much smaller)", ms < 0.1f * mh, "");
        auto sn = run (1, 1.0f, 0.0f, 4.0f); mn = 9; mx = -9; for (float v : sn) { mn = std::min (mn, v); mx = std::max (mx, v); } check ("gate: sine shape swells smoothly between shut and open", mn < 0.001f && mx > 0.999f);
        auto sw = run (2, 1.0f, 0.0f, 4.0f); check ("gate: saw down starts open and falls", sw[1] > 0.99f && sw[2000] < sw[100]);
        int cross = 0; for (size_t i = 1; i < sq.size(); ++i) if (sq[i - 1] == 0.0f && sq[i] == 1.0f) ++cross; check ("gate: 4 Hz gives four openings per second", cross == 4 || cross == 3, "");
    }
    std::printf ("\n%d passed, %d failed\n", pass, fail); return fail ? 1 : 0;
}
