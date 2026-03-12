#pragma once

#include <JuceHeader.h>

#include "UserDspApi.h"

class WavFileSource
{
public:
    juce::Result loadFromFile(const juce::File& file, juce::AudioFormatManager& formatManager);
    void clear();
    void reset();
    void prepare(double sampleRate, int maxBlockSize, int maxChannels = DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS);
    void generate(float* const* outputs, int numChannels, int numSamples);

    bool hasFileLoaded() const noexcept;
    juce::String getLoadedFileName() const;
    int getChannelCount() const noexcept;

    void setLooping(bool shouldLoop) noexcept;
    bool isLooping() const noexcept;

    void setPositionNormalized(double newPosition) noexcept;
    double getPositionNormalized() const noexcept;

private:
    mutable juce::SpinLock dataLock;
    juce::AudioBuffer<float> audioData;
    juce::String loadedFileName;
    int loadedChannelCount = 0;
    std::atomic<juce::int64> playbackPosition { 0 };
    std::atomic<bool> looping { false };
};
