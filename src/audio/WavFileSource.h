#pragma once

#include <JuceHeader.h>

class WavFileSource
{
public:
    juce::Result loadFromFile(const juce::File& file, juce::AudioFormatManager& formatManager);
    void clear();
    void reset();
    void prepare(double sampleRate, int maxBlockSize);
    void generate(float* output, int numSamples);

    bool hasFileLoaded() const noexcept;
    juce::String getLoadedFileName() const;

    void setLooping(bool shouldLoop) noexcept;
    bool isLooping() const noexcept;

    void setPositionNormalized(double newPosition) noexcept;
    double getPositionNormalized() const noexcept;

private:
    mutable juce::SpinLock dataLock;
    juce::AudioBuffer<float> audioData;
    juce::String loadedFileName;
    std::atomic<juce::int64> playbackPosition { 0 };
    std::atomic<bool> looping { false };
};
