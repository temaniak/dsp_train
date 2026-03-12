#pragma once

#include <JuceHeader.h>

class OscilloscopeBuffer
{
public:
    explicit OscilloscopeBuffer(int capacitySamples = 16384);

    void pushSamples(const float* const* channels, int numChannels, int numSamples) noexcept;
    int popAvailable(float* leftDestination, float* rightDestination, int maxSamples) noexcept;
    void clear() noexcept;

private:
    juce::AbstractFifo fifo;
    std::vector<float> leftStorage;
    std::vector<float> rightStorage;
};
