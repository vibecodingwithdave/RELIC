// Automation lanes: eight independent generators, each with its own shape, drawn cycle, timing, depth and one target, plus one separate
// global generator ("fan-out LFO") that can feed eleven targets at once. Lanes are global (not per voice). Header-only, no JUCE.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include "Mod.h"

namespace rf {

constexpr int kAutoLanes = 8;
constexpr int kAutoPts = kLfoPts;        // a drawn lane cycle uses the same 32 points as the matrix LFOs: stored 0..1, 0.5 = zero

enum AutoShape { AsDraw = 0, AsSine, AsTri, AsSaw, AsSquare, AsSampleHold, AsEnv, kAutoShapes };

// What a lane can drive. Index 0 is None. The order is part of the saved state: only ever append.
enum AutoTarget
{
    AtNone = 0, AtCutoff, AtReso, AtPitch, AtPan, AtSynthPitch, AtGroupPitch, AtGroupDetune, AtSynthVol, AtSampleVol, AtSamplePitchA, AtSamplePitchB,
    AtTextureLevel, AtOscAPos, AtOscBPos, AtOscCPos, AtOscMix, AtMaster, AtReverbMix, AtReverbSize, AtDelayMix, AtChorusMix, AtFlangerMix,
    AtTremDepth, AtSatDrive, AtWahFreq, AtWidth, AtPitchDelayMix, AtLofiMix, AtSGateMix, AtTalkShape, AtCrushMix,
    AtLane1Depth, AtLane2Depth, AtLane3Depth, AtLane4Depth, AtGateDepth, kAutoTargets
};

// How much one unit of (depth x output x intensity) moves each target, in the unit the engine applies it in. Only some are measured against the reference.
//   exact = measured; approx = documented estimate, not calibrated.
struct AutoTargetInfo { const char* name; float scale; bool measured; };
inline const AutoTargetInfo& autoTargetInfo (int t)
{
    static const AutoTargetInfo info[kAutoTargets] = {
        { "None", 0.0f, false }, { "Cutoff", 1.0f, false }, { "Resonance", 1.0f, false }, { "Pitch", 1.0f, true }, { "Pan", 1.0f, true },
        { "Synth Pitch", 1.0f, true }, { "Group Pitch", 2.0f, true }, { "Group Detune", 1.0f, true }, { "Synth Volume", 2.3f, true }, { "Sample Volume", 1.0f, false },
        { "Sample A Pitch", 1.0f, false }, { "Sample B Pitch", 1.0f, false }, { "Texture Level", 1.0f, false }, { "Osc A Position", 1.0f, false },
        { "Osc B Position", 1.0f, false }, { "Osc C Position", 1.0f, false }, { "Osc/Sample Mix", 1.0f, false }, { "Master Volume", 1.5f, true },
        { "Reverb Mix", 1.0f, false }, { "Reverb Size", 1.0f, false }, { "Delay Mix", 1.0f, false }, { "Chorus Mix", 1.0f, false }, { "Flanger Mix", 1.0f, false },
        { "Tremolo Depth", 1.0f, false }, { "Saturation Drive", 1.0f, false }, { "Wah Frequency", 1.0f, false }, { "Stereo Width", 1.0f, false },
        { "Pitch Delay Mix", 1.0f, false }, { "Lo-fi Mix", 1.0f, false }, { "Spectral Gate Mix", 1.0f, false }, { "Talk Shape", 1.0f, false },
        { "Crush Mix", 1.0f, false }, { "Lane 1 Depth", 1.0f, false }, { "Lane 2 Depth", 1.0f, false }, { "Lane 3 Depth", 1.0f, false }, { "Lane 4 Depth", 1.0f, false }, { "Gate Depth", 1.0f, false } };
    return info[t < 0 ? 0 : (t >= kAutoTargets ? 0 : t)];
}
inline bool autoIsDepthTarget (int t) { return t >= AtLane1Depth && t <= AtLane4Depth; }

// The eleven destinations of the fan-out generator, in its bit order.
inline int legacyTarget (int bit)
{
    static const int t[11] = { AtCutoff, AtReso, AtDelayMix, AtReverbMix, AtChorusMix, AtFlangerMix, AtWahFreq, AtCrushMix, AtWidth, AtTremDepth, AtOscMix };
    return t[bit < 0 ? 0 : (bit > 10 ? 10 : bit)];
}

// Timing. Division 0..6 = 1 bar, 1/2, 1/4, 1/8, 1/16, 2 bars, 4 bars (in quarter notes: 4, 2, 1, .5, .25, 8, 16); 7 = free (Hz).
inline double autoDivBeats (int div) { static const double b[7] = { 4.0, 2.0, 1.0, 0.5, 0.25, 8.0, 16.0 }; return div >= 0 && div < 7 ? b[div] : 0.0; }
inline double autoFeelMul (int feel) { return feel == 1 ? 1.5 : (feel == 2 ? 2.0 / 3.0 : 1.0); }       // straight, dotted (x1.5 long), triplet (x2/3 long)
inline double autoLaneHz (int div, int feel, double bpm, double freeHz)
{
    const double b = autoDivBeats (div);
    return b <= 0.0 ? freeHz : std::max (20.0, bpm) / 60.0 / (b * autoFeelMul (feel));
}

struct AutoLaneCfg
{
    bool on = false; int shape = AsSine; float depth = 0.0f; int dst = AtNone; int div = 2, feel = 0; float phase = 0.0f; bool trig = false;
    float envA = 10.0f, envD = 300.0f, hz = 1.0f; const float* pts = nullptr;
};
struct LegacyCfg { bool on = false; int shape = 0; double hz = 1.0; float depth = 0.0f; bool bit[11] {}; float amt[11] {}; };

struct AutoLane
{
    double ph = 0.0, envT = 0.0; float held = 0.3f; uint32_t rng = 0x2545F491u;
    void reset() { ph = 0.0; envT = 0.0; held = 0.3f; rng = 0x2545F491u; }
    void newHeld() { rng = rng * 1664525u + 1013904223u; held = (float) ((rng >> 8) / 8388608.0 - 1.0); if (held == 0.0f) held = 1e-4f; }
    void retrigger() { ph = 0.0; envT = 0.0; newHeld(); }
    float next (const AutoLaneCfg& c, double hz, double sr)
    {
        ph += hz / sr; bool wrap = false;
        if (ph >= 1.0) { ph -= std::floor (ph); wrap = true; }
        if (wrap) { newHeld(); if (! c.trig && c.shape == AsEnv) envT = 0.0; }
        double q = ph + (double) c.phase; q -= std::floor (q);
        switch (c.shape)
        {
            case AsDraw:       return c.pts ? lfoCustomEval (c.pts, q) : 0.0f;
            case AsTri:        return (float) (q < 0.5 ? 4.0 * q - 1.0 : 3.0 - 4.0 * q);
            case AsSaw:        return (float) (2.0 * q - 1.0);
            case AsSquare:     return q < 0.5 ? 1.0f : -1.0f;
            case AsSampleHold: return held;
            case AsEnv:
            {
                if (envT < 0.0) return 0.0f;
                const double a = std::max (0.0005, (double) c.envA * 0.001), d = std::max (0.001, (double) c.envD * 0.001), t = envT; envT += 1.0 / sr;
                if (t < a) return (float) (t / a);
                if (t < a + d) return (float) (1.0 - (t - a) / d);
                envT = -1.0; return 0.0f;
            }
            default:           return (float) std::sin (2.0 * 3.14159265358979323846 * q);
        }
    }
};

// All lanes and the fan-out generator for one block, written into one buffer per target (units: see autoTargetInfo).
struct AutoBank
{
    static constexpr int kMax = 16384;
    AutoLane lane[kAutoLanes]; ModLfo leg;
    std::vector<float> tgt; bool used[kAutoTargets] {};
    bool any = false;
    void prepare() { tgt.assign ((size_t) kAutoTargets * kMax, 0.0f); for (auto& l : lane) l.reset(); leg.reset(); }
    void reset() { for (auto& l : lane) l.reset(); leg.reset(); }
    void retrigger (const AutoLaneCfg* cfg) { for (int k = 0; k < kAutoLanes; ++k) if (cfg[k].on && cfg[k].trig) lane[k].retrigger(); }
    // Samples [off, off + n) of the block. Every lane that is on runs even when it has no target, so its phase stays put.
    void fill (const AutoLaneCfg* cfg, const LegacyCfg& lg, float heat, double bpm, double sr, int off, int n)
    {
        for (int t = 0; t < kAutoTargets; ++t) used[t] = false;
        any = false; if (tgt.empty()) return;
        n = std::min (n, kMax - off); if (n <= 0) return;
        bool any = false; for (int k = 0; k < kAutoLanes; ++k) if (cfg[k].on && cfg[k].dst > 0 && cfg[k].dst < kAutoTargets) { used[cfg[k].dst] = true; any = true; }
        if (lg.on) for (int b = 0; b < 11; ++b) if (lg.bit[b]) { used[legacyTarget (b)] = true; any = true; }
        if (! any) return;
        this->any = true;
        for (int t = 1; t < kAutoTargets; ++t) if (used[t] && ! autoIsDepthTarget (t)) std::fill (tgt.begin() + (size_t) t * kMax + off, tgt.begin() + (size_t) t * kMax + off + n, 0.0f);
        double hz[kAutoLanes]; for (int k = 0; k < kAutoLanes; ++k) hz[k] = autoLaneHz (cfg[k].div, cfg[k].feel, bpm, (double) cfg[k].hz);
        for (int i = 0; i < n; ++i)
        {
            float out[kAutoLanes] {}, add[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            for (int k = 0; k < kAutoLanes; ++k) if (cfg[k].on) out[k] = lane[k].next (cfg[k], hz[k], sr);
            for (int k = 0; k < kAutoLanes; ++k)          // lanes that drive another lane's depth: always on their own base depth, so chains and cycles are never followed
                if (cfg[k].on && autoIsDepthTarget (cfg[k].dst)) add[cfg[k].dst - AtLane1Depth] += out[k] * cfg[k].depth * heat;
            for (int k = 0; k < kAutoLanes; ++k)
            {
                const int t = cfg[k].dst; if (! cfg[k].on || t <= 0 || t >= kAutoTargets || autoIsDepthTarget (t)) continue;
                const float d = std::min (1.0f, std::max (0.0f, cfg[k].depth + (k < 4 ? add[k] : 0.0f)));
                tgt[(size_t) t * kMax + (size_t) (off + i)] += out[k] * d * heat * autoTargetInfo (t).scale;
            }
            if (lg.on)
            {
                const float x = leg.next (lg.hz, sr, lg.shape);
                for (int b = 0; b < 11; ++b) if (lg.bit[b]) { const int t = legacyTarget (b); tgt[(size_t) t * kMax + (size_t) (off + i)] += x * lg.depth * lg.amt[b] * autoTargetInfo (t).scale; }
            }
        }
    }
    inline float at (int t, int i) const { return used[t] ? tgt[(size_t) t * kMax + (size_t) (i < kMax ? i : kMax - 1)] : 0.0f; }
};

} // namespace rf
