#include "audio/SignalSources.h"

namespace
{
constexpr std::array<SourceType, 5> availableSources
{
    SourceType::sine,
    SourceType::whiteNoise,
    SourceType::impulse,
    SourceType::wavFile,
    SourceType::hardwareInput
};

}

juce::String sourceTypeToId(SourceType type)
{
    switch (type)
    {
        case SourceType::sine:       return "sine";
        case SourceType::whiteNoise: return "whiteNoise";
        case SourceType::impulse:    return "impulse";
        case SourceType::wavFile:    return "wavFile";
        case SourceType::hardwareInput: return "hardwareInput";
    }

    return "sine";
}

juce::String sourceTypeToDisplayName(SourceType type)
{
    switch (type)
    {
        case SourceType::sine:       return "Sine";
        case SourceType::whiteNoise: return "White Noise";
        case SourceType::impulse:    return "Impulse";
        case SourceType::wavFile:    return "WAV File";
        case SourceType::hardwareInput: return "Hardware Input";
    }

    return "Sine";
}

SourceType sourceTypeFromId(const juce::String& id)
{
    for (const auto type : availableSources)
        if (sourceTypeToId(type) == id)
            return type;

    return SourceType::sine;
}

const std::array<SourceType, 5>& getAvailableSourceTypes()
{
    return availableSources;
}

void SineSource::prepare(double sampleRate, int, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
}

void SineSource::reset()
{
    phase = 0.0;
}

void SineSource::generate(float* const* outputs, int numChannels, int numSamples)
{
    if (outputs == nullptr || numChannels <= 0 || numSamples <= 0)
        return;

    const auto frequency = juce::jlimit(20.0f, 20000.0f, frequencyHz.load(std::memory_order_relaxed));
    const auto phaseDelta = juce::MathConstants<double>::twoPi * static_cast<double>(frequency) / currentSampleRate;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto value = std::sin(static_cast<float>(phase));

        for (int channel = 0; channel < numChannels; ++channel)
            if (outputs[channel] != nullptr)
                outputs[channel][sample] = value;

        phase += phaseDelta;

        if (phase >= juce::MathConstants<double>::twoPi)
            phase -= juce::MathConstants<double>::twoPi;
    }
}

void SineSource::setFrequency(float newFrequency) noexcept
{
    frequencyHz.store(newFrequency, std::memory_order_relaxed);
}

float SineSource::getFrequency() const noexcept
{
    return frequencyHz.load(std::memory_order_relaxed);
}

void WhiteNoiseSource::prepare(double, int, int)
{
}

void WhiteNoiseSource::reset()
{
}

void WhiteNoiseSource::generate(float* const* outputs, int numChannels, int numSamples)
{
    if (outputs == nullptr || numChannels <= 0 || numSamples <= 0)
        return;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto value = random.nextFloat() * 2.0f - 1.0f;

        for (int channel = 0; channel < numChannels; ++channel)
            if (outputs[channel] != nullptr)
                outputs[channel][sample] = value;
    }
}

void ImpulseSource::prepare(double, int, int)
{
}

void ImpulseSource::reset()
{
    armed.store(true, std::memory_order_relaxed);
}

void ImpulseSource::generate(float* const* outputs, int numChannels, int numSamples)
{
    if (outputs == nullptr || numChannels <= 0 || numSamples <= 0)
        return;

    for (int channel = 0; channel < numChannels; ++channel)
        if (outputs[channel] != nullptr)
            juce::FloatVectorOperations::clear(outputs[channel], numSamples);

    if (armed.exchange(false, std::memory_order_relaxed))
        for (int channel = 0; channel < numChannels; ++channel)
            if (outputs[channel] != nullptr)
                outputs[channel][0] = 1.0f;
}
