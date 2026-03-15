#include "util/AppSettings.h"

#include "util/AppPaths.h"

namespace
{
constexpr int appSettingsVersion = 1;
}

namespace appsettings
{
AppSettingsState loadAppSettings()
{
    AppSettingsState settings;
    const auto settingsFile = apppaths::getSettingsFile();

    if (! settingsFile.existsAsFile())
        return settings;

    const auto settingsText = settingsFile.loadFileAsString();
    const auto parsedValue = juce::JSON::parse(settingsText);
    const auto* object = parsedValue.getDynamicObject();

    if (object == nullptr)
        return settings;

    settings.selectedMidiInputDeviceName = object->getProperty("selectedMidiInputDeviceName").toString().trim();
    return settings;
}

juce::Result saveAppSettings(const AppSettingsState& settings)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("version", appSettingsVersion);
    object->setProperty("selectedMidiInputDeviceName", settings.selectedMidiInputDeviceName.trim());
    const auto settingsText = juce::JSON::toString(juce::var(object), true);
    const auto settingsFile = apppaths::getSettingsFile();

    if (auto parentDirectory = settingsFile.getParentDirectory(); ! parentDirectory.exists())
        parentDirectory.createDirectory();

    juce::TemporaryFile temporaryFile(settingsFile);

    if (! temporaryFile.getFile().replaceWithText(settingsText))
        return juce::Result::fail("Failed to write application settings.");

    if (! temporaryFile.overwriteTargetFileWithTemporary())
        return juce::Result::fail("Failed to replace application settings.");

    return juce::Result::ok();
}
} // namespace appsettings
