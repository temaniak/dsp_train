#include <algorithm>
#include <cmath>

class DelayExampleProcessor
{
public:
    const char* getName() const
    {
        return "Delay Basics";
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
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < maxDelaySamples; ++sample)
                delayBuffer[channel][sample] = 0.0f;

        writeIndex = 0;
    }

    void process(const float* const* inputs,
                 float* const* outputs,
                 int numInputChannels,
                 int numOutputChannels,
                 int numSamples)
    {
        const float delaySeconds = mapExp(controls.timeKnob, 0.05f, 1.20f);
        const int delaySamples = std::clamp(static_cast<int>(delaySeconds * sampleRate), 1, maxDelaySamples - 1);
        const float feedback = 0.05f + controls.feedbackKnob * 0.90f;
        const float wet = controls.wetKnob;
        const float inputLevel = controls.inputLevelKnob;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const int readIndex = (writeIndex + maxDelaySamples - delaySamples) % maxDelaySamples;

            for (int channel = 0; channel < numOutputChannels; ++channel)
            {
                auto* output = outputs[channel];
                const int delayChannel = channel < 2 ? channel : 1;

                if (output == nullptr)
                    continue;

                // Delay only makes sense when something enters it.
                // Feed it with Hardware Input, WAV, or an Impulse source.
                const float dry = getInputSample(inputs, numInputChannels, channel, sample) * inputLevel;
                const float delayed = delayBuffer[delayChannel][readIndex];

                delayBuffer[delayChannel][writeIndex] = dry + delayed * feedback;
                output[sample] = dry + wet * (delayed - dry);
            }

            writeIndex = (writeIndex + 1) % maxDelaySamples;
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

    static constexpr int maxDelaySamples = 96000;

    double sampleRate = 44100.0;
    float delayBuffer[2][maxDelaySamples] {};
    int writeIndex = 0;
};
