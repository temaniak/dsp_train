#pragma once

#include <algorithm>
#include <cmath>

class SimpleGateEnvelope
{
public:
    void prepare(double newSampleRate)
    {
        sampleRate = newSampleRate > 1.0 ? static_cast<float>(newSampleRate) : 44100.0f;
        updateAttackCoefficient();
        updateReleaseCoefficient();
        reset();
    }

    void reset()
    {
        currentValue = 0.0f;
        state = State::idle;
    }

    void setAttackTime(float seconds)
    {
        attackSeconds = std::max(0.001f, seconds);
        updateAttackCoefficient();
    }

    void setReleaseTime(float seconds)
    {
        releaseSeconds = std::max(0.005f, seconds);
        updateReleaseCoefficient();
    }

    void noteOn()
    {
        /*
            We intentionally restart from zero on every note-on.

            This makes the articulation easy to hear and easy to explain:
            each key press produces a fresh attack, even if another note was
            already sounding one sample earlier.
        */
        currentValue = 0.0f;
        state = State::attack;
    }

    void noteOff()
    {
        if (state != State::idle)
            state = State::release;
    }

    float process()
    {
        switch (state)
        {
            case State::idle:
                currentValue = 0.0f;
                break;

            case State::attack:
                currentValue += (1.0f - currentValue) * attackCoefficient;

                if (currentValue >= 0.999f)
                {
                    currentValue = 1.0f;
                    state = State::sustain;
                }
                break;

            case State::sustain:
                currentValue = 1.0f;
                break;

            case State::release:
                currentValue *= releaseCoefficient;

                if (currentValue <= 0.0001f)
                {
                    currentValue = 0.0f;
                    state = State::idle;
                }
                break;
        }

        return currentValue;
    }

    bool isActive() const
    {
        return state != State::idle;
    }

private:
    enum class State
    {
        idle,
        attack,
        sustain,
        release
    };

    void updateAttackCoefficient()
    {
        attackCoefficient = 1.0f - std::exp(-1.0f / (attackSeconds * sampleRate));
    }

    void updateReleaseCoefficient()
    {
        releaseCoefficient = std::exp(-1.0f / (releaseSeconds * sampleRate));
    }

    float sampleRate = 44100.0f;
    float attackSeconds = 0.005f;
    float releaseSeconds = 0.25f;
    float attackCoefficient = 0.0f;
    float releaseCoefficient = 0.0f;
    float currentValue = 0.0f;
    State state = State::idle;
};
