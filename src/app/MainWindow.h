#pragma once

#include <JuceHeader.h>

class MainWindow final : public juce::DocumentWindow
{
public:
    explicit MainWindow(const juce::String& name);

    void closeButtonPressed() override;
};
