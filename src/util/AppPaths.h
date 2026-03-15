#pragma once

#include <JuceHeader.h>

namespace apppaths
{
inline juce::File getLocalAppDataRoot()
{
    const auto localAppData = juce::SystemStats::getEnvironmentVariable("LOCALAPPDATA", {});

    if (localAppData.isNotEmpty())
        return juce::File(localAppData);

    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
}

inline juce::File getAppRoot()
{
    return getLocalAppDataRoot().getChildFile("DSP Education Stand");
}

inline juce::File getWorkspaceRoot()
{
    return getAppRoot().getChildFile("user_dsp");
}

inline juce::File getSettingsFile()
{
    return getAppRoot().getChildFile("settings.json");
}

inline juce::File findProjectResource(const juce::String& relativePath)
{
    const auto executableFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    const auto currentDirectory = juce::File::getCurrentWorkingDirectory();

    auto search = [relativePath] (juce::File base) -> juce::File
    {
        for (int depth = 0; depth < 10 && base.exists(); ++depth)
        {
            const auto candidate = base.getChildFile(relativePath);

            if (candidate.exists())
                return candidate;

            const auto parent = base.getParentDirectory();

            if (parent == base)
                break;

            base = parent;
        }

        return {};
    };

    if (auto candidate = search(executableFile.getParentDirectory()); candidate.exists())
        return candidate;

    return search(currentDirectory);
}
} // namespace apppaths
