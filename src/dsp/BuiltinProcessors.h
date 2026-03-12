#pragma once

#include <JuceHeader.h>

#include "UserDspApi.h"

enum class BuiltinProcessorType
{
    bypass,
    gain,
    hardClip,
    onePoleLowpass,
    simpleDelay
};

struct BuiltinParameterSpec
{
    juce::String id;
    juce::String name;
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float defaultValue = 0.0f;
};

struct BuiltinProcessorDescriptor
{
    BuiltinProcessorType type = BuiltinProcessorType::bypass;
    juce::String id;
    juce::String name;
    std::array<BuiltinParameterSpec, 4> parameters;
    int parameterCount = 0;
};

struct BuiltinPresetData
{
    juce::String processorId;
    std::array<float, 4> parameterValues {};
};

juce::String builtinProcessorTypeToId(BuiltinProcessorType type);
juce::String builtinProcessorTypeToDisplayName(BuiltinProcessorType type);
BuiltinProcessorType builtinProcessorTypeFromId(const juce::String& id);
const std::array<BuiltinProcessorDescriptor, 5>& getBuiltinProcessorDescriptors();
const BuiltinProcessorDescriptor& getBuiltinProcessorDescriptor(BuiltinProcessorType type);

class IBlockProcessor
{
public:
    virtual ~IBlockProcessor() = default;

    virtual void prepare(double sampleRate, int maxBlockSize, int maxChannels = DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS) = 0;
    virtual void reset() = 0;
    virtual void process(const float* const* inputs,
                         float* const* outputs,
                         int numInputChannels,
                         int numOutputChannels,
                         int numSamples) = 0;
};

class BuiltinProcessorChain final : public IBlockProcessor
{
public:
    BuiltinProcessorChain();

    void prepare(double sampleRate, int maxBlockSize, int maxChannels = DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS) override;
    void reset() override;
    void process(const float* const* inputs,
                 float* const* outputs,
                 int numInputChannels,
                 int numOutputChannels,
                 int numSamples) override;

    void setSelectedProcessor(BuiltinProcessorType type) noexcept;
    BuiltinProcessorType getSelectedProcessor() const noexcept;

    void setParameter(int index, float value) noexcept;
    float getParameter(int index) const noexcept;
    void resetParametersToDefaults(BuiltinProcessorType type) noexcept;

    BuiltinPresetData capturePresetData() const noexcept;
    void applyPresetData(const BuiltinPresetData& preset) noexcept;

private:
    const float* getInputChannel(const float* const* inputs, int numInputChannels, int channel) const noexcept;
    void processGain(const float* const* inputs, float* const* outputs, int numInputChannels, int numOutputChannels, int numSamples) noexcept;
    void processHardClip(const float* const* inputs, float* const* outputs, int numInputChannels, int numOutputChannels, int numSamples) noexcept;
    void processLowpass(const float* const* inputs, float* const* outputs, int numInputChannels, int numOutputChannels, int numSamples) noexcept;
    void processDelay(const float* const* inputs, float* const* outputs, int numInputChannels, int numOutputChannels, int numSamples) noexcept;

    std::atomic<BuiltinProcessorType> selectedType { BuiltinProcessorType::bypass };
    std::array<std::atomic<float>, 4> parameterValues;
    double currentSampleRate = 44100.0;
    int currentMaxBlockSize = 512;
    int currentMaxChannels = DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS;
    std::array<float, DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS> lowpassState {};
    juce::AudioBuffer<float> delayBuffer;
    int delayWritePosition = 0;
};
