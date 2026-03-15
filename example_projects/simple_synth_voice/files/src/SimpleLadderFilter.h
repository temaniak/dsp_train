#pragma once

#include <algorithm>
#include <cmath>

class SimpleLadderFilter
{
public:
    void prepare(double newSampleRate)
    {
        sampleRate = newSampleRate > 1.0 ? static_cast<float>(newSampleRate) : 44100.0f;
        reset();
    }

    void reset()
    {
        z1 = 0.0f;
        z2 = 0.0f;
        z3 = 0.0f;
        z4 = 0.0f;
    }

    float process(float inputSample, float cutoffHz, float resonance)
    {
        /*
            This is a deliberately compact "ladder-style" low-pass filter.

            It is not meant to be a perfect analog model.
            It is meant to be readable:

            - cutoff controls how fast each stage can move
            - resonance feeds some of the last stage back to the input
            - four one-pole stages in series create a smoother synth tone

            The important educational point is not the exact topology.
            The important point is that oscillator -> filter -> envelope is a
            common synth voice structure.
        */
        const float limitedCutoff = std::clamp(cutoffHz, 20.0f, 0.45f * sampleRate);
        const float g = std::tan(pi * limitedCutoff / sampleRate);
        const float filterInput = inputSample - resonance * z4;

        z1 += g * (filterInput - z1);
        z2 += g * (z1 - z2);
        z3 += g * (z2 - z3);
        z4 += g * (z3 - z4);
        return z4;
    }

private:
    static constexpr float pi = 3.14159265358979323846f;

    float sampleRate = 44100.0f;
    float z1 = 0.0f;
    float z2 = 0.0f;
    float z3 = 0.0f;
    float z4 = 0.0f;
};
