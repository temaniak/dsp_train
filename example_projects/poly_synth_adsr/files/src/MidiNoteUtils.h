#pragma once

#include <algorithm>
#include <cmath>

namespace polysynth
{
inline float clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

inline float mapExponential(float normalisedValue, float minimum, float maximum)
{
    normalisedValue = clamp01(normalisedValue);
    return minimum * std::pow(maximum / minimum, normalisedValue);
}

inline float midiNoteToFrequency(float midiNoteNumber)
{
    return 440.0f * std::pow(2.0f, (midiNoteNumber - 69.0f) / 12.0f);
}

inline float pitchWheelToSemitones(float pitchWheel, float bendRangeInSemitones)
{
    return pitchWheel * bendRangeInSemitones;
}
} // namespace polysynth
