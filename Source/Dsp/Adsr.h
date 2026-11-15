#pragma once
#include <algorithm>

namespace rf {

struct Adsr
{
    enum Stage { Idle, Attack, Decay, Sustain, Release };

    void noteOn()  { stage = Attack; }
    void noteOff() { if (stage != Idle) { stage = Release; relStart = level; } }
    bool active() const { return stage != Idle; }

    // times in milliseconds
    inline float next (float aMs, float dMs, float s, float rMs, double sr)
    {
        switch (stage)
        {
            case Attack:
            {
                const float inc = aMs <= 0.01f ? 1.0f : (float) (1000.0 / (aMs * sr));
                level += inc;
                if (level >= 1.0f) { level = 1.0f; stage = Decay; }
                break;
            }
            case Decay:
            {
                const float dec = dMs <= 0.01f ? 1.0f : (float) ((1.0 - s) * 1000.0 / (dMs * sr));
                level -= dec;
                if (level <= s) { level = s; stage = Sustain; }
                break;
            }
            case Sustain: level = s; break;
            case Release:
            {
                const float dec = rMs <= 0.01f ? 1.0f : (float) (relStart * 1000.0 / (rMs * sr));
                level -= dec;
                if (level <= 0.0f) { level = 0.0f; stage = Idle; }
                break;
            }
            default: level = 0.0f; break;
        }
        return level;
    }

    Stage stage = Idle;
    float level = 0.0f, relStart = 0.0f;
};

} // namespace rf
