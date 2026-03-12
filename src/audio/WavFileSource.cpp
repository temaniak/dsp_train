#include "audio/WavFileSource.h"

juce::Result WavFileSource::loadFromFile(const juce::File& file, juce::AudioFormatManager& formatManager)
{
    auto reader = std::unique_ptr<juce::AudioFormatReader>(formatManager.createReaderFor(file));

    if (reader == nullptr)
        return juce::Result::fail("Unable to open WAV file.");

    const auto loadedChannels = juce::jlimit(1, DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS, static_cast<int>(reader->numChannels));
    juce::AudioBuffer<float> loadedData(loadedChannels, static_cast<int>(reader->lengthInSamples));

    if (loadedData.getNumSamples() <= 0)
        return juce::Result::fail("The WAV file is empty.");

    juce::AudioBuffer<float> tempBuffer(reader->numChannels, loadedData.getNumSamples());

    if (! reader->read(&tempBuffer, 0, tempBuffer.getNumSamples(), 0, true, true))
        return juce::Result::fail("Failed to read WAV file contents.");

    for (int channel = 0; channel < loadedChannels; ++channel)
        loadedData.copyFrom(channel, 0, tempBuffer, channel, 0, tempBuffer.getNumSamples());

    {
        const juce::SpinLock::ScopedLockType lock(dataLock);
        audioData = std::move(loadedData);
        loadedFileName = file.getFileName();
        loadedChannelCount = loadedChannels;
        playbackPosition.store(0, std::memory_order_relaxed);
    }

    return juce::Result::ok();
}

void WavFileSource::clear()
{
    const juce::SpinLock::ScopedLockType lock(dataLock);
    audioData.setSize(DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS, 0);
    loadedFileName.clear();
    loadedChannelCount = 0;
    playbackPosition.store(0, std::memory_order_relaxed);
}

void WavFileSource::reset()
{
    playbackPosition.store(0, std::memory_order_relaxed);
}

void WavFileSource::prepare(double, int, int)
{
}

void WavFileSource::generate(float* const* outputs, int numChannels, int numSamples)
{
    if (outputs == nullptr || numChannels <= 0 || numSamples <= 0)
        return;

    for (int channel = 0; channel < numChannels; ++channel)
        if (outputs[channel] != nullptr)
            juce::FloatVectorOperations::clear(outputs[channel], numSamples);

    const juce::SpinLock::ScopedTryLockType lock(dataLock);

    if (! lock.isLocked() || audioData.getNumSamples() == 0 || loadedChannelCount <= 0)
        return;

    auto position = playbackPosition.load(std::memory_order_relaxed);
    const auto totalSamples = static_cast<juce::int64>(audioData.getNumSamples());
    const auto shouldLoop = looping.load(std::memory_order_relaxed);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        if (position >= totalSamples)
        {
            if (shouldLoop)
                position = 0;
            else
                break;
        }

        const auto currentIndex = static_cast<int>(position);

        if (loadedChannelCount == 1)
        {
            const auto value = audioData.getReadPointer(0)[currentIndex];

            for (int channel = 0; channel < numChannels; ++channel)
                if (outputs[channel] != nullptr)
                    outputs[channel][sample] = value;
        }
        else
        {
            const auto left = audioData.getReadPointer(0)[currentIndex];
            const auto right = audioData.getReadPointer(1)[currentIndex];

            if (numChannels == 1)
            {
                if (outputs[0] != nullptr)
                    outputs[0][sample] = 0.5f * (left + right);
            }
            else
            {
                if (outputs[0] != nullptr)
                    outputs[0][sample] = left;

                if (outputs[1] != nullptr)
                    outputs[1][sample] = right;
            }
        }

        ++position;
    }

    playbackPosition.store(position, std::memory_order_relaxed);
}

int WavFileSource::getChannelCount() const noexcept
{
    const juce::SpinLock::ScopedLockType lock(dataLock);
    return loadedChannelCount;
}

bool WavFileSource::hasFileLoaded() const noexcept
{
    const juce::SpinLock::ScopedLockType lock(dataLock);
    return audioData.getNumSamples() > 0;
}

juce::String WavFileSource::getLoadedFileName() const
{
    const juce::SpinLock::ScopedLockType lock(dataLock);
    return loadedFileName;
}

void WavFileSource::setLooping(bool shouldLoop) noexcept
{
    looping.store(shouldLoop, std::memory_order_relaxed);
}

bool WavFileSource::isLooping() const noexcept
{
    return looping.load(std::memory_order_relaxed);
}

void WavFileSource::setPositionNormalized(double newPosition) noexcept
{
    const juce::SpinLock::ScopedLockType lock(dataLock);

    if (audioData.getNumSamples() == 0)
        return;

    const auto clamped = juce::jlimit(0.0, 1.0, newPosition);
    const auto samplePosition = static_cast<juce::int64>(clamped * static_cast<double>(audioData.getNumSamples() - 1));
    playbackPosition.store(samplePosition, std::memory_order_relaxed);
}

double WavFileSource::getPositionNormalized() const noexcept
{
    const juce::SpinLock::ScopedLockType lock(dataLock);

    if (audioData.getNumSamples() == 0)
        return 0.0;

    const auto position = static_cast<double>(playbackPosition.load(std::memory_order_relaxed));
    return juce::jlimit(0.0, 1.0, position / static_cast<double>(juce::jmax(1, audioData.getNumSamples() - 1)));
}
