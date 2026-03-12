#include "ui/OscilloscopeBuffer.h"

OscilloscopeBuffer::OscilloscopeBuffer(int capacitySamples)
    : fifo(juce::jmax(1024, capacitySamples)),
      leftStorage(static_cast<std::size_t>(juce::jmax(1024, capacitySamples)), 0.0f),
      rightStorage(static_cast<std::size_t>(juce::jmax(1024, capacitySamples)), 0.0f)
{
}

void OscilloscopeBuffer::pushSamples(const float* const* channels, int numChannels, int numSamples) noexcept
{
    if (channels == nullptr || numSamples <= 0 || numChannels <= 0)
        return;

    auto remaining = numSamples;
    const auto* leftSource = channels[0];
    const auto* rightSource = numChannels > 1 ? channels[1] : channels[0];

    if (leftSource == nullptr)
        return;

    if (rightSource == nullptr)
        rightSource = leftSource;

    if (remaining > static_cast<int>(leftStorage.size()))
    {
        const auto skip = remaining - static_cast<int>(leftStorage.size());
        leftSource += skip;
        rightSource += skip;
        remaining = static_cast<int>(leftStorage.size());
    }

    while (remaining > 0)
    {
        const auto writeSize = juce::jmin(remaining, fifo.getFreeSpace());

        if (writeSize <= 0)
            break;

        auto scope = fifo.write(writeSize);

        if (scope.blockSize1 > 0)
        {
            juce::FloatVectorOperations::copy(leftStorage.data() + scope.startIndex1, leftSource, scope.blockSize1);
            juce::FloatVectorOperations::copy(rightStorage.data() + scope.startIndex1, rightSource, scope.blockSize1);
        }

        if (scope.blockSize2 > 0)
        {
            juce::FloatVectorOperations::copy(leftStorage.data() + scope.startIndex2, leftSource + scope.blockSize1, scope.blockSize2);
            juce::FloatVectorOperations::copy(rightStorage.data() + scope.startIndex2, rightSource + scope.blockSize1, scope.blockSize2);
        }

        leftSource += writeSize;
        rightSource += writeSize;
        remaining -= writeSize;
    }
}

int OscilloscopeBuffer::popAvailable(float* leftDestination, float* rightDestination, int maxSamples) noexcept
{
    if (leftDestination == nullptr || rightDestination == nullptr || maxSamples <= 0)
        return 0;

    const auto ready = juce::jmin(maxSamples, fifo.getNumReady());

    if (ready <= 0)
        return 0;

    auto scope = fifo.read(ready);
    auto copied = 0;

    if (scope.blockSize1 > 0)
    {
        juce::FloatVectorOperations::copy(leftDestination, leftStorage.data() + scope.startIndex1, scope.blockSize1);
        juce::FloatVectorOperations::copy(rightDestination, rightStorage.data() + scope.startIndex1, scope.blockSize1);
        copied += scope.blockSize1;
    }

    if (scope.blockSize2 > 0)
    {
        juce::FloatVectorOperations::copy(leftDestination + copied, leftStorage.data() + scope.startIndex2, scope.blockSize2);
        juce::FloatVectorOperations::copy(rightDestination + copied, rightStorage.data() + scope.startIndex2, scope.blockSize2);
        copied += scope.blockSize2;
    }

    return copied;
}

void OscilloscopeBuffer::clear() noexcept
{
    fifo.reset();
    std::fill(leftStorage.begin(), leftStorage.end(), 0.0f);
    std::fill(rightStorage.begin(), rightStorage.end(), 0.0f);
}
