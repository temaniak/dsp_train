#include <cmath>

class OscillatorExampleProcessor
{
public:
    const char* getName() const
    {
        return "Oscillator Basics";
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
        phase = 0.0f;
    }

    void process(const float* const* inputs,
                 float* const* outputs,
                 int numInputChannels,
                 int numOutputChannels,
                 int numSamples)
    {
        const float pitchHz = mapExp(controls.pitchKnob, 40.0f, 1200.0f);
        const float oscLevel = controls.oscLevelKnob;
        const float inputMix = controls.inputMixKnob;
        const float shape = controls.shapeKnob;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            // The oscillator is built from two very common building blocks:
            // a sine wave and a saw wave. Wave Shape blends between them.
            const float saw = 2.0f * phase - 1.0f;
            const float sine = std::sin(twoPi * phase);
            const float oscillatorSample = (1.0f - shape) * sine + shape * saw;

            advancePhase(pitchHz);

            // Every output channel mixes the incoming signal with the oscillator.
            // This makes the example useful with Hardware Input, WAV, or Noise.
            for (int channel = 0; channel < numOutputChannels; ++channel)
            {
                auto* output = outputs[channel];

                if (output == nullptr)
                    continue;

                const float dry = getInputSample(inputs, numInputChannels, channel, sample);
                output[sample] = dry * inputMix + oscillatorSample * oscLevel;
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

    void advancePhase(float frequencyHz)
    {
        phase += frequencyHz / static_cast<float>(sampleRate);

        while (phase >= 1.0f)
            phase -= 1.0f;
    }

    static constexpr float twoPi = 6.28318530717958647692f;

    double sampleRate = 44100.0;
    float phase = 0.0f;
};
