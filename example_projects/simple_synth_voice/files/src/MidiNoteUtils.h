#pragma once

#include <algorithm>
#include <cmath>

namespace monomidi
{
/*
    These helper functions are intentionally tiny and header-only.

    They answer three questions that appear in almost every synth example:

    1. How do we turn a 0..1 knob into a useful musical range?
    2. How do we convert MIDI note numbers into oscillator frequency?
    3. How do we interpret pitch wheel data in semitones?

    By isolating those tasks here, the main processor can stay focused on
    signal flow instead of low-level math details.
*/

inline float clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

inline float mapExponential(float normalisedValue, float minimum, float maximum)
{
    normalisedValue = clamp01(normalisedValue);
    return minimum * std::pow(maximum / minimum, normalisedValue);
}

inline float mapSignedSemitoneOffset(float normalisedValue, float maximumAbsoluteSemitones)
{
    normalisedValue = clamp01(normalisedValue);
    return (normalisedValue * 2.0f - 1.0f) * maximumAbsoluteSemitones;
}

inline float midiNoteToFrequency(float midiNoteNumber)
{
    return 440.0f * std::pow(2.0f, (midiNoteNumber - 69.0f) / 12.0f);
}

inline float pitchWheelToSemitones(float pitchWheel, float bendRangeInSemitones)
{
    /*
        The application now exposes pitch wheel in the range -1..1.

        -1.0 means "fully down"
         0.0 means "centered"
         1.0 means "fully up"

        The synth chooses how many semitones that wheel should represent.
        In these examples we use a classic +/- 2 semitone bend range.
    */
    return pitchWheel * bendRangeInSemitones;
}
} // namespace monomidi
