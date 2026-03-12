#pragma once

#include <JuceHeader.h>

#include "app/MainWindow.h"

class DspEducationStandApplication final : public juce::JUCEApplication
{
public:
    ~DspEducationStandApplication() override;

    const juce::String getApplicationName() override;
    const juce::String getApplicationVersion() override;
    bool moreThanOneInstanceAllowed() override;

    void initialise(const juce::String& commandLine) override;
    void shutdown() override;
    void systemRequestedQuit() override;
    void anotherInstanceStarted(const juce::String& commandLine) override;

private:
    std::unique_ptr<MainWindow> mainWindow;
};
