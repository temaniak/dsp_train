#include "app/MainWindow.h"

#include "app/MainComponent.h"

MainWindow::MainWindow(const juce::String& name)
    : juce::DocumentWindow(name, ide::background, juce::DocumentWindow::allButtons)
{
    setLookAndFeel(&lookAndFeel);
    setUsingNativeTitleBar(false);
    setTitleBarHeight(36);
    setTitleBarButtonsRequired(juce::DocumentWindow::allButtons,
#if JUCE_MAC
                               true
#else
                               false
#endif
    );
    setColour(juce::ResizableWindow::backgroundColourId, ide::background);
    setColour(juce::DocumentWindow::textColourId, ide::text);
    setContentOwned(new MainComponent(), true);
    setResizable(true, true);
    centreWithSize(1400, 900);
    setVisible(true);
}

MainWindow::~MainWindow()
{
    setLookAndFeel(nullptr);
}

bool MainWindow::tryToCloseWindow()
{
    if (auto* mainComponent = dynamic_cast<MainComponent*>(getContentComponent()); mainComponent != nullptr)
        return mainComponent->handleCloseRequest();

    return true;
}

void MainWindow::closeButtonPressed()
{
    if (tryToCloseWindow())
        juce::JUCEApplication::getInstance()->quit();
}
