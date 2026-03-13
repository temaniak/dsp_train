#include <JuceHeader.h>

#include "audio/AudioConfiguration.h"
#include "userdsp/UserDspProjectManager.h"
#include "userdsp/UserDspProjectUtils.h"

namespace
{
juce::Result writeTextFile(const juce::File& file, const juce::String& content)
{
    if (auto parent = file.getParentDirectory(); ! parent.exists())
        parent.createDirectory();

    if (! file.replaceWithText(content))
        return juce::Result::fail("Failed to write " + file.getFullPathName());

    return juce::Result::ok();
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    UserDspProjectManager projectManager;

    if (projectManager.getActiveFilePath() != "src/main.cpp"
        || projectManager.getProcessorClassName() != "MyAudioProcessor")
    {
        return 1;
    }

    const auto& starterControllers = projectManager.getControllerDefinitions();

    if (! starterControllers.empty())
        return 2;

    if (projectManager.createFolder({}, "include").failed())
        return 3;

    if (projectManager.createFolder("include", "filters").failed())
        return 4;

    if (projectManager.createFile("include/filters", "OnePole.h").failed())
        return 5;

    projectManager.getDocument().replaceAllContent("struct OnePole { float z = 0.0f; };\n");

    if (projectManager.selectFile("src/main.cpp").failed())
        return 6;

    projectManager.getDocument().replaceAllContent(
        "#include \"include/filters/OnePole.h\"\n\n"
        "class MyAudioProcessor\n"
        "{\n"
        "public:\n"
        "    void prepareToPlay(double) {}\n"
        "    void processAudio(float*, int) {}\n"
        "};\n");

    projectManager.setProcessorClassName("MyAudioProcessor");

    if (projectManager.addController(UserDspControllerType::knob).failed())
        return 7;

    if (projectManager.addController(UserDspControllerType::button).failed())
        return 8;

    if (projectManager.addController(UserDspControllerType::toggle).failed())
        return 9;

    if (projectManager.updateController(0, { UserDspControllerType::knob, "Drive Amount", "driveAmount", "CC 21 / Ch 1" }).failed())
        return 10;

    if (projectManager.updateController(1, { UserDspControllerType::button, "Fire", "fireButton", "Note C3 / Ch 1" }).failed())
        return 11;

    if (projectManager.updateController(2, { UserDspControllerType::toggle, "Bypass", "bypassToggle", "CC 64 / Ch 1" }).failed())
        return 12;

    if (projectManager.moveController(2, 0).failed())
        return 13;

    ProjectAudioState audioState;
    audioState.cachedPreferred.valid = true;
    audioState.cachedPreferred.sampleRate = 48000.0;
    audioState.cachedPreferred.blockSize = 256;
    audioState.cachedPreferred.preferredInputChannels = 2;
    audioState.cachedPreferred.preferredOutputChannels = 2;
    audioState.overrides.sampleRateOverridden = true;
    audioState.overrides.sampleRate = 44100.0;
    audioState.overrides.outputChannelsOverridden = true;
    audioState.deviceSelection.inputDeviceName = "Input Device";
    audioState.deviceSelection.outputDeviceName = "Output Device";
    audioState.deviceSelection.enabledInputChannels = { 0, 1 };
    audioState.deviceSelection.enabledOutputChannels = { 0, 1 };
    audioState.deviceSelection.inputRouting = { 0, 1 };
    audioState.deviceSelection.outputRouting = { 1, 0 };
    audioState.lastKnownActual.inputDeviceName = "Input Device";
    audioState.lastKnownActual.outputDeviceName = "Output Device";
    audioState.lastKnownActual.sampleRate = 44100.0;
    audioState.lastKnownActual.blockSize = 512;
    audioState.lastKnownActual.activeInputChannels = 1;
    audioState.lastKnownActual.activeOutputChannels = 2;
    projectManager.setAudioState(audioState);

    const auto projectArchive = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                  .getChildFile("dsp_education_stand_project_test.dspedu");

    if (const auto saveProjectResult = projectManager.saveProjectAs(projectArchive); saveProjectResult.failed())
        return 14;

    UserDspProjectManager loadedProjectManager;

    if (const auto loadProjectResult = loadedProjectManager.loadProjectArchive(projectArchive); loadProjectResult.failed())
        return 15;

    const auto& loadedControllers = loadedProjectManager.getControllerDefinitions();
    const auto& loadedAudioState = loadedProjectManager.getAudioState();

    if (loadedProjectManager.getProcessorClassName() != "MyAudioProcessor"
        || loadedProjectManager.getNodeForPath("include/filters/OnePole.h") == nullptr
        || loadedProjectManager.getActiveFilePath() != "src/main.cpp"
        || loadedControllers.size() != 3
        || loadedControllers[0] != UserDspControllerDefinition { UserDspControllerType::toggle, "Bypass", "bypassToggle", "CC 64 / Ch 1" }
        || loadedControllers[1] != UserDspControllerDefinition { UserDspControllerType::knob, "Drive Amount", "driveAmount", "CC 21 / Ch 1" }
        || loadedControllers[2] != UserDspControllerDefinition { UserDspControllerType::button, "Fire", "fireButton", "Note C3 / Ch 1" }
        || loadedAudioState != audioState)
    {
        return 16;
    }

    UserDspProjectManager importedProjectManager;
    const auto legacyImportFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                    .getChildFile("dsp_education_stand_legacy_import.cpp");

    const auto legacySource = juce::String(
        "#include \"UserDspApi.h\"\n\n"
        "class MyAudioProcessor\n"
        "{\n"
        "public:\n"
        "    void prepareToPlay(double) {}\n"
        "    void processAudio(float*, int) {}\n"
        "};\n\n"
        "DSP_EDU_DEFINE_SIMPLE_PLUGIN(MyAudioProcessor)\n");

    if (const auto writeLegacyResult = writeTextFile(legacyImportFile, legacySource); writeLegacyResult.failed())
        return 17;

    juce::String importedRelativePath;

    if (const auto importResult = importedProjectManager.importFile(legacyImportFile, "src", &importedRelativePath); importResult.failed())
        return 18;

    if (importedProjectManager.getProcessorClassName() != "MyAudioProcessor"
        || importedRelativePath.isEmpty())
    {
        return 19;
    }

    const auto* importedNode = importedProjectManager.getNodeForPath(importedRelativePath);

    if (importedNode == nullptr
        || importedNode->content.contains("UserDspApi.h")
        || importedNode->content.contains("DSP_EDU_DEFINE_SIMPLE_PLUGIN"))
    {
        return 20;
    }

    return 0;
}
