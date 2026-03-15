#pragma once

#include <algorithm>
#include <cmath>

class AdsrEnvelope
{
public:
    void prepare(double newSampleRate)
    {
        sampleRate = newSampleRate > 1.0 ? static_cast<float>(newSampleRate) : 44100.0f;
        reset();
        updateCoefficients();
    }

    void reset()
    {
        currentValue = 0.0f;
        state = State::idle;
    }

    void setParameters(float newAttackSeconds,
                       float newDecaySeconds,
                       float newSustainLevel,
                       float newReleaseSeconds)
    {
        attackSeconds = std::max(0.001f, newAttackSeconds);
        decaySeconds = std::max(0.001f, newDecaySeconds);
        sustainLevel = std::clamp(newSustainLevel, 0.0f, 1.0f);
        releaseSeconds = std::max(0.005f, newReleaseSeconds);
        updateCoefficients();
    }

    void noteOn()
    {
        state = State::attack;
    }

    void noteOff()
    {
        if (state != State::idle)
            state = State::release;
    }

    float process()
    {
        /*
            ADSR stands for:

            - Attack  : how fast the note rises
            - Decay   : how fast it falls after the peak
            - Sustain : the level while the key is held
            - Release : how fast it fades after key release

            This implementation aims to be easy to read, not maximally fancy.
        */
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
                    state = State::decay;
                }
                break;

            case State::decay:
                currentValue = sustainLevel + (currentValue - sustainLevel) * decayCoefficient;

                if (currentValue <= sustainLevel + 0.0001f)
                {
                    currentValue = sustainLevel;
                    state = State::sustain;
                }
                break;

            case State::sustain:
                currentValue = sustainLevel;
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
        decay,
        sustain,
        release
    };

    void updateCoefficients()
    {
        attackCoefficient = 1.0f - std::exp(-1.0f / (attackSeconds * sampleRate));
        decayCoefficient = std::exp(-1.0f / (decaySeconds * sampleRate));
        releaseCoefficient = std::exp(-1.0f / (releaseSeconds * sampleRate));
    }

    float sampleRate = 44100.0f;
    float attackSeconds = 0.01f;
    float decaySeconds = 0.2f;
    float sustainLevel = 0.6f;
    float releaseSeconds = 0.3f;
    float attackCoefficient = 0.0f;
    float decayCoefficient = 0.0f;
    float releaseCoefficient = 0.0f;
    float currentValue = 0.0f;
    State state = State::idle;
};
