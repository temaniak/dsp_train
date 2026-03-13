#include <algorithm>
#include <cmath>

class FilterExampleProcessor
{
public:
    const char* getName() const
    {
        return "Filter Basics";
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
        sampleRate = spec.sampleRate > 1.0 ? spec.sampleRate : 44100.0;
        reset();
    }

    void reset()
    {
        low[0] = low[1] = 0.0f;
        band[0] = band[1] = 0.0f;
    }

    void process(const float* const* inputs,
                 float* const* outputs,
                 int numInputChannels,
                 int numOutputChannels,
                 int numSamples)
    {
        const float cutoffHz = mapExp(controls.cutoffKnob, 40.0f, 12000.0f);
        const float resonance = 0.2f + controls.resonanceKnob * 1.6f;
        const float wet = controls.wetKnob;
        const float drive = 1.0f + controls.driveKnob * 5.0f;
        const float f = computeFilterCoefficient(cutoffHz);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            for (int channel = 0; channel < numOutputChannels; ++channel)
            {
                auto* output = outputs[channel];
                const int stateChannel = channel < 2 ? channel : 1;

                if (output == nullptr)
                    continue;

                // This project is a classic "effect" example:
                // it expects an incoming sound and changes its tone.
                const float dry = getInputSample(inputs, numInputChannels, channel, sample);
                const float driven = std::tanh(dry * drive);

                // Chamberlin state-variable filter.
                // low follows the smoothed signal, band carries resonance.
                const float high = driven - low[stateChannel] - resonance * band[stateChannel];
                band[stateChannel] += f * high;
                low[stateChannel] += f * band[stateChannel];

                output[sample] = dry + wet * (low[stateChannel] - dry);
            }
        }
    }

private:
    float getInputSample(const float* const* inputs, int numInputChannels, int channel, int sample) const
    {
        if (inputs == nullptr || numInputChannels <= 0)
            return 0.0f;

        if (channel < numInputChannels && inputs[channel] != nullptr)
            return inputs[channel][sample];

        return inputs[0] != nullptr ? inputs[0][sample] : 0.0f;
    }

    float mapExp(float value, float minimum, float maximum) const
    {
        return minimum * std::pow(maximum / minimum, value);
    }

    float computeFilterCoefficient(float cutoffHz) const
    {
        const float limitedCutoff = std::clamp(cutoffHz, 20.0f, 0.25f * static_cast<float>(sampleRate));
        return 2.0f * std::sin(pi * limitedCutoff / static_cast<float>(sampleRate));
    }

    static constexpr float pi = 3.14159265358979323846f;

    double sampleRate = 44100.0;
    float low[2] { 0.0f, 0.0f };
    float band[2] { 0.0f, 0.0f };
};
