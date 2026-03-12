#include "UserDspApi.h"

#include <algorithm>
#include <cmath>

class ExampleUserProcessor
{
public:
    const char* getName() const
    {
        return "Soft Clip + Tone";
    }

    int getParameterCount() const
    {
        return 3;
    }

    bool getParameterInfo(int index, DspEduParameterInfo& info) const
    {
        switch (index)
        {
            case 0:
                dspedu::copyText(info.id, "drive");
                dspedu::copyText(info.name, "Drive");
                info.minValue = 1.0f;
                info.maxValue = 12.0f;
                info.defaultValue = 2.0f;
                return true;

            case 1:
                dspedu::copyText(info.id, "tone");
                dspedu::copyText(info.name, "Tone");
                info.minValue = 20.0f;
                info.maxValue = 8000.0f;
                info.defaultValue = 1800.0f;
                return true;

            case 2:
                dspedu::copyText(info.id, "mix");
                dspedu::copyText(info.name, "Mix");
                info.minValue = 0.0f;
                info.maxValue = 1.0f;
                info.defaultValue = 1.0f;
                return true;

            default:
                return false;
        }
    }

    void prepareToPlay(double newSampleRate)
    {
        sampleRate = std::max(1.0, newSampleRate);
        reset();
    }

    void reset()
    {
        toneState = 0.0f;
    }

    void setParameter(int index, float value)
    {
        switch (index)
        {
            case 0: drive = std::clamp(value, 1.0f, 12.0f); break;
            case 1: toneHz = std::clamp(value, 20.0f, 8000.0f); break;
            case 2: mix = std::clamp(value, 0.0f, 1.0f); break;
            default: break;
        }
    }

    void processAudio(float* buffer, int numSamples)
    {
        const auto alpha = computeLowpassAlpha(toneHz);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const auto dry = buffer[sample];
            const auto driven = std::tanh(dry * drive);
            toneState += alpha * (driven - toneState);
            buffer[sample] = dry + mix * (toneState - dry);
        }
    }

private:
    float computeLowpassAlpha(float cutoffHz) const
    {
        const auto clampedCutoff = std::clamp(cutoffHz, 20.0f, 0.45f * static_cast<float>(sampleRate));
        const auto x = std::exp(-2.0f * 3.14159265358979323846f * clampedCutoff / static_cast<float>(sampleRate));
        return 1.0f - x;
    }

    double sampleRate = 44100.0;
    float drive = 2.0f;
    float toneHz = 1800.0f;
    float mix = 1.0f;
    float toneState = 0.0f;
};

DSP_EDU_DEFINE_SIMPLE_PLUGIN(ExampleUserProcessor)
