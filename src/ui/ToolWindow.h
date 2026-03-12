#pragma once

#include <JuceHeader.h>

#include "ui/DarkIdeLookAndFeel.h"

class ToolWindow final : public juce::DocumentWindow
{
public:
    ToolWindow(const juce::String& title,
               std::unique_ptr<juce::Component> content,
               int width,
               int height,
               int minWidth,
               int minHeight);
    ~ToolWindow() override;

    void present(juce::Component* parentComponent);

    juce::Component* getToolContent() const noexcept;

    void closeButtonPressed() override;

private:
    ide::DarkIdeLookAndFeel lookAndFeel;
};
