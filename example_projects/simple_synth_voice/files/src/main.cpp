#include <algorithm>
#include <cmath>

class SimpleSynthVoiceProcessor
{
public:
    const char* getName() const
    {
        return "Simple Synth Voice";
    }

    bool getPreferredAudioConfig(DspEduPreferredAudioConfig& config) const
    {
        config.preferredSampleRate = 48000.0;
        config.preferredBlockSize = 256;
        config.preferredInputChannels = 0;
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
        phase = 0.0f;
        envelope = 0.0f;
        previousTriggerState = false;
        z1 = z2 = z3 = z4 = 0.0f;
    }

    void process(const float* const*,
                 float* const* outputs,
                 int,
                 int numOutputChannels,
                 int numSamples)
    {
        const float pitchHz = mapExp(controls.pitchKnob, 40.0f, 1000.0f);
        const float baseCutoff = mapExp(controls.cutoffKnob, 40.0f, 4000.0f);
        const float resonance = controls.resonanceKnob * 3.8f;
        const float decaySeconds = mapExp(controls.decayKnob, 0.03f, 2.5f);
        const float decayMultiplier = std::exp(-1.0f / (decaySeconds * static_cast<float>(sampleRate)));
        const float envAmountHz = controls.envAmountKnob * 8000.0f;

        if (controls.triggerButton && ! previousTriggerState)
            envelope = 1.0f;

        previousTriggerState = controls.triggerButton;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            // A synth voice starts with an oscillator, then shapes it with
            // an envelope and a filter. This example keeps the structure simple.
            const float saw = 2.0f * phase - 1.0f;
            advancePhase(pitchHz);

            const float modulatedCutoff = baseCutoff + envelope * envAmountHz;
            const float filtered = processLadderSample(saw, modulatedCutoff, resonance);
            const float voiceSample = filtered * envelope * 0.25f;

            for (int channel = 0; channel < numOutputChannels; ++channel)
                if (outputs[channel] != nullptr)
                    outputs[channel][sample] = voiceSample;

            envelope *= decayMultiplier;
        }
    }

private:
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

    float processLadderSample(float input, float cutoffHz, float resonance)
    {
        const float limitedCutoff = std::clamp(cutoffHz, 20.0f, 0.45f * static_cast<float>(sampleRate));
        const float g = static_cast<float>(std::tan(pi * limitedCutoff / static_cast<float>(sampleRate)));
        const float filterInput = input - resonance * z4;

        z1 += g * (filterInput - z1);
        z2 += g * (z1 - z2);
        z3 += g * (z2 - z3);
        z4 += g * (z3 - z4);
        return z4;
    }

    static constexpr float pi = 3.14159265358979323846f;

    double sampleRate = 44100.0;
    float phase = 0.0f;
    float envelope = 0.0f;
    bool previousTriggerState = false;
    float z1 = 0.0f;
    float z2 = 0.0f;
    float z3 = 0.0f;
    float z4 = 0.0f;
};
