#include <cmath>

class EnvelopeExampleProcessor
{
public:
    const char* getName() const
    {
        return "Envelope Basics";
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
        envelope = 0.0f;
        previousTriggerState = false;
    }

    void process(const float* const* inputs,
                 float* const* outputs,
                 int numInputChannels,
                 int numOutputChannels,
                 int numSamples)
    {
        const float decaySeconds = mapExp(controls.decayKnob, 0.03f, 3.0f);
        const float decayMultiplier = std::exp(-1.0f / (decaySeconds * static_cast<float>(sampleRate)));
        const float amount = controls.amountKnob;
        const float dryLevel = controls.dryKnob;

        // A button is easiest to use as a one-shot trigger on its rising edge.
        if (controls.triggerButton && ! previousTriggerState)
            envelope = 1.0f;

        previousTriggerState = controls.triggerButton;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            for (int channel = 0; channel < numOutputChannels; ++channel)
            {
                auto* output = outputs[channel];

                if (output == nullptr)
                    continue;

                // The envelope does not create sound by itself.
                // It shapes the level of whatever signal is coming in.
                const float dry = getInputSample(inputs, numInputChannels, channel, sample);
                const float envelopeGain = dryLevel + amount * envelope;
                output[sample] = dry * envelopeGain;
            }

            envelope *= decayMultiplier;
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

    double sampleRate = 44100.0;
    float envelope = 0.0f;
    bool previousTriggerState = false;
};
