#pragma once

#include <JuceHeader.h>

class OscilloscopeBuffer
{
public:
    explicit OscilloscopeBuffer(int capacitySamples = 16384);

    void pushSamples(const float* samples, int numSamples) noexcept;
    int popAvailable(float* destination, int maxSamples) noexcept;
    void clear() noexcept;

private:
    juce::AbstractFifo fifo;
    std::vector<float> storage;
};
