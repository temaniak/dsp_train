#include "dsp/BuiltinProcessors.h"

namespace
{
const std::array<BuiltinProcessorDescriptor, 5> descriptors
{{
    { BuiltinProcessorType::bypass, "bypass", "Bypass", {}, 0 },
    {
        BuiltinProcessorType::gain,
        "gain",
        "Gain",
        {{
            { "gain", "Gain", 0.0f, 2.0f, 1.0f }
        }},
        1
    },
    {
        BuiltinProcessorType::hardClip,
        "hardClip",
        "Hard Clip",
        {{
            { "drive", "Drive", 0.5f, 8.0f, 1.5f },
            { "threshold", "Threshold", 0.1f, 1.0f, 0.8f }
        }},
        2
    },
    {
        BuiltinProcessorType::onePoleLowpass,
        "onePoleLowpass",
        "One-Pole Lowpass",
        {{
            { "cutoffHz", "Cutoff Hz", 20.0f, 12000.0f, 1200.0f }
        }},
        1
    },
    {
        BuiltinProcessorType::simpleDelay,
        "simpleDelay",
        "Simple Delay",
        {{
            { "timeMs", "Time ms", 10.0f, 1000.0f, 250.0f },
            { "feedback", "Feedback", 0.0f, 0.95f, 0.35f },
            { "mix", "Mix", 0.0f, 1.0f, 0.35f }
        }},
        3
    }
}};

const BuiltinProcessorDescriptor& findDescriptor(BuiltinProcessorType type)
{
    for (const auto& descriptor : descriptors)
        if (descriptor.type == type)
            return descriptor;

    return descriptors.front();
}
}

juce::String builtinProcessorTypeToId(BuiltinProcessorType type)
{
    return findDescriptor(type).id;
}

juce::String builtinProcessorTypeToDisplayName(BuiltinProcessorType type)
{
    return findDescriptor(type).name;
}

BuiltinProcessorType builtinProcessorTypeFromId(const juce::String& id)
{
    for (const auto& descriptor : descriptors)
        if (descriptor.id == id)
            return descriptor.type;

    return BuiltinProcessorType::bypass;
}

const std::array<BuiltinProcessorDescriptor, 5>& getBuiltinProcessorDescriptors()
{
    return descriptors;
}

const BuiltinProcessorDescriptor& getBuiltinProcessorDescriptor(BuiltinProcessorType type)
{
    return findDescriptor(type);
}

BuiltinProcessorChain::BuiltinProcessorChain()
{
    resetParametersToDefaults(BuiltinProcessorType::bypass);
}

void BuiltinProcessorChain::prepare(double sampleRate, int maxBlockSize, int maxChannels)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    currentMaxBlockSize = juce::jmax(1, maxBlockSize);
    currentMaxChannels = juce::jlimit(1, DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS, maxChannels);
    delayBuffer.setSize(currentMaxChannels,
                        static_cast<int>(currentSampleRate * 2.0) + currentMaxBlockSize,
                        false,
                        false,
                        true);
    reset();
}

void BuiltinProcessorChain::reset()
{
    lowpassState.fill(0.0f);
    delayBuffer.clear();
    delayWritePosition = 0;
}

void BuiltinProcessorChain::process(const float* const* inputs,
                                    float* const* outputs,
                                    int numInputChannels,
                                    int numOutputChannels,
                                    int numSamples)
{
    jassert(outputs != nullptr);

    switch (selectedType.load(std::memory_order_relaxed))
    {
        case BuiltinProcessorType::bypass:
            processGain(inputs, outputs, numInputChannels, numOutputChannels, numSamples);
            break;

        case BuiltinProcessorType::gain:
            processGain(inputs, outputs, numInputChannels, numOutputChannels, numSamples);
            break;

        case BuiltinProcessorType::hardClip:
            processHardClip(inputs, outputs, numInputChannels, numOutputChannels, numSamples);
            break;

        case BuiltinProcessorType::onePoleLowpass:
            processLowpass(inputs, outputs, numInputChannels, numOutputChannels, numSamples);
            break;

        case BuiltinProcessorType::simpleDelay:
            processDelay(inputs, outputs, numInputChannels, numOutputChannels, numSamples);
            break;
    }
}

void BuiltinProcessorChain::setSelectedProcessor(BuiltinProcessorType type) noexcept
{
    selectedType.store(type, std::memory_order_relaxed);
}

BuiltinProcessorType BuiltinProcessorChain::getSelectedProcessor() const noexcept
{
    return selectedType.load(std::memory_order_relaxed);
}

void BuiltinProcessorChain::setParameter(int index, float value) noexcept
{
    if (! juce::isPositiveAndBelow(index, static_cast<int>(parameterValues.size())))
        return;

    const auto& descriptor = findDescriptor(getSelectedProcessor());

    if (index >= descriptor.parameterCount)
        return;

    const auto& spec = descriptor.parameters[static_cast<std::size_t>(index)];
    parameterValues[static_cast<std::size_t>(index)].store(juce::jlimit(spec.minValue, spec.maxValue, value), std::memory_order_relaxed);
}

float BuiltinProcessorChain::getParameter(int index) const noexcept
{
    if (! juce::isPositiveAndBelow(index, static_cast<int>(parameterValues.size())))
        return 0.0f;

    return parameterValues[static_cast<std::size_t>(index)].load(std::memory_order_relaxed);
}

void BuiltinProcessorChain::resetParametersToDefaults(BuiltinProcessorType type) noexcept
{
    const auto& descriptor = findDescriptor(type);

    for (std::size_t index = 0; index < parameterValues.size(); ++index)
    {
        const auto defaultValue = index < static_cast<std::size_t>(descriptor.parameterCount)
                                ? descriptor.parameters[index].defaultValue
                                : 0.0f;

        parameterValues[index].store(defaultValue, std::memory_order_relaxed);
    }
}

BuiltinPresetData BuiltinProcessorChain::capturePresetData() const noexcept
{
    BuiltinPresetData preset;
    preset.processorId = builtinProcessorTypeToId(getSelectedProcessor());

    for (std::size_t index = 0; index < parameterValues.size(); ++index)
        preset.parameterValues[index] = parameterValues[index].load(std::memory_order_relaxed);

    return preset;
}

void BuiltinProcessorChain::applyPresetData(const BuiltinPresetData& preset) noexcept
{
    const auto type = builtinProcessorTypeFromId(preset.processorId);
    setSelectedProcessor(type);

    const auto& descriptor = getBuiltinProcessorDescriptor(type);

    for (int index = 0; index < descriptor.parameterCount; ++index)
        setParameter(index, preset.parameterValues[static_cast<std::size_t>(index)]);
}

const float* BuiltinProcessorChain::getInputChannel(const float* const* inputs, int numInputChannels, int channel) const noexcept
{
    if (inputs == nullptr || numInputChannels <= 0)
        return nullptr;

    return inputs[juce::jlimit(0, juce::jmax(0, numInputChannels - 1), channel)];
}

void BuiltinProcessorChain::processGain(const float* const* inputs,
                                        float* const* outputs,
                                        int numInputChannels,
                                        int numOutputChannels,
                                        int numSamples) noexcept
{
    const auto gain = selectedType.load(std::memory_order_relaxed) == BuiltinProcessorType::bypass ? 1.0f : getParameter(0);

    for (int channel = 0; channel < numOutputChannels; ++channel)
    {
        auto* output = outputs[channel];

        if (output == nullptr)
            continue;

        if (const auto* input = getInputChannel(inputs, numInputChannels, channel); input != nullptr)
        {
            juce::FloatVectorOperations::copy(output, input, numSamples);
            juce::FloatVectorOperations::multiply(output, gain, numSamples);
        }
        else
        {
            juce::FloatVectorOperations::clear(output, numSamples);
        }
    }
}

void BuiltinProcessorChain::processHardClip(const float* const* inputs,
                                            float* const* outputs,
                                            int numInputChannels,
                                            int numOutputChannels,
                                            int numSamples) noexcept
{
    const auto drive = getParameter(0);
    const auto threshold = juce::jlimit(0.05f, 1.0f, getParameter(1));

    for (int channel = 0; channel < numOutputChannels; ++channel)
    {
        const auto* input = getInputChannel(inputs, numInputChannels, channel);
        auto* output = outputs[channel];

        if (output == nullptr)
            continue;

        if (input == nullptr)
        {
            juce::FloatVectorOperations::clear(output, numSamples);
            continue;
        }

        for (int sample = 0; sample < numSamples; ++sample)
            output[sample] = juce::jlimit(-threshold, threshold, input[sample] * drive);
    }
}

void BuiltinProcessorChain::processLowpass(const float* const* inputs,
                                          float* const* outputs,
                                          int numInputChannels,
                                          int numOutputChannels,
                                          int numSamples) noexcept
{
    const auto cutoffHz = juce::jlimit(20.0f, static_cast<float>(currentSampleRate * 0.45), getParameter(0));
    const auto alpha = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * cutoffHz / static_cast<float>(currentSampleRate));

    for (int channel = 0; channel < numOutputChannels; ++channel)
    {
        const auto* input = getInputChannel(inputs, numInputChannels, channel);
        auto* output = outputs[channel];

        if (output == nullptr)
            continue;

        if (input == nullptr)
        {
            juce::FloatVectorOperations::clear(output, numSamples);
            continue;
        }

        auto& state = lowpassState[static_cast<std::size_t>(channel)];

        for (int sample = 0; sample < numSamples; ++sample)
        {
            state += alpha * (input[sample] - state);
            output[sample] = state;
        }
    }
}

void BuiltinProcessorChain::processDelay(const float* const* inputs,
                                         float* const* outputs,
                                         int numInputChannels,
                                         int numOutputChannels,
                                         int numSamples) noexcept
{
    if (delayBuffer.getNumSamples() <= 0)
    {
        processGain(inputs, outputs, numInputChannels, numOutputChannels, numSamples);
        return;
    }

    const auto delaySamples = juce::jlimit(1,
                                           delayBuffer.getNumSamples() - 1,
                                           static_cast<int>(getParameter(0) * 0.001f * static_cast<float>(currentSampleRate)));
    const auto feedback = juce::jlimit(0.0f, 0.95f, getParameter(1));
    const auto mix = juce::jlimit(0.0f, 1.0f, getParameter(2));

    for (int channel = 0; channel < numOutputChannels; ++channel)
    {
        const auto* input = getInputChannel(inputs, numInputChannels, channel);
        auto* output = outputs[channel];

        if (output == nullptr)
            continue;

        auto* delayChannel = delayBuffer.getWritePointer(channel);

        if (input == nullptr || delayChannel == nullptr)
        {
            juce::FloatVectorOperations::clear(output, numSamples);
            continue;
        }

        int writePosition = delayWritePosition;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            auto readPosition = writePosition - delaySamples;

            if (readPosition < 0)
                readPosition += delayBuffer.getNumSamples();

            const auto delayed = delayChannel[readPosition];
            const auto dry = input[sample];
            output[sample] = dry + mix * (delayed - dry);
            delayChannel[writePosition] = dry + delayed * feedback;
            writePosition = (writePosition + 1) % delayBuffer.getNumSamples();
        }
    }

    delayWritePosition = (delayWritePosition + numSamples) % delayBuffer.getNumSamples();
}
