#include "app/DspEducationStandApplication.h"

#include "app/MainWindow.h"

DspEducationStandApplication::~DspEducationStandApplication() = default;

const juce::String DspEducationStandApplication::getApplicationName()
{
    return "DSP Education Stand";
}

const juce::String DspEducationStandApplication::getApplicationVersion()
{
    return "0.1.0";
}

bool DspEducationStandApplication::moreThanOneInstanceAllowed()
{
    return true;
}

void DspEducationStandApplication::initialise(const juce::String&)
{
    mainWindow = std::make_unique<MainWindow>(getApplicationName());
}

void DspEducationStandApplication::shutdown()
{
    mainWindow.reset();
}

void DspEducationStandApplication::systemRequestedQuit()
{
    if (mainWindow == nullptr || mainWindow->tryToCloseWindow())
        quit();
}

void DspEducationStandApplication::anotherInstanceStarted(const juce::String&)
{
}
