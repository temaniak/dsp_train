#pragma once

#include "ui/OscilloscopeBuffer.h"

class OscilloscopeComponent final : public juce::Component,
                                    private juce::Timer
{
public:
    explicit OscilloscopeComponent(OscilloscopeBuffer& buffer);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void setRefreshSuspended(bool shouldSuspend);

private:
    void timerCallback() override;
    void pushHistorySample(float sample) noexcept;

    OscilloscopeBuffer& inputBuffer;
    std::array<float, 1024> history {};
    std::array<float, 2048> readScratch {};
    int historyWritePosition = 0;
};
