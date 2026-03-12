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

    void prepareToPlay(double newSampleRate)
    {
        sampleRate = std::max(1.0, newSampleRate);
        reset();
    }

    void reset()
    {
        toneState = 0.0f;
        exciteEnvelope = 0.0f;
    }

    void processAudio(float* buffer, int numSamples)
    {
        if (controls.bypassToggle)
            return;

        const auto drive = remap01(controls.driveKnob, 1.0f, 14.0f);
        const auto toneHz = remap01(controls.toneKnob, 250.0f, 8000.0f);
        const auto mix = controls.mixKnob;
        const auto exciteTarget = controls.exciteButton ? 1.0f : 0.0f;
        const auto alpha = computeLowpassAlpha(toneHz);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            exciteEnvelope += 0.18f * (exciteTarget - exciteEnvelope);
            const auto dry = buffer[sample];
            const auto excited = dry + 0.22f * exciteEnvelope;
            const auto driven = std::tanh(excited * drive);
            toneState += alpha * (driven - toneState);
            buffer[sample] = dry + mix * (toneState - dry);
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
    float toneState = 0.0f;
    float exciteEnvelope = 0.0f;
};
