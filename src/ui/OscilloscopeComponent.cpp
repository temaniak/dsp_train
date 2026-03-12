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

    for (int index = 0; index < static_cast<int>(history.size()); ++index)
    {
        const auto sampleIndex = (historyWritePosition + index) % static_cast<int>(history.size());
        const auto x = bounds.getX() + bounds.getWidth() * (static_cast<float>(index) / static_cast<float>(history.size() - 1));
        const auto y = juce::jmap(history[static_cast<std::size_t>(sampleIndex)], -1.1f, 1.1f, bounds.getBottom(), bounds.getY());

        if (index == 0)
            waveform.startNewSubPath(x, y);
        else
            waveform.lineTo(x, y);
    }

    g.setColour(juce::Colour(0xff4fc1ff));
    g.strokePath(waveform, juce::PathStrokeType(2.0f));
}

void OscilloscopeComponent::resized()
{
}

void OscilloscopeComponent::timerCallback()
{
    while (const auto copied = inputBuffer.popAvailable(readScratch.data(), static_cast<int>(readScratch.size())))
        for (int sample = 0; sample < copied; ++sample)
            pushHistorySample(readScratch[static_cast<std::size_t>(sample)]);

    repaint();
}

void OscilloscopeComponent::pushHistorySample(float sample) noexcept
{
    history[static_cast<std::size_t>(historyWritePosition)] = sample;
    historyWritePosition = (historyWritePosition + 1) % static_cast<int>(history.size());
}
