#include "../Source/Dsp/Effects.h"
#include "../Source/Dsp/Effects2.h"
#include "../Source/Dsp/Mod.h"
#include <cstdio>
static int fails = 0;
static void check (const char* n, bool ok) { std::printf ("  [%s] %s\n", ok ? "PASS" : "FAIL", n); if (! ok) ++fails; }
static double frac (double x) { return x - std::floor (x); }
int main()
{
    const double sr = 44100.0;
    // ---- tremolo: with the lock the gain is an exact function of the host position, from any start, across a tempo change
    for (double ppq0 : { 0.0, 1.37, 100.123 })
    {
        rf::Tremolo t; double ppq = ppq0, bpm = 120.0, worst = 0.0; const double beats = 1.0;          // 1/4 note
        for (int blk = 0; blk < 200; ++blk)
        {
            if (blk == 100) bpm = 93.0;                                                                 // tempo change
            const double hz = bpm / 60.0 / beats; t.lockPhase (rf::barLockedPhase (ppq, beats, hz, sr));
            for (int i = 0; i < 512; ++i)
            {
                float l = 1.0f, r = 1.0f; t.process (l, r, (float) hz, 1.0f, sr);
                const double want = 1.0 - (0.5 + 0.5 * std::sin (2.0 * rf::kPi * frac ((ppq + i * bpm / 60.0 / sr) / beats)));
                worst = std::max (worst, std::abs (l - want));
            }
            ppq += 512 * bpm / 60.0 / sr;
        }
        char nm[96]; std::snprintf (nm, sizeof nm, "tremolo from ppq %.3f follows the bar exactly, also across a tempo change (max err %.1e)", ppq0, worst); check (nm, worst < 2e-5);
    }
    { // non-vacuous: without the lock the phase depends on where the plugin started
        rf::Tremolo a, b; double worst = 0.0; for (int i = 0; i < 2000; ++i) { float l1 = 1, r1 = 1, l2 = 1, r2 = 1; a.process (l1, r1, 2.0f, 1.0f, sr); if (i > 700) b.process (l2, r2, 2.0f, 1.0f, sr); else { float x = 1, y = 1; b.process (x, y, 2.0f, 1.0f, sr); } worst = std::max (worst, (double) std::abs (l1 - l2)); }
        check ("without the lock, two instances started at different times disagree", worst > 0.01);
    }
    // ---- a lock in the middle of a cycle: the cycle length does not matter (1/1 and 1/16 and 2/1 all line up)
    for (int idx : { 1, 5, 15, 9, 12 })
    {
        rf::Tremolo t; const double beats = rf::lfoSyncBeats (idx), bpm = 120.0, hz = rf::syncedHz (idx, bpm, 1.0); double ppq = 7.777, worst = 0.0;
        for (int blk = 0; blk < 50; ++blk)
        {
            t.lockPhase (rf::barLockedPhase (ppq, beats, hz, sr));
            for (int i = 0; i < 480; ++i) { float l = 1, r = 1; t.process (l, r, (float) hz, 1.0f, sr); worst = std::max (worst, std::abs ((double) l - (1.0 - (0.5 + 0.5 * std::sin (2.0 * rf::kPi * frac ((ppq + i * bpm / 60.0 / sr) / beats)))))); }
            ppq += 480 * bpm / 60.0 / sr;
        }
        char nm[80]; std::snprintf (nm, sizeof nm, "note value %d (%.3f beats) lines up (max err %.1e)", idx, beats, worst); check (nm, worst < 2e-5);
    }
    // ---- the other effects keep their phase where the lock puts it
    {
        const double beats = 0.5, bpm = 140.0, hz = bpm / 60.0 / beats, ppq = 12.3456; const double want = frac (ppq / beats);          // the first sample uses the phase of the block start
        rf::Panner pn; pn.lockPhase (rf::barLockedPhase (ppq, beats, hz, sr)); { float l = 1, r = 1; pn.process (l, r, (float) hz, 1.0f, 0, sr); } check ("panner phase", std::abs (frac (pn.lfo.phase - want + 0.5) - 0.5) < 1e-9);
        rf::Chorus ch; ch.prepare (sr); ch.lockPhase (rf::barLockedPhase (ppq, beats, hz, sr)); { float l = 1, r = 1; ch.process (l, r, (float) hz, 0.5f, 0.5f, sr); } check ("chorus phase", std::abs (frac (ch.phase - want + 0.5) - 0.5) < 1e-9);
        rf::Flanger fl; fl.prepare (sr); fl.lockPhase (rf::barLockedPhase (ppq, beats, hz, sr)); { float l = 1, r = 1; fl.process (l, r, (float) hz, 0.5f, 0.3f, 0.5f, sr); } check ("flanger phase", std::abs (frac (fl.phase - want + 0.5) - 0.5) < 1e-9);
        rf::Wah wh; wh.prepare (sr); wh.lockPhase (rf::barLockedPhase (ppq, beats, hz, sr)); { float l = 1, r = 1; wh.process (l, r, 0, (float) hz, 0.5f, 0.5f, 0.5f, sr); } check ("wah phase", std::abs (frac (wh.phase - want + 0.5) - 0.5) < 1e-9);
    }
    // ---- talk box: its LFO steps every 16th sample, so the lock has to account for where it is in that count
    for (int warm : { 0, 1, 5, 15, 16, 37 })
    {
        rf::TalkBox tb; tb.prepare (sr); for (int i = 0; i < warm; ++i) { float l = 0.1f, r = 0.1f; tb.process (l, r, 0.5f, 0.5f, 2.0f, 0.5f, 0.5f, sr); }
        const double beats = 1.0, bpm = 120.0, hz = 2.0, ppq = 3.21; const double bp = frac (ppq / beats); tb.lockPhase (bp, hz, sr);
        const int k = (16 - (warm & 15)) & 15; for (int i = 0; i <= k; ++i) { float l = 0.1f, r = 0.1f; tb.process (l, r, 0.5f, 0.5f, (float) hz, 0.5f, 0.5f, sr); }
        const double want = frac (bp + hz * k / sr); char nm[80]; std::snprintf (nm, sizeof nm, "talk box vowel LFO lines up after %d warm-up samples", warm); check (nm, std::abs (frac (tb.lfoPhase() - want + 0.5) - 0.5) < 2e-5);
        (void) bpm;
    }
    check ("barLockedPhase stays in 0..1", rf::barLockedPhase (0.0, 1.0, 2.0, 44100.0) >= 0.0 && rf::barLockedPhase (0.0, 1.0, 2.0, 44100.0) < 1.0 && rf::barLockedPhase (1e6, 16.0, 0.02, 44100.0) < 1.0);
    std::printf ("%s\n", fails ? "FAILED" : "all pass"); return fails ? 1 : 0;
}
