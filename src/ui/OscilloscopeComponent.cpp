#include "ui/OscilloscopeComponent.h"

OscilloscopeComponent::OscilloscopeComponent(OscilloscopeBuffer& buffer)
    : inputBuffer(buffer)
{
    startTimerHz(30);
}

void OscilloscopeComponent::setRefreshSuspended(bool shouldSuspend)
{
    if (shouldSuspend)
    {
        stopTimer();
        return;
    }

    startTimerHz(30);
}

void OscilloscopeComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1e1e1e));

    auto bounds = getLocalBounds().toFloat().reduced(8.0f);
    g.setColour(juce::Colour(0xff3c3c3c));
    g.drawRect(bounds, 1.0f);
    g.setColour(juce::Colour(0xff2a2d2e));
    g.drawHorizontalLine(static_cast<int>(bounds.getCentreY()), bounds.getX(), bounds.getRight());

    juce::Path waveform;
    juce::Path rightWaveform;

    for (int index = 0; index < static_cast<int>(leftHistory.size()); ++index)
    {
        const auto sampleIndex = (historyWritePosition + index) % static_cast<int>(leftHistory.size());
        const auto x = bounds.getX() + bounds.getWidth() * (static_cast<float>(index) / static_cast<float>(leftHistory.size() - 1));
        const auto leftY = juce::jmap(leftHistory[static_cast<std::size_t>(sampleIndex)], -1.1f, 1.1f, bounds.getBottom(), bounds.getY());
        const auto rightY = juce::jmap(rightHistory[static_cast<std::size_t>(sampleIndex)], -1.1f, 1.1f, bounds.getBottom(), bounds.getY());

        if (index == 0)
        {
            waveform.startNewSubPath(x, leftY);
            rightWaveform.startNewSubPath(x, rightY);
        }
        else
        {
            waveform.lineTo(x, leftY);
            rightWaveform.lineTo(x, rightY);
        }
    }

    g.setColour(juce::Colour(0xff4fc1ff));
    g.strokePath(waveform, juce::PathStrokeType(2.0f));
    g.setColour(juce::Colour(0xffffb35c).withAlpha(0.85f));
    g.strokePath(rightWaveform, juce::PathStrokeType(1.6f));
}

void OscilloscopeComponent::resized()
{
}

void OscilloscopeComponent::timerCallback()
{
    while (const auto copied = inputBuffer.popAvailable(leftReadScratch.data(),
                                                        rightReadScratch.data(),
                                                        static_cast<int>(leftReadScratch.size())))
        for (int sample = 0; sample < copied; ++sample)
            pushHistorySample(leftReadScratch[static_cast<std::size_t>(sample)],
                              rightReadScratch[static_cast<std::size_t>(sample)]);

    repaint();
}

void OscilloscopeComponent::pushHistorySample(float leftSample, float rightSample) noexcept
{
    leftHistory[static_cast<std::size_t>(historyWritePosition)] = leftSample;
    rightHistory[static_cast<std::size_t>(historyWritePosition)] = rightSample;
    historyWritePosition = (historyWritePosition + 1) % static_cast<int>(leftHistory.size());
}
