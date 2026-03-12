#include "ui/OscilloscopeBuffer.h"

OscilloscopeBuffer::OscilloscopeBuffer(int capacitySamples)
    : fifo(juce::jmax(1024, capacitySamples)),
      storage(static_cast<std::size_t>(juce::jmax(1024, capacitySamples)), 0.0f)
{
}

void OscilloscopeBuffer::pushSamples(const float* samples, int numSamples) noexcept
{
    if (samples == nullptr || numSamples <= 0)
        return;

    auto* source = samples;
    auto remaining = numSamples;

    if (remaining > static_cast<int>(storage.size()))
    {
        source += remaining - static_cast<int>(storage.size());
        remaining = static_cast<int>(storage.size());
    }

    while (remaining > 0)
    {
        const auto writeSize = juce::jmin(remaining, fifo.getFreeSpace());

        if (writeSize <= 0)
            break;

        auto scope = fifo.write(writeSize);

        if (scope.blockSize1 > 0)
            juce::FloatVectorOperations::copy(storage.data() + scope.startIndex1, source, scope.blockSize1);

        if (scope.blockSize2 > 0)
            juce::FloatVectorOperations::copy(storage.data() + scope.startIndex2, source + scope.blockSize1, scope.blockSize2);

        source += writeSize;
        remaining -= writeSize;
    }
}

int OscilloscopeBuffer::popAvailable(float* destination, int maxSamples) noexcept
{
    if (destination == nullptr || maxSamples <= 0)
        return 0;

    const auto ready = juce::jmin(maxSamples, fifo.getNumReady());

    if (ready <= 0)
        return 0;

    auto scope = fifo.read(ready);
    auto copied = 0;

    if (scope.blockSize1 > 0)
    {
        juce::FloatVectorOperations::copy(destination, storage.data() + scope.startIndex1, scope.blockSize1);
        copied += scope.blockSize1;
    }

    if (scope.blockSize2 > 0)
    {
        juce::FloatVectorOperations::copy(destination + copied, storage.data() + scope.startIndex2, scope.blockSize2);
        copied += scope.blockSize2;
    }

    return copied;
}

void OscilloscopeBuffer::clear() noexcept
{
    fifo.reset();
    std::fill(storage.begin(), storage.end(), 0.0f);
}
