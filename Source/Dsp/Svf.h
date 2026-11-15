// Two-pole state-variable filter (topology-preserving transform).
#pragma once
#include <cmath>
#include <algorithm>

namespace rf {

struct Svf
{
    enum Type { LowPass = 0, HighPass = 1, BandPass = 2, Notch = 3 };

    void setup (float cutoffHz, float q, double sampleRate)
    {
        const float c = std::min (cutoffHz, (float) (sampleRate * 0.49));
        g = std::tan (3.14159265358979f * c / (float) sampleRate);
        k = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    inline float process (float v0, int type)
    {
        const float v3 = v0 - s2;
        const float v1 = a1 * s1 + a2 * v3;
        const float v2 = s2 + a2 * s1 + a3 * v3;
        s1 = 2.0f * v1 - s1;
        s2 = 2.0f * v2 - s2;
        switch (type)
        {
            case LowPass:  return v2;
            case HighPass: return v0 - k * v1 - v2;
            case BandPass: return k * v1;       // unity gain at the centre
            default:       return v0 - k * v1;  // notch
        }
    }

    void reset() { s1 = s2 = 0.0f; }

    float g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0, s1 = 0, s2 = 0;
};

// Resonance knob (0..1) to Q, taken from measured behavior: Q = 0.707 + 5.2 * reso.
inline float resonanceToQ (float reso) { return 0.7071f + 5.2f * reso; }

} // namespace rf
