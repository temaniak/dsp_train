#include "audio/SignalSources.h"

namespace
{
constexpr std::array<SourceType, 4> availableSources
{
    SourceType::sine,
    SourceType::whiteNoise,
    SourceType::impulse,
    SourceType::wavFile
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

const std::array<SourceType, 4>& getAvailableSourceTypes()
{
    return availableSources;
}

void SineSource::prepare(double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
}

void SineSource::reset()
{
    phase = 0.0;
}

void SineSource::generate(float* output, int numSamples)
{
    jassert(output != nullptr);

    const auto frequency = juce::jlimit(20.0f, 20000.0f, frequencyHz.load(std::memory_order_relaxed));
    const auto phaseDelta = juce::MathConstants<double>::twoPi * static_cast<double>(frequency) / currentSampleRate;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        output[sample] = std::sin(static_cast<float>(phase));
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

void WhiteNoiseSource::prepare(double, int)
{
}

void WhiteNoiseSource::reset()
{
}

void WhiteNoiseSource::generate(float* output, int numSamples)
{
    jassert(output != nullptr);

    for (int sample = 0; sample < numSamples; ++sample)
        output[sample] = random.nextFloat() * 2.0f - 1.0f;
}

void ImpulseSource::prepare(double, int)
{
}

void ImpulseSource::reset()
{
    armed.store(true, std::memory_order_relaxed);
}

void ImpulseSource::generate(float* output, int numSamples)
{
    jassert(output != nullptr);
    juce::FloatVectorOperations::clear(output, numSamples);

    if (armed.exchange(false, std::memory_order_relaxed) && numSamples > 0)
        output[0] = 1.0f;
}
