#include "ui/ToolWindow.h"

namespace
{
constexpr int toolWindowTitleBarHeight = 34;
}

ToolWindow::ToolWindow(const juce::String& title,
                       std::unique_ptr<juce::Component> content,
                       int width,
                       int height,
                       int minWidth,
                       int minHeight)
    : juce::DocumentWindow(title, ide::background, juce::DocumentWindow::closeButton)
{
   #if JUCE_MAC
    constexpr bool titleButtonsOnLeft = true;
   #else
    constexpr bool titleButtonsOnLeft = false;
   #endif

    setLookAndFeel(&lookAndFeel);
    setUsingNativeTitleBar(false);
    setTitleBarHeight(toolWindowTitleBarHeight);
    setTitleBarButtonsRequired(juce::DocumentWindow::closeButton, titleButtonsOnLeft);
    setResizable(true, true);
    setResizeLimits(minWidth, minHeight, 8192, 8192);
    setColour(juce::ResizableWindow::backgroundColourId, ide::background);
    setColour(juce::DocumentWindow::textColourId, ide::text);
    setContentOwned(content.release(), true);
    centreWithSize(width, height);
}

ToolWindow::~ToolWindow()
{
    setLookAndFeel(nullptr);
}

void ToolWindow::present(juce::Component* parentComponent)
{
    if (! isVisible())
    {
        if (parentComponent != nullptr)
            centreAroundComponent(parentComponent, getWidth(), getHeight());

        setVisible(true);
    }

    toFront(true);
}

juce::Component* ToolWindow::getToolContent() const noexcept
{
    return getContentComponent();
}

void ToolWindow::closeButtonPressed()
{
    setVisible(false);
}
