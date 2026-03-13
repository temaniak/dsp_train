#include <algorithm>
#include <cmath>

class LfoExampleProcessor
{
public:
    const char* getName() const
    {
        return "LFO Basics";
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
        state[0] = state[1] = 0.0f;
    }

    void process(const float* const* inputs,
                 float* const* outputs,
                 int numInputChannels,
                 int numOutputChannels,
                 int numSamples)
    {
        const float rateHz = mapExp(controls.rateKnob, 0.10f, 12.0f);
        const float depth = controls.depthKnob;
        const float baseCutoff = mapExp(controls.baseCutoffKnob, 80.0f, 4000.0f);
        const float wet = controls.wetKnob;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            // An LFO is just another oscillator, but very slow.
            // Here it moves a filter cutoff instead of making audible pitch.
            const float lfo01 = 0.5f + 0.5f * std::sin(twoPi * phase);
            const float minCutoff = baseCutoff * (1.0f - 0.9f * depth);
            const float maxCutoff = baseCutoff + (12000.0f - baseCutoff) * depth;
            const float modulatedCutoff = minCutoff + (maxCutoff - minCutoff) * lfo01;
            const float alpha = computeLowpassAlpha(modulatedCutoff);

            advancePhase(rateHz);

            for (int channel = 0; channel < numOutputChannels; ++channel)
            {
                auto* output = outputs[channel];
                const int stateChannel = channel < 2 ? channel : 1;

                if (output == nullptr)
                    continue;

                const float dry = getInputSample(inputs, numInputChannels, channel, sample);
                state[stateChannel] += alpha * (dry - state[stateChannel]);
                output[sample] = dry + wet * (state[stateChannel] - dry);
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

    float computeLowpassAlpha(float cutoffHz) const
    {
        const float limitedCutoff = std::clamp(cutoffHz, 20.0f, 0.45f * static_cast<float>(sampleRate));
        return 1.0f - std::exp(-twoPi * limitedCutoff / static_cast<float>(sampleRate));
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
    float state[2] { 0.0f, 0.0f };
};
