#pragma once

#include <JuceHeader.h>

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

    virtual void prepare(double sampleRate, int maxBlockSize) = 0;
    virtual void reset() = 0;
    virtual void process(const float* input, float* output, int numSamples) = 0;
};

class BuiltinProcessorChain final : public IBlockProcessor
{
public:
    BuiltinProcessorChain();

    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void process(const float* input, float* output, int numSamples) override;

    void setSelectedProcessor(BuiltinProcessorType type) noexcept;
    BuiltinProcessorType getSelectedProcessor() const noexcept;

    void setParameter(int index, float value) noexcept;
    float getParameter(int index) const noexcept;
    void resetParametersToDefaults(BuiltinProcessorType type) noexcept;

    BuiltinPresetData capturePresetData() const noexcept;
    void applyPresetData(const BuiltinPresetData& preset) noexcept;

private:
    void processGain(const float* input, float* output, int numSamples) noexcept;
    void processHardClip(const float* input, float* output, int numSamples) noexcept;
    void processLowpass(const float* input, float* output, int numSamples) noexcept;
    void processDelay(const float* input, float* output, int numSamples) noexcept;

    std::atomic<BuiltinProcessorType> selectedType { BuiltinProcessorType::bypass };
    std::array<std::atomic<float>, 4> parameterValues;
    double currentSampleRate = 44100.0;
    int currentMaxBlockSize = 512;
    float lowpassState = 0.0f;
    std::vector<float> delayBuffer;
    int delayWritePosition = 0;
};
