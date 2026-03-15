#pragma once

#include <JuceHeader.h>

struct AppSettingsState
{
    juce::String selectedMidiInputDeviceName;
};

namespace appsettings
{
AppSettingsState loadAppSettings();
juce::Result saveAppSettings(const AppSettingsState& settings);
} // namespace appsettings
