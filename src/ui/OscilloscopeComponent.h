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
    void pushHistorySample(float leftSample, float rightSample) noexcept;

    OscilloscopeBuffer& inputBuffer;
    std::array<float, 1024> leftHistory {};
    std::array<float, 1024> rightHistory {};
    std::array<float, 2048> leftReadScratch {};
    std::array<float, 2048> rightReadScratch {};
    int historyWritePosition = 0;
};
