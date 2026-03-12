#include <JuceHeader.h>

#include "dsp/BuiltinProcessors.h"
#include "presets/PresetManager.h"
#include "userdsp/UserDspProjectManager.h"
#include "userdsp/UserDspProjectUtils.h"

namespace
{
bool approximatelyEqual(float a, float b, float epsilon = 1.0e-4f)
{
    return std::abs(a - b) <= epsilon;
}

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

    BuiltinProcessorChain processor;
    processor.prepare(48000.0, 512);
    processor.setSelectedProcessor(BuiltinProcessorType::gain);
    processor.resetParametersToDefaults(BuiltinProcessorType::gain);
    processor.setParameter(0, 0.5f);

    std::array<float, 4> input { 1.0f, -1.0f, 0.25f, -0.25f };
    std::array<float, 4> output {};
    processor.process(input.data(), output.data(), static_cast<int>(input.size()));

    if (! approximatelyEqual(output[0], 0.5f) || ! approximatelyEqual(output[1], -0.5f))
        return 1;

    PresetManager presetManager;
    BuiltinPresetData preset;
    preset.processorId = "simpleDelay";
    preset.parameterValues = { 120.0f, 0.4f, 0.25f, 0.0f };

    const auto tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                            .getChildFile("dsp_education_stand_preset_test.json");

    if (const auto saveResult = presetManager.savePreset(tempFile, preset); saveResult.failed())
        return 2;

    BuiltinPresetData loadedPreset;

    if (const auto loadResult = presetManager.loadPreset(tempFile, loadedPreset); loadResult.failed())
        return 3;

    if (loadedPreset.processorId != preset.processorId || ! approximatelyEqual(loadedPreset.parameterValues[1], preset.parameterValues[1]))
        return 4;

    UserDspProjectManager projectManager;

    if (projectManager.getActiveFilePath() != "src/ExampleUserProcessor.cpp"
        || projectManager.getProcessorClassName() != "ExampleUserProcessor")
    {
        return 5;
    }

    const auto& starterControllers = projectManager.getControllerDefinitions();

    if (starterControllers != userdsp::getDefaultTemplateControllers())
        return 6;

    if (projectManager.createFolder("include", "filters").failed())
        return 7;

    if (projectManager.createFile("include/filters", "OnePole.h").failed())
        return 8;

    projectManager.getDocument().replaceAllContent("struct OnePole { float z = 0.0f; };\n");

    if (projectManager.selectFile("src/ExampleUserProcessor.cpp").failed())
        return 9;

    projectManager.getDocument().replaceAllContent(
        "#include \"include/filters/OnePole.h\"\n\n"
        "class ExampleUserProcessor\n"
        "{\n"
        "public:\n"
        "    void prepareToPlay(double) {}\n"
        "    void processAudio(float*, int) {}\n"
        "};\n");

    projectManager.setProcessorClassName("ExampleUserProcessor");

    for (int index = static_cast<int>(projectManager.getControllerDefinitions().size()) - 1; index >= 0; --index)
        if (projectManager.removeController(index).failed())
            return 10;

    if (projectManager.addController(UserDspControllerType::knob).failed())
        return 11;

    if (projectManager.addController(UserDspControllerType::button).failed())
        return 12;

    if (projectManager.addController(UserDspControllerType::toggle).failed())
        return 13;

    if (projectManager.updateController(0, { UserDspControllerType::knob, "Drive Amount", "driveAmount" }).failed())
        return 14;

    if (projectManager.updateController(1, { UserDspControllerType::button, "Fire", "fireButton" }).failed())
        return 15;

    if (projectManager.updateController(2, { UserDspControllerType::toggle, "Bypass", "bypassToggle" }).failed())
        return 16;

    if (projectManager.moveController(2, 0).failed())
        return 17;

    const auto projectArchive = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                  .getChildFile("dsp_education_stand_project_test.dspedu");

    if (const auto saveProjectResult = projectManager.saveProjectAs(projectArchive); saveProjectResult.failed())
        return 18;

    UserDspProjectManager loadedProjectManager;

    if (const auto loadProjectResult = loadedProjectManager.loadProjectArchive(projectArchive); loadProjectResult.failed())
        return 19;

    const auto& loadedControllers = loadedProjectManager.getControllerDefinitions();

    if (loadedProjectManager.getProcessorClassName() != "ExampleUserProcessor"
        || loadedProjectManager.getNodeForPath("include/filters/OnePole.h") == nullptr
        || loadedProjectManager.getActiveFilePath() != "src/ExampleUserProcessor.cpp"
        || loadedControllers.size() != 3
        || loadedControllers[0] != UserDspControllerDefinition { UserDspControllerType::toggle, "Bypass", "bypassToggle" }
        || loadedControllers[1] != UserDspControllerDefinition { UserDspControllerType::knob, "Drive Amount", "driveAmount" }
        || loadedControllers[2] != UserDspControllerDefinition { UserDspControllerType::button, "Fire", "fireButton" })
    {
        return 20;
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
        return 21;

    juce::String importedRelativePath;

    if (const auto importResult = importedProjectManager.importFile(legacyImportFile, "src", &importedRelativePath); importResult.failed())
        return 22;

    if (importedProjectManager.getProcessorClassName() != "MyAudioProcessor"
        || importedRelativePath.isEmpty())
    {
        return 23;
    }

    const auto* importedNode = importedProjectManager.getNodeForPath(importedRelativePath);

    if (importedNode == nullptr
        || importedNode->content.contains("UserDspApi.h")
        || importedNode->content.contains("DSP_EDU_DEFINE_SIMPLE_PLUGIN"))
    {
        return 24;
    }

    return 0;
}
