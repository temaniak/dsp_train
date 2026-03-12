#include <algorithm>
#include <cmath>

// UserDspApi.h, the generated controls header, and the wrapper are injected automatically.
// The default example project exposes:
// controls.driveKnob, controls.toneKnob, controls.mixKnob,
// controls.exciteButton, controls.bypassToggle.

class ExampleUserProcessor
{
public:
    const char* getName() const
    {
        return "Drive Lab Example";
    }

    bool getPreferredAudioConfig(DspEduPreferredAudioConfig& config) const
    {
        config.preferredSampleRate = 48000.0;
        config.preferredBlockSize = 256;
        config.preferredInputChannels = 2;
        config.preferredOutputChannels = 2;
        return true;
    }

    void prepare(const DspEduProcessSpec& spec)
    {
        sampleRate = std::max(1.0, spec.sampleRate);
        reset();
    }

    void reset()
    {
        toneState[0] = 0.0f;
        toneState[1] = 0.0f;
        exciteEnvelope = 0.0f;
    }

    void process(const float* const* inputs,
                 float* const* outputs,
                 int numInputChannels,
                 int numOutputChannels,
                 int numSamples)
    {
        if (numOutputChannels <= 0 || outputs == nullptr || outputs[0] == nullptr)
            return;

        const auto drive = remap01(controls.driveKnob, 1.0f, 14.0f);
        const auto toneHz = remap01(controls.toneKnob, 250.0f, 8000.0f);
        const auto mix = controls.mixKnob;
        const auto exciteTarget = controls.exciteButton ? 1.0f : 0.0f;
        const auto alpha = computeLowpassAlpha(toneHz);
        const auto* inputLeft = numInputChannels > 0 && inputs != nullptr ? inputs[0] : nullptr;
        const auto* inputRight = numInputChannels > 1 && inputs != nullptr ? inputs[1] : inputLeft;
        auto* outputLeft = outputs[0];
        auto* outputRight = numOutputChannels > 1 ? outputs[1] : outputs[0];

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const auto dryLeft = inputLeft != nullptr ? inputLeft[sample] : 0.0f;
            const auto dryRight = inputRight != nullptr ? inputRight[sample] : dryLeft;

            if (controls.bypassToggle)
            {
                outputLeft[sample] = dryLeft;

                if (numOutputChannels > 1 && outputRight != nullptr)
                    outputRight[sample] = dryRight;

                continue;
            }

            exciteEnvelope += 0.18f * (exciteTarget - exciteEnvelope);
            const auto excitedLeft = dryLeft + 0.22f * exciteEnvelope;
            const auto excitedRight = dryRight + 0.22f * exciteEnvelope;
            const auto drivenLeft = std::tanh(excitedLeft * drive);
            const auto drivenRight = std::tanh(excitedRight * drive);
            toneState[0] += alpha * (drivenLeft - toneState[0]);
            toneState[1] += alpha * (drivenRight - toneState[1]);
            outputLeft[sample] = dryLeft + mix * (toneState[0] - dryLeft);

            if (numOutputChannels > 1 && outputRight != nullptr)
                outputRight[sample] = dryRight + mix * (toneState[1] - dryRight);

            exciteEnvelope *= 0.995f;
        }
    }

private:
    float remap01(float value, float minimum, float maximum) const
    {
        const auto clamped = std::clamp(value, 0.0f, 1.0f);
        return minimum + (maximum - minimum) * clamped;
    }

    float computeLowpassAlpha(float cutoffHz) const
    {
        const auto clampedCutoff = std::clamp(cutoffHz, 20.0f, 0.45f * static_cast<float>(sampleRate));
        const auto x = std::exp(-2.0f * 3.14159265358979323846f * clampedCutoff / static_cast<float>(sampleRate));
        return 1.0f - x;
    }

    double sampleRate = 44100.0;
    float toneState[2] { 0.0f, 0.0f };
    float exciteEnvelope = 0.0f;
};
