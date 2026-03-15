#pragma once

#include <algorithm>
#include <cmath>

class StateVariableLowPass
{
public:
    void prepare(double newSampleRate)
    {
        sampleRate = newSampleRate > 1.0 ? static_cast<float>(newSampleRate) : 44100.0f;
        reset();
    }

    void reset()
    {
        ic1eq = 0.0f;
        ic2eq = 0.0f;
    }

    float process(float inputSample, float cutoffHz, float resonance)
    {
        /*
            This is a TPT state-variable filter in low-pass mode.

            Why use it in the poly example?

            - It gives us a clear resonance control.
            - It stays compact enough to explain in a classroom setting.
            - Every voice can keep its own internal filter state.
        */
        const float limitedCutoff = std::clamp(cutoffHz, 20.0f, 0.45f * sampleRate);
        const float q = 0.55f + resonance * 11.45f;
        const float g = std::tan(pi * limitedCutoff / sampleRate);
        const float k = 1.0f / q;
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;
        const float v3 = inputSample - ic2eq;
        const float v1 = a1 * ic1eq + a2 * v3;
        const float v2 = ic2eq + a2 * ic1eq + a3 * v3;

        ic1eq = 2.0f * v1 - ic1eq;
        ic2eq = 2.0f * v2 - ic2eq;
        return v2;
    }

private:
    static constexpr float pi = 3.14159265358979323846f;

    float sampleRate = 44100.0f;
    float ic1eq = 0.0f;
    float ic2eq = 0.0f;
};
