#pragma once

#include <JuceHeader.h>

#include "ui/DarkIdeLookAndFeel.h"

class MainWindow final : public juce::DocumentWindow
{
public:
    explicit MainWindow(const juce::String& name);
    ~MainWindow() override;

    bool tryToCloseWindow();
    void closeButtonPressed() override;

private:
    ide::DarkIdeLookAndFeel lookAndFeel;
};
