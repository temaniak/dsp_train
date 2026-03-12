#pragma once

#include <JuceHeader.h>

enum class SourceType
{
    sine,
    whiteNoise,
    impulse,
    wavFile
};

juce::String sourceTypeToId(SourceType type);
juce::String sourceTypeToDisplayName(SourceType type);
SourceType sourceTypeFromId(const juce::String& id);
const std::array<SourceType, 4>& getAvailableSourceTypes();

class ISignalSource
{
public:
    virtual ~ISignalSource() = default;

    virtual void prepare(double sampleRate, int maxBlockSize) = 0;
    virtual void reset() = 0;
    virtual void generate(float* output, int numSamples) = 0;
};

class SineSource final : public ISignalSource
{
public:
    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void generate(float* output, int numSamples) override;

    void setFrequency(float newFrequency) noexcept;
    float getFrequency() const noexcept;

private:
    std::atomic<float> frequencyHz { 440.0f };
    double currentSampleRate = 44100.0;
    double phase = 0.0;
};

class WhiteNoiseSource final : public ISignalSource
{
public:
    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void generate(float* output, int numSamples) override;

private:
    juce::Random random;
};

class ImpulseSource final : public ISignalSource
{
public:
    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void generate(float* output, int numSamples) override;

private:
    std::atomic<bool> armed { true };
};
