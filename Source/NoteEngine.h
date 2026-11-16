// Chord expander and arpeggiator: keys in, notes out, with sample-accurate timing.
// Pipeline:  played keys -> chord expansion -> arpeggiator (optional) -> synth voices.
#pragma once
#include <vector>
#include <array>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <string>

namespace rf {

struct NoteEvt { int64_t t = 0; bool on = false; int note = 60; float vel = 1.0f; };
struct RhythmCell { int type = 0; float velocity = 1.0f; }; // Hit, Rest, Tie

struct ChordArpSettings
{
    bool chordOn = false; int key = 0, scale = 0, type = 0, inversion = 0, voicing = 0; bool lock = true; float strumMs = 0.0f;
    bool arpOn = false, hold = false, run = true; int rate = 10, mode = 0, octaves = 1, order = 0, grouping = 0, wshape = 0, chordMode = 0, shape = 0; bool voiceLead = false; float gate = 0.5f, swing = 0.0f, human = 0.0f;
    bool rhythmOn = false; int rhythmLength = 16; std::array<RhythmCell, 16> rhythm {};
    // CUSTOM mode (arp_mode 8): a hand-made pattern of up to 64 steps (four 16-step bars). Each step is a note number in the held notes (0 = lowest, 1 = next,
    // wrapping upward an octave at a time; negative numbers wrap downward).
    int patLength = 16; std::array<int, 64> pattern {};
    // EVENTS mode (arp_mode 9): up to 64 steps, each its own event. Bit i of mask means 'held note number i' (as in CUSTOM, numbers past the last held note continue an octave up),
    // so one step can sound several notes together; mask 0 is a rest (notes already ringing are left alone). len is how many steps the event lasts (0: the normal gate), vel its level 1..127.
    int evLength = 16; std::array<int, 64> evMask {}, evLen {}, evVel {};
    // CUSTOM chord mode (chord_mode 2): one chord shape for each scale degree. Bit i of a mask means 'the root plus i semitones' is in the chord (bits 0 to 21).
    std::array<int, 7> degShape { 0x91, 0x89, 0x89, 0x91, 0x91, 0x89, 0x49 };   // the major scale's own triads: major, minor, minor, major, major, minor, diminished
    bool twoHand = false;   // 2-HAND: a slow low voice under the arp line (see NoteEngine::twoHandStep)
    // Relic's own design (the reference has no audible equivalent): CHANCE is the probability that a step plays (a dropped step is silent but the pattern still
    // moves on); SPRAY is the probability that a note that does play jumps one octave up or down at random.
    float chance = 1.0f, spray = 0.0f;
};

// Seven-note scales as semitone offsets from the key.
inline const int kScales[9][7] = {
    { 0, 2, 4, 5, 7, 9, 11 },   // Major
    { 0, 2, 3, 5, 7, 8, 10 },   // Minor
    { 0, 2, 3, 5, 7, 9, 10 },   // Dorian
    { 0, 1, 3, 5, 7, 8, 10 },   // Phrygian
    { 0, 2, 4, 6, 7, 9, 11 },   // Lydian
    { 0, 2, 4, 5, 7, 9, 10 },   // Mixolydian
    { 0, 1, 3, 5, 6, 8, 10 },   // Locrian
    { 0, 2, 3, 5, 7, 8, 11 },   // Harmonic minor
    { 0, 2, 3, 5, 7, 9, 11 } }; // Melodic minor

// FIXED mode: one chord shape, transposed under every key (24 shapes, intervals measured on a reference instrument).
inline const std::vector<int>& fixedShape (int i)
{
    static const std::vector<int> t[24] = { { 0, 4, 7 }, { 0, 3, 7 }, { 0, 4, 7, 11 }, { 0, 3, 7, 10 }, { 0, 4, 7, 10 }, { 0, 4, 7, 11, 14 }, { 0, 3, 7, 10, 14 }, { 0, 4, 7, 10, 14 },
        { 0, 4, 7, 11, 14, 17 }, { 0, 3, 7, 10, 14, 17 }, { 0, 4, 7, 11, 14, 17, 21 }, { 0, 2, 7 }, { 0, 5, 7 }, { 0, 4, 7, 14 }, { 0, 4, 7, 9 }, { 0, 3, 7, 9 }, { 0, 3, 6 }, { 0, 3, 6, 9 },
        { 0, 4, 8 }, { 0, 5, 7, 10, 14 }, { 0, 5, 7, 10, 14, 21 }, { 0, 4, 8, 10 }, { 0, 4, 8, 10, 15 }, { 0, 4, 7, 10, 13 } };
    return t[std::max (0, std::min (23, i))];
}
inline const char* fixedShapeName (int i)
{
    static const char* n[24] = { "", "m", "maj7", "m7", "7", "maj9", "m9", "9", "maj11", "m11", "maj13", "sus2", "sus4", "add9", "6", "m6", "dim", "dim7", "aug", "9sus4", "13sus4", "7#5", "7#5#9", "7b9" };
    return n[std::max (0, std::min (23, i))];
}

// Chord types, as scale-degree steps stacked on the root (0 = root, 2 = third, 4 = fifth, 6 = seventh ...)
inline std::vector<int> chordSteps (int type)
{
    switch (type)
    {
        case 1: return { 0, 1, 4 };            // Sus2
        case 2: return { 0, 3, 4 };            // Sus4
        case 3: return { 0, 2, 4, 6 };         // 7th
        case 4: return { 0, 2, 4, 6, 8 };      // 9th
        case 5: return { 0, 2, 4, 8 };         // Add9
        case 6: return { 0, 4, 7 };            // Power (root, fifth, octave)
        case 7: return { 0, 2, 4, 6, 8, 10 };           // 11th
        case 8: return { 0, 2, 4, 6, 8, 10, 12 };       // 13th
        default: return { 0, 2, 4 };           // Triad
    }
}

// The scale degree (0..6) a key belongs to, or -1 if it is outside the scale.
inline int degreeOf (int note, const ChordArpSettings& s)
{
    const int r = ((note % 12) - s.key + 12) % 12;
    for (int d = 0; d < 7; ++d) if (kScales[s.scale][d] == r) return d;
    return -1;
}

// Notes (low to high) that a played key becomes. With chords off, just the key.
inline std::vector<int> chordNotes (int note, const ChordArpSettings& s, int* degreeOut = nullptr)
{
    if (degreeOut) *degreeOut = -1;
    if (! s.chordOn) return { note };
    const auto& sc = kScales[s.scale];
    int d = degreeOf (note, s);
    int root = note;
    bool diatonic = d >= 0;
    if (! diatonic && s.lock)                         // snap an out-of-scale key down to the nearest scale tone
    {
        for (int k = 1; k < 12 && ! diatonic; ++k) { const int dd = degreeOf (note - k, s); if (dd >= 0) { d = dd; root = note - k; diatonic = true; } }
    }
    std::vector<int> out;
    if (s.chordMode == 1) { for (int iv : fixedShape (s.shape)) out.push_back (note + iv); }      // FIXED: the same shape under every key, no scale
    else if (diatonic && s.chordMode == 2)             // CUSTOM: the shape stored for this scale degree, built on the scale note
    {
        if (degreeOut) *degreeOut = d;
        for (int i = 0; i < 22; ++i) if (s.degShape[(size_t) d] & (1 << i)) out.push_back (root + i);
    }
    else if (diatonic)
    {
        if (degreeOut) *degreeOut = d;
        for (int step : chordSteps (s.type))
        {
            if (s.type == 6) { out.push_back (root + (step == 0 ? 0 : step == 4 ? 7 : 12)); continue; }
            const int k = d + step;
            out.push_back (root + (sc[k % 7] + 12 * (k / 7)) - sc[d]);
        }
    }
    else                                              // lock off and outside the scale: a plain major-type chord on the key
    {
        const std::vector<int> iv = s.type == 6 ? std::vector<int> { 0, 7, 12 } : s.type == 1 ? std::vector<int> { 0, 2, 7 } : s.type == 2 ? std::vector<int> { 0, 5, 7 }
                                  : s.type == 3 ? std::vector<int> { 0, 4, 7, 11 } : s.type == 4 ? std::vector<int> { 0, 4, 7, 11, 14 } : s.type == 5 ? std::vector<int> { 0, 4, 7, 14 } : s.type == 7 ? std::vector<int> { 0, 4, 7, 11, 14, 17 } : s.type == 8 ? std::vector<int> { 0, 4, 7, 11, 14, 17, 21 } : std::vector<int> { 0, 4, 7 };
        for (int i : iv) out.push_back (note + i);
    }
    std::sort (out.begin(), out.end());
    for (int i = 0; i < s.inversion && out.size() > 1; ++i) { const int lo = out.front(); out.erase (out.begin()); out.push_back (lo + 12); }
    if (out.size() >= 3)
    {
        if (s.voicing == 1) out[1] += 12;                                  // Open: second note up an octave
        else if (s.voicing == 2) out[out.size() - 2] -= 12;                // Drop 2: second from the top down an octave
        else if (s.voicing == 3) { out[0] -= 12; out[2] += 12; }           // Spread: bass down, third voice up
        std::sort (out.begin(), out.end());
    }
    for (auto& n : out) n = std::max (0, std::min (127, n));
    out.erase (std::unique (out.begin(), out.end()), out.end());
    return out;
}

inline const char* noteName (int pc)
{
    static const char* n[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return n[((pc % 12) + 12) % 12];
}

// A short name for the chord a scale degree gives with the current settings, e.g. "Dm7".
inline std::string chordName (int degree, const ChordArpSettings& s)
{
    ChordArpSettings c = s; c.chordOn = true; c.inversion = 0; c.voicing = 0; c.lock = true;
    if (s.chordMode == 1) return std::string (noteName (s.key + kScales[s.scale][degree])) + fixedShapeName (s.shape);
    if (s.chordMode == 2)      // CUSTOM: name the stored shape from its intervals
    {
        const int m = s.degShape[(size_t) degree]; auto has = [&] (int iv) { return (m & (1 << iv)) != 0 || (m & (1 << (iv + 12))) != 0; };
        std::string n = noteName (s.key + kScales[s.scale][degree]);
        if (m == 0) return n + "?";
        const bool minor3 = has (3), major3 = has (4), dim5 = has (6) && ! has (7), aug5 = has (8) && ! has (7);
        if (! minor3 && ! major3) n += has (5) ? "sus4" : has (2) ? "sus2" : (has (7) ? "5" : "");
        else if (minor3 && dim5) n += "dim"; else if (minor3) n += "m"; else if (major3 && aug5) n += "aug";
        if (has (11)) n += "maj7"; else if (has (10)) n += "7";
        if (m & (1 << 14)) n += "9"; return n;
    }
    const int base = 60 + ((s.key + kScales[s.scale][degree]) % 12);
    auto notes = chordNotes (base, c);
    if (notes.empty()) return "";
    std::string name = noteName (notes[0]);
    auto has = [&] (int iv) { for (int n : notes) if ((n - notes[0]) % 12 == iv) return true; return false; };
    const bool minor3 = has (3), major3 = has (4), dim5 = has (6) && ! has (7), aug5 = has (8) && ! has (7);
    if (s.type == 6) return name + "5"; if (s.type == 7) return name + (minor3 ? "m11" : "11"); if (s.type == 8) return name + (minor3 ? "m13" : "13");
    if (s.type == 1) return name + "sus2"; if (s.type == 2) return name + "sus4";
    if (minor3 && dim5) name += "dim"; else if (minor3) name += "m"; else if (major3 && aug5) name += "aug";
    if (s.type == 3) name += has (11) ? "maj7" : "7"; if (s.type == 4) name += has (11) ? "maj9" : "9"; if (s.type == 5) name += "add9";
    if (minor3 && s.type == 3 && has (10)) { /* m7 already reads as "m" + "7" */ }
    return name;
}

// Waterfall: which of the L notes (low to high) plays on a given step. Patterns measured on a reference instrument.
//  Cascade: 0 1 2 3, then the next four up-positions backwards (7 6 5 4), repeating every 8 steps.
//  Spiral 0 1 2 3 3 4 5 6   Geyser (0 1 2 3)(2 3 4 5)(2 3 4 5)   Twin 0 1 2 3 1 2 3 4   Tide: plain up and back down   Climb: 0..7 up, restarting every 8 steps
//  With a grouping, each group restarts the cascade phrase, cut to the group length (shape is ignored then).
// Where a step falls inside its group. The groupings (5-5-6, 7-7-2, 3-3-3-3-4, 4-4-4-4) each fill one 16-step bar.
inline int groupStep (int grouping, int step)
{
    static const int g[4][5] = { { 5, 5, 6, 0, 0 }, { 7, 7, 2, 0, 0 }, { 3, 3, 3, 3, 4 }, { 4, 4, 4, 4, 0 } };
    int j = step % 16, start = 0;
    for (int k = 0; k < 5 && g[grouping - 1][k] > 0; ++k) { if (j < start + g[grouping - 1][k]) return j - start; start += g[grouping - 1][k]; }
    return j;
}

inline int waterfallPos (const ChordArpSettings& s, int step, int L)
{
    L = std::max (1, L); int p = 0;
    if (s.grouping > 0)
    {
        const int j = groupStep (s.grouping, step);
        p = j < 4 ? j : 11 - j;
    }
    else switch (s.wshape)
    {
        case 1: { const int j = step % 8; p = j < 4 ? j : j - 1; break; }
        case 2: { const int b = (step / 4) % 3, j = step % 4; p = b == 0 ? j : j + 2; break; }
        case 3: { const int j = step % 8; p = j < 4 ? j : j - 3; break; }
        case 4: { const int cyc = std::max (1, 2 * L - 2), k = step % cyc; p = k < L ? k : cyc - k; break; }
        case 5: p = step % 8; break;
        default: { const int j = step % 8; p = j < 4 ? j : 11 - j; break; }
    }
    return p % L;
}

class NoteEngine
{
public:
    void reset (int64_t t, std::vector<NoteEvt>& out)
    {
        for (int n = 0; n < 128; ++n) if (cnt[(size_t) n] > 0) out.push_back ({ t, false, n, 0.0f });
        cnt.fill (0); held.clear(); pending.clear(); tiedNotes.clear(); for (auto& v : soundingFor) v.clear(); arpActive = false; stopTail = false; step = 0; patternStep = 0; cycleSteps = 0;
        clearLowVoice();
    }

    // Process one block. 'in' holds this block's note events (absolute sample times, sorted). Output events are appended to 'out'.
    void process (int64_t t0, int N, const std::vector<NoteEvt>& in, const ChordArpSettings& s, double bpm, double ppq, bool playing, double sr, std::vector<NoteEvt>& out)
    {
        lastSr = sr;
        if (! s.rhythmOn) releaseTie (t0, out);
        const int64_t t1 = t0 + N;
        const bool resume = s.run && ! lastRun;
        // GO -> STOP finishes the current pass, including its last note's gate.
        if (! s.run && lastRun && arpActive && patternStep > 0 && cycleSteps > 0 && patternStep % cycleSteps == 0)
        {
            const int length = std::max (1, std::min (16, s.rhythmLength));
            if (s.rhythmOn && ! tiedNotes.empty() && s.rhythm[(size_t) (step % length)].type == 2)
            { stopTail = true; tailSteps = 0; }
            else
            {
                arpActive = false;
                scheduleTieOff (std::max (t0, (int64_t) std::llround (stepTime (s))));
            }
        }
        lastRun = s.run;
        int key = (s.chordOn ? 1 : 0) | (s.arpOn ? 2 : 0) | (s.key << 2) | (s.scale << 6) | (s.type << 10) | (s.inversion << 14) | (s.voicing << 17) | (s.lock ? 1 << 20 : 0) | (s.chordMode << 21) | (s.shape << 22) | (s.voiceLead ? 1 << 27 : 0);
        if (s.chordMode == 2) { uint32_t h = 2166136261u; for (int m : s.degShape) h = (h ^ (uint32_t) m) * 16777619u; key = (int) (((uint32_t) key * 31u + h) & 0x7fffffffu); }   // editing a Custom shape restarts the chord
        if (key != modeKey || (s.arpOn && resume))
        {
            const bool first = (modeKey < 0); modeKey = key; prevVoicing.clear();
            if (! first) restart (t0, s, bpm, ppq, playing, sr, out);
        }
        size_t idx = 0;
        for (;;)
        {
            const int64_t ti = idx < in.size() ? in[idx].t : INT64_MAX;
            int64_t tp = INT64_MAX; for (auto& e : pending) tp = std::min (tp, e.t);
            const int64_t ts = arpActive ? (int64_t) std::llround (stepTime (s)) : INT64_MAX;
            const int64_t t = std::min ({ ti, tp, lowOffT, ts });
            if (t >= t1) break;
            if (t == ti) { const auto& e = in[idx++]; if (e.on) handleOn (e, s, bpm, ppq, playing, sr, out); else handleOff (e, s, out); }
            else if (t == tp) emitPending (tp, out);
            else if (t == lowOffT) { off (t, lowNote, out); lowOffT = INT64_MAX; lowActive = false; }
            else arpStep (t, s, bpm, sr, out);
        }
    }

    bool isArpRunning() const { return arpActive; }
    bool isArpWaiting() const { return ! arpActive && pending.empty() && tiedNotes.empty(); }
    int currentStep() const { return step; }
    int lastDegree() const { return lastDeg; }
    int heldCount() const { return (int) held.size(); }

private:
    struct Held { int note; float vel; uint64_t order; bool physical; };
    std::vector<Held> held;
    std::array<int, 128> cnt {};
    std::array<std::vector<int>, 128> soundingFor;      // which notes each played key turned on (chord, no arp)
    std::vector<NoteEvt> pending;
    std::vector<int> tiedNotes;
    bool stopTail = false; int tailSteps = 0;
    uint64_t orderCounter = 0; int modeKey = -1;
    bool arpActive = false, lastRun = true; double stepBase = 0.0; int step = 0, patternStep = 0, cycleSteps = 0; bool swingPending = false; int lastDeg = -1; uint32_t rng = 12345u;

    // 14 divisions: 1/1 1/2 1/2D 1/2T 1/4 1/4D 1/4T 1/8 1/8D 1/8T 1/16 1/16D 1/16T 1/32 (beats are quarter notes; D = dotted x1.5, T = triplet x2/3)
    static double rateBeats (int r) { static const double b[14] = { 4.0, 2.0, 3.0, 4.0 / 3.0, 1.0, 1.5, 2.0 / 3.0, 0.5, 0.75, 1.0 / 3.0, 0.25, 0.375, 1.0 / 6.0, 0.125 }; return b[std::max (0, std::min (13, r))]; }
    static int effRate (const ChordArpSettings& s) { return s.grouping != 0 && s.mode == 7 ? 10 : s.rate; }       // a grouping locks Waterfall to 1/16, as the reference does
    static double stepLen (const ChordArpSettings& s, double bpm, double sr) { return 60.0 / std::max (20.0, bpm) * sr * rateBeats (effRate (s)); }
    double stepTime (const ChordArpSettings& s) const { return stepBase + ((step & 1) ? swingDelay : 0.0) + humanOff; }
    double swingDelay = 0.0, humanOff = 0.0; std::vector<int> perm;

    // 2-HAND low voice. Measured on a reference instrument (v1.5.1) (a local probe script the arp line is untouched; every 4th step (from the first)
    // one extra note sounds, one octave below a held chord tone. The tones are taken lowest to highest and cycle (C E G B C E ...), whatever the
    // arp mode, order or octave range. It is held for about 95% of the 4 steps at a steady level, about 3 dB under the arp note.
    bool lowActive = false; int lowNote = 0, lowCount = 0; int64_t lowOffT = INT64_MAX;
    static constexpr int kLowEvery = 4; static constexpr double kLowHold = 0.95; static constexpr float kLowLevel = 0.72f;
    void clearLowVoice() { lowActive = false; lowOffT = INT64_MAX; lowCount = 0; }
    void twoHandStep (int64_t t, const std::vector<std::pair<int, float>>& tones, double len, std::vector<NoteEvt>& out)
    {
        if (tones.empty() || step % kLowEvery != 0) return;
        if (lowActive) { off (t, lowNote, out); lowActive = false; }
        const auto& tone = tones[(size_t) (lowCount++ % (int) tones.size())];
        lowNote = std::max (0, tone.first - 12);
        on (t, lowNote, std::max (0.02f, std::min (1.0f, tone.second * kLowLevel)), out);
        lowActive = true; lowOffT = t + (int64_t) std::llround (len * kLowEvery * kLowHold);
    }
    float rnd01() { rng = rng * 1664525u + 1013904223u; return (float) (rng >> 8) / 16777216.0f; }

    void on (int64_t t, int note, float vel, std::vector<NoteEvt>& out) { if (cnt[(size_t) note]++ == 0) out.push_back ({ t, true, note, vel }); else { out.push_back ({ t, false, note, 0.0f }); out.push_back ({ t, true, note, vel }); } }
    void off (int64_t t, int note, std::vector<NoteEvt>& out) { if (cnt[(size_t) note] > 0 && --cnt[(size_t) note] == 0) out.push_back ({ t, false, note, 0.0f }); }

    void releaseTie (int64_t t, std::vector<NoteEvt>& out)
    {
        for (int n : tiedNotes) off (t, n, out);
        tiedNotes.clear();
    }
    void scheduleTieOff (int64_t t)
    {
        for (int n : tiedNotes) pending.push_back ({ t, false, n, 0.0f });
        tiedNotes.clear();
    }

    void emitPending (int64_t t, std::vector<NoteEvt>& out)
    {
        for (size_t i = 0; i < pending.size();)
            if (pending[i].t == t) { const auto e = pending[i]; pending.erase (pending.begin() + (long) i); if (e.on) on (t, e.note, e.vel, out); else off (t, e.note, out); }
            else ++i;
    }

    void startArp (int64_t t, const ChordArpSettings& s, double bpm, double ppq, bool playing, double sr)
    {
        arpActive = true; stopTail = false; tailSteps = 0; step = 0; patternStep = 0; cycleSteps = 0; lowCount = 0; swingDelay = s.swing * 0.5 * stepLen (s, bpm, sr); humanOff = 0.0; perm.clear();   // swing 1.0 pushes every other step half a step late
        double start = (double) t;
        if (playing && ppq >= 0.0)                         // line the first step up with the host's beat grid
        {
            const double b = rateBeats (effRate (s)), nextGrid = std::ceil (ppq / b - 1.0e-6) * b;
            start += (nextGrid - ppq) * 60.0 / std::max (20.0, bpm) * sr;
        }
        else start += 0.025 * sr;                          // wait 25 ms for the rest of a chord (the reference does the same), so the first step sees every key
        stepBase = start;
    }

    void restart (int64_t t, const ChordArpSettings& s, double bpm, double ppq, bool playing, double sr, std::vector<NoteEvt>& out, bool fresh = false)
    {
        const bool start = s.run || arpActive || fresh;
        for (int n = 0; n < 128; ++n) if (cnt[(size_t) n] > 0) out.push_back ({ t, false, n, 0.0f });
        cnt.fill (0); pending.clear(); tiedNotes.clear(); for (auto& v : soundingFor) v.clear(); arpActive = false; stopTail = false; clearLowVoice();
        if (held.empty()) return;
        if (s.arpOn) { if (start) startArp (t, s, bpm, ppq, playing, sr); }
        else for (auto& h : held) playChord (t, h.note, h.vel, s, out);
    }

    void playChord (int64_t t, int note, float vel, const ChordArpSettings& s, std::vector<NoteEvt>& out)
    {
        int deg = -1; auto notes = chordNotes (note, s, &deg); if (deg >= 0) lastDeg = deg;
        if (s.voiceLead) notes = leadVoicing (notes, note); else prevVoicing.clear();
        soundingFor[(size_t) note] = notes;
        const double strum = s.strumMs * 0.001 * lastSr;
        for (size_t i = 0; i < notes.size(); ++i)
        {
            const int64_t ti = t + (int64_t) std::llround (strum * (double) i);
            if (i == 0 || strum <= 0.0) on (ti, notes[i], vel, out); else pending.push_back ({ ti, true, notes[i], vel });
        }
    }
    double lastSr = 44100.0;
    std::vector<int> prevVoicing;
    // Voice leading: of all inversions and octave placements of this chord, take the one that moves the voices least from the last chord
    // (sum of semitone moves, matched low to high), keeping the lowest note no more than 3 semitones under the key pressed. Measured on a reference instrument.
    std::vector<int> leadVoicing (std::vector<int> notes, int key)
    {
        std::sort (notes.begin(), notes.end());
        if (prevVoicing.empty() || prevVoicing.size() != notes.size()) { prevVoicing = notes; return notes; }
        std::vector<int> best; int bestCost = 1 << 30; bool bestOk = false; auto stack = notes;
        for (size_t r = 0; r < notes.size(); ++r)
        {
            if (r > 0) { const int lo = stack.front(); stack.erase (stack.begin()); stack.push_back (lo + 12); std::sort (stack.begin(), stack.end()); }
            for (int m = -60; m <= 60; m += 12)
            {
                auto c = stack; for (auto& v : c) v += m;
                if (c.front() < 0 || c.back() > 127) continue;
                int cost = 0; for (size_t i = 0; i < c.size(); ++i) cost += std::abs (c[i] - prevVoicing[i]);
                const bool ok = c.front() >= key - 3;
                if ((ok && ! bestOk) || (ok == bestOk && (cost < bestCost || (cost == bestCost && c.front() < best.front())))) { best = c; bestCost = cost; bestOk = ok; }
            }
        }
        if (best.empty()) best = notes;
        prevVoicing = best; return best;
    }

    void handleOn (const NoteEvt& e, const ChordArpSettings& s, double bpm, double ppq, bool playing, double sr, std::vector<NoteEvt>& out)
    {
        lastSr = sr;
        const bool fresh = std::none_of (held.begin(), held.end(), [] (const Held& h) { return h.physical; });
        if (s.arpOn && s.hold)
        {
            bool anyPhysical = false; for (auto& h : held) anyPhysical |= h.physical;
            if (! anyPhysical) held.clear();                                   // a new gesture replaces the latched notes
        }
        held.erase (std::remove_if (held.begin(), held.end(), [&] (const Held& h) { return h.note == e.note; }), held.end());
        held.push_back ({ e.note, e.vel, orderCounter++, true });
        if (s.arpOn)
        {
            // STOP consumes a gesture once. Adding/repressing a key while another
            // is down cannot re-arm it; all physical keys must first be released.
            if (! s.run && fresh) restart (e.t, s, bpm, ppq, playing, sr, out, true);
            else if (s.run && ! arpActive) startArp (e.t, s, bpm, ppq, playing, sr);
        }
        else playChord (e.t, e.note, e.vel, s, out);
    }

    void handleOff (const NoteEvt& e, const ChordArpSettings& s, std::vector<NoteEvt>& out)
    {
        if (s.arpOn)
        {
            for (auto it = held.begin(); it != held.end(); ++it)
                if (it->note == e.note) { if (s.hold) it->physical = false; else { held.erase (it); } break; }
            if (held.empty()) { for (int n = 0; n < 128; ++n) if (cnt[(size_t) n] > 0) { cnt[(size_t) n] = 1; off (e.t, n, out); } pending.clear(); tiedNotes.clear(); arpActive = false; stopTail = false; clearLowVoice(); }
            return;
        }
        const auto mine = soundingFor[(size_t) e.note];
        for (int n : mine) off (e.t, n, out);
        soundingFor[(size_t) e.note].clear();
        pending.erase (std::remove_if (pending.begin(), pending.end(), [&] (const NoteEvt& p) { return p.on && std::find (mine.begin(), mine.end(), p.note) != mine.end(); }), pending.end());   // cancel this key's strum notes not yet played
        held.erase (std::remove_if (held.begin(), held.end(), [&] (const Held& h) { return h.note == e.note; }), held.end());
    }

    void arpStep (int64_t t, const ChordArpSettings& s, double bpm, double sr, std::vector<NoteEvt>& out)
    {
        // notes to cycle through: every held key, expanded by the chord engine, across the octave range
        std::vector<std::pair<int, float>> base;
        std::vector<Held> order = held; const bool played = s.mode == 5 || s.order == 1; if (! played) std::sort (order.begin(), order.end(), [] (const Held& a, const Held& b) { return a.note < b.note; });
        else std::sort (order.begin(), order.end(), [] (const Held& a, const Held& b) { return a.order < b.order; });
        for (auto& h : order) for (int n : chordNotes (h.note, s)) { bool dup = false; for (auto& b : base) dup |= (b.first == n); if (! dup) base.push_back ({ n, h.vel }); }
        if (! played) std::sort (base.begin(), base.end());
        if (base.empty()) { arpActive = false; return; }
        std::vector<std::pair<int, float>> pat;
        for (int o = 0; o < std::max (1, s.octaves); ++o) for (auto& b : base) if (b.first + 12 * o <= 127) pat.push_back ({ b.first + 12 * o, b.second });
        const int L = (int) pat.size(); const double len = stepLen (s, bpm, sr);
        // In the plain modes a grouping (Relic's own design: the reference's behaviour there could not be measured cleanly) cuts the line into the group
        // lengths and restarts the pattern at the top of every group, so a pass is one 16-step bar. Chord mode and Waterfall keep their own rules.
        const bool grouped = s.grouping > 0 && s.mode != 6 && s.mode != 7 && s.mode != 8 && s.mode != 9;
        const int customLen = std::max (1, std::min (64, s.patLength));
        cycleSteps = s.mode == 6 ? 1 : s.mode == 8 ? customLen : s.mode == 9 ? std::max (1, std::min (64, s.evLength)) : s.mode == 7 ? (s.grouping > 0 ? 16 : 8) : grouped ? 16
                     : (s.mode == 2 || s.mode == 3) ? std::max (1, 2 * L - 2) : L;
        const int ps = grouped ? groupStep (s.grouping, patternStep) : patternStep;   // pattern position: restarts in every group
        const int laneLength = std::max (1, std::min (16, s.rhythmLength));
        const auto cell = s.rhythmOn && s.mode != 9 ? s.rhythm[(size_t) (step % laneLength)] : RhythmCell {};
        if (stopTail && cell.type != 2)
        { releaseTie (t, out); arpActive = false; stopTail = false; return; }
        const bool hit = cell.type == 0;
        const bool tieNext = s.rhythmOn && s.mode != 9 && s.rhythm[(size_t) ((step + 1) % laneLength)].type == 2;
        if (s.order >= 2 && s.mode != 6 && s.mode != 8 && s.mode != 9 && L > 1 && hit) // reorder only when a pattern note will actually be consumed
        {
            std::vector<std::pair<int, float>> src = pat; std::vector<int> idx;
            if (s.order == 2) { int lo = 0, hi = L - 1; while (lo <= hi) { idx.push_back (lo++); if (lo <= hi) idx.push_back (hi--); } }
            else if (s.order == 3) { const int mid = L / 2; idx.push_back (mid); for (int k = 1; (int) idx.size() < L; ++k) { if (mid - k >= 0) idx.push_back (mid - k); if (mid + k < L) idx.push_back (mid + k); } }
            else { if ((int) perm.size() != L || patternStep % L == 0) { perm.resize ((size_t) L); for (int k = 0; k < L; ++k) perm[(size_t) k] = k; for (int k = L - 1; k > 0 && patternStep % L == 0; --k) std::swap (perm[(size_t) k], perm[(size_t) (rnd01() * (k + 1)) % (size_t) (k + 1)]); } idx = perm; }
            for (int k = 0; k < L; ++k) pat[(size_t) k] = src[(size_t) idx[(size_t) k]];
        }
        std::vector<std::pair<int, float>> play; int evLen = 0; float evVel = -1.0f;
        if (s.mode == 6) play = pat;
        else if (s.mode == 9)       // EVENTS: every note whose bit is set sounds together, at this step's own level and length
        {
            const int k = patternStep % std::max (1, std::min (64, s.evLength)), nb = (int) base.size(), mask = s.evMask[(size_t) k];
            evLen = std::max (0, std::min (16, s.evLen[(size_t) k])); evVel = (float) std::max (1, std::min (127, s.evVel[(size_t) k])) / 127.0f;
            for (int d = 0; d < 12 && nb > 0; ++d) if (mask & (1 << d))
            {
                int note = base[(size_t) (d % nb)].first + 12 * (d / nb);
                while (note > 127) note -= 12;
                bool dup = false; for (auto& q : play) dup |= (q.first == note); if (! dup) play.push_back ({ note, base[(size_t) (d % nb)].second });
            }
        }
        else if (s.mode == 8)       // CUSTOM: the step's note number picks a held note; whole turns of the held-note list shift the octave
        {
            const int nb = (int) base.size(), d = s.pattern[(size_t) (patternStep % customLen)];
            const int q = (d >= 0 ? d / nb : -((-d + nb - 1) / nb)), r = d - q * nb;
            int note = base[(size_t) r].first + 12 * q;
            while (note > 127) note -= 12;
            while (note < 0) note += 12;
            play.push_back ({ note, base[(size_t) r].second });
        }
        else
        {
            int i = 0; const int cyc = std::max (1, 2 * L - 2);
            switch (s.mode)
            {
                case 1: i = L - 1 - (ps % L); break;                                                   // Down
                case 2: { const int k = ps % cyc; i = k < L ? k : cyc - k; break; }                    // Up-Down
                case 3: { const int k = ps % cyc; i = L - 1 - (k < L ? k : cyc - k); break; }          // Down-Up
                case 4: if (hit) { rng = rng * 1664525u + 1013904223u; i = (int) ((rng >> 8) % (uint32_t) L); } break; // Random
                case 7: i = waterfallPos (s, patternStep, L); break;                                            // Waterfall
                default: i = ps % L; break;                                                            // Up, Played
            }
            play.push_back (pat[(size_t) std::max (0, std::min (L - 1, i))]);
        }
        // Measured Waterfall curve: 1/8..6 steps, quadratic in the gate knob.
        // Plain modes keep the existing gate law and full-gate overlap.
        const double gateDuration = s.mode == 7 ? len * (0.125 + 5.875 * s.gate * s.gate)
                                     : s.gate >= 0.999f ? len + 0.008 * sr : s.gate * len;
        const int64_t offT = t + (int64_t) std::llround (gateDuration + (evLen > 1 ? (evLen - 1) * len : 0.0));
        // HUMAN: a little level give on every step, and now and then a ghost note (very quiet) once it is turned well up
        float vm = 1.0f + (rnd01() - 0.5f) * 0.2f * s.human;
        if (s.human > 0.6f && rnd01() < (s.human - 0.6f) * 0.6f) vm *= 0.2f;
        if (cell.type != 2) releaseTie (t, out);
        if (cell.type == 1)
        {
            // Waterfall's long gates ring through rests; a rest adds no new hit.
            // Plain modes cut the legacy 8 ms full-gate overlap at the boundary.
            if (s.mode != 7)
            {
                // the 2-HAND low voice is slow and runs on its own clock, so a rest does not cut it
                for (int n = 0; n < 128; ++n) if (cnt[(size_t) n] > 0 && ! (lowActive && n == lowNote)) { out.push_back ({ t, false, n, 0.0f }); cnt[(size_t) n] = 0; }
                if (lowActive) cnt[(size_t) lowNote] = 1;
                pending.clear();
            }
        }
        else if (cell.type == 0)
        {
            // CHANCE: a dropped step is silent, but patternStep still advances below so the phrase keeps its place
            const bool dropped = s.chance < 0.999f && rnd01() >= s.chance;
            if (! dropped) for (auto& p : play)
            {
                // Measured reference lane floor: even a stored zero is a 6% hit.
                const float velocity = std::max (0.02f, std::min (1.0f, (evVel >= 0.0f ? evVel : p.second) * vm)) * std::max (0.06f, std::min (1.0f, cell.velocity));
                int note = p.first;
                if (s.spray > 0.0f && rnd01() < s.spray)          // SPRAY: this note jumps an octave, up or down
                {
                    const int jump = rnd01() < 0.5f ? 12 : -12;
                    if (note + jump >= 0 && note + jump <= 127) note += jump;
                    else if (note - jump >= 0 && note - jump <= 127) note -= jump;
                }
                on (t, note, velocity, out);
                if (tieNext) tiedNotes.push_back (note);
                else pending.push_back ({ offT, false, note, 0.0f });
            }
        }
        else if (cell.type == 2 && ! stopTail && ! tieNext)
        {
            // Ordinary ties carry through whole intermediate cells, then use
            // the gate point of the last tied cell (measured on a reference instrument).
            scheduleTieOff (offT);
        }
        if (s.twoHand) { auto tones = base; std::sort (tones.begin(), tones.end()); twoHandStep (t, tones, len, out); }
        // Rests/ties retain the next pitch: they never skip a pattern note.
        if (hit) ++patternStep;
        // advance the rhythm clock (the tempo may have changed since the last one)
        stepBase += len; ++step; swingDelay = s.swing * 0.5 * len;
        // Keep pending note-offs alive after the final hit. A new physical
        // gesture (or GO) is the only way out of the completed STOP pass.
        if (! s.run && hit && patternStep % cycleSteps == 0)
        {
            // STOP counts emitted hits, not rhythm slots. It then carries the
            // final hit through any immediately following ties to their end.
            if (tieNext) { stopTail = true; tailSteps = 0; }
            else arpActive = false;
        }
        else if (stopTail && cell.type == 2 && (! tieNext || ++tailSteps >= laneLength))
        {
            arpActive = false; stopTail = false;
            scheduleTieOff (std::max (t + 1, (int64_t) std::llround (stepBase + ((step & 1) ? swingDelay : 0.0))));
        }
        // HUMAN timing: steps lean slightly ahead and scatter around the beat (scatter of 4 ms at 0.5 and 8 ms at 1.0, as measured on a reference instrument)
        humanOff = s.human > 0.0f ? ((double) (-3.0f * s.human) + (rnd01() + rnd01() + rnd01() - 1.5f) * 2.0f * 8.0f * s.human) * 0.001 * sr : 0.0;
    }
};

} // namespace rf
