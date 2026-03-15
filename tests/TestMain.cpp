#include <JuceHeader.h>

#include <cmath>

#include "audio/AudioConfiguration.h"
#include "midi/MidiBinding.h"
#include "midi/MidiPerformanceState.h"
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

UserDspControllerDefinition makeController(UserDspControllerType type,
                                           const juce::String& label,
                                           const juce::String& codeName,
                                           MidiBinding binding = {},
                                           juce::String midiBindingHint = {})
{
    UserDspControllerDefinition definition;
    definition.type = type;
    definition.label = label;
    definition.codeName = codeName;
    definition.midiBinding = binding;
    definition.midiBindingHint = midiBindingHint;
    return definition;
}

bool approximatelyEqual(float lhs, float rhs, float epsilon = 1.0e-4f)
{
    return std::abs(lhs - rhs) <= epsilon;
}

juce::Result writeLegacyProjectArchive(const juce::File& archiveFile)
{
    juce::ZipFile::Builder builder;
    auto* projectObject = new juce::DynamicObject();
    projectObject->setProperty("version", 3);
    projectObject->setProperty("projectName", "Legacy MIDI Hint Project");
    projectObject->setProperty("processorClass", "MyAudioProcessor");
    projectObject->setProperty("activeFilePath", "src/main.cpp");

    juce::Array<juce::var> controllers;
    auto* controllerObject = new juce::DynamicObject();
    controllerObject->setProperty("type", "knob");
    controllerObject->setProperty("label", "Legacy Gain");
    controllerObject->setProperty("codeName", "legacyGain");
    controllerObject->setProperty("midiBindingHint", "CC 74 / Ch 3");
    controllers.add(juce::var(controllerObject));
    projectObject->setProperty("controllers", controllers);

    auto* rootNode = new juce::DynamicObject();
    rootNode->setProperty("name", {});
    rootNode->setProperty("type", "folder");

    auto* srcNode = new juce::DynamicObject();
    srcNode->setProperty("name", "src");
    srcNode->setProperty("type", "folder");

    auto* mainNode = new juce::DynamicObject();
    mainNode->setProperty("name", "main.cpp");
    mainNode->setProperty("type", "file");
    mainNode->setProperty("children", juce::Array<juce::var>());

    juce::Array<juce::var> srcChildren;
    srcChildren.add(juce::var(mainNode));
    srcNode->setProperty("children", srcChildren);

    juce::Array<juce::var> rootChildren;
    rootChildren.add(juce::var(srcNode));
    rootNode->setProperty("children", rootChildren);

    projectObject->setProperty("tree", juce::var(rootNode));
    const auto projectText = juce::JSON::toString(juce::var(projectObject), true);

    builder.addEntry(new juce::MemoryInputStream(projectText.toRawUTF8(),
                                                 static_cast<size_t>(projectText.getNumBytesAsUTF8()),
                                                 true),
                     9,
                     "project.json",
                     juce::Time::getCurrentTime());

    const auto mainSource = juce::String(
        "class MyAudioProcessor\n"
        "{\n"
        "public:\n"
        "    void prepareToPlay(double) {}\n"
        "    void processAudio(float*, int) {}\n"
        "};\n");

    builder.addEntry(new juce::MemoryInputStream(mainSource.toRawUTF8(),
                                                 static_cast<size_t>(mainSource.getNumBytesAsUTF8()),
                                                 true),
                     9,
                     "files/src/main.cpp",
                     juce::Time::getCurrentTime());

    juce::TemporaryFile temporaryArchive(archiveFile);
    std::unique_ptr<juce::FileOutputStream> outputStream(temporaryArchive.getFile().createOutputStream());

    if (outputStream == nullptr)
        return juce::Result::fail("Failed to create legacy project archive.");

    if (! builder.writeToStream(*outputStream, nullptr))
        return juce::Result::fail("Failed to write legacy project archive.");

    outputStream->flush();
    outputStream.reset();

    if (! temporaryArchive.overwriteTargetFileWithTemporary())
        return juce::Result::fail("Failed to replace legacy project archive.");

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

    if (! projectManager.getControllerDefinitions().empty())
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

    const MidiBinding driveBinding { MidiBindingSource::cc, 1, 21 };
    const MidiBinding fireBinding { MidiBindingSource::noteGate, 1, 60 };
    const MidiBinding bypassBinding { MidiBindingSource::cc, 1, 64 };

    if (projectManager.updateController(0, makeController(UserDspControllerType::knob, "Drive Amount", "driveAmount", driveBinding)).failed())
        return 10;

    if (projectManager.updateController(1, makeController(UserDspControllerType::button, "Fire", "fireButton", fireBinding)).failed())
        return 11;

    if (projectManager.updateController(2, makeController(UserDspControllerType::toggle, "Bypass", "bypassToggle", bypassBinding)).failed())
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
        || loadedControllers[0] != makeController(UserDspControllerType::toggle,
                                                  "Bypass",
                                                  "bypassToggle",
                                                  bypassBinding,
                                                  midi::buildMidiBindingSummary(bypassBinding))
        || loadedControllers[1] != makeController(UserDspControllerType::knob,
                                                  "Drive Amount",
                                                  "driveAmount",
                                                  driveBinding,
                                                  midi::buildMidiBindingSummary(driveBinding))
        || loadedControllers[2] != makeController(UserDspControllerType::button,
                                                  "Fire",
                                                  "fireButton",
                                                  fireBinding,
                                                  midi::buildMidiBindingSummary(fireBinding))
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

    UserDspProjectManager midiBindingProjectManager;

    if (midiBindingProjectManager.addController(UserDspControllerType::knob).failed()
        || midiBindingProjectManager.addController(UserDspControllerType::button).failed()
        || midiBindingProjectManager.addController(UserDspControllerType::knob).failed()
        || midiBindingProjectManager.addController(UserDspControllerType::knob).failed()
        || midiBindingProjectManager.addController(UserDspControllerType::knob).failed())
    {
        return 21;
    }

    const std::array<UserDspControllerDefinition, 5> midiRoundTripDefinitions
    {
        makeController(UserDspControllerType::knob, "Macro", "macro", MidiBinding { MidiBindingSource::cc, 2, 74 }),
        makeController(UserDspControllerType::button, "Gate", "gate", MidiBinding { MidiBindingSource::noteGate, 3, 61 }),
        makeController(UserDspControllerType::knob, "Velocity", "velocity", MidiBinding { MidiBindingSource::noteVelocity, 4, 62 }),
        makeController(UserDspControllerType::knob, "Note Number", "noteNumber", MidiBinding { MidiBindingSource::noteNumber, 5, 63 }),
        makeController(UserDspControllerType::knob, "Pitch", "pitch", MidiBinding { MidiBindingSource::pitchWheel, 6, -1 })
    };

    for (int index = 0; index < static_cast<int>(midiRoundTripDefinitions.size()); ++index)
    {
        if (midiBindingProjectManager.updateController(index, midiRoundTripDefinitions[static_cast<std::size_t>(index)]).failed())
            return 22 + index;
    }

    const auto midiRoundTripArchive = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                        .getChildFile("dsp_education_stand_midi_roundtrip.dspedu");

    if (const auto saveMidiRoundTrip = midiBindingProjectManager.saveProjectAs(midiRoundTripArchive); saveMidiRoundTrip.failed())
        return 28;

    UserDspProjectManager loadedMidiBindingProjectManager;

    if (const auto loadMidiRoundTrip = loadedMidiBindingProjectManager.loadProjectArchive(midiRoundTripArchive); loadMidiRoundTrip.failed())
        return 29;

    const auto& loadedMidiBindingDefinitions = loadedMidiBindingProjectManager.getControllerDefinitions();

    if (loadedMidiBindingDefinitions.size() != midiRoundTripDefinitions.size())
        return 30;

    for (int index = 0; index < static_cast<int>(midiRoundTripDefinitions.size()); ++index)
    {
        auto expected = midiRoundTripDefinitions[static_cast<std::size_t>(index)];
        expected.midiBindingHint = midi::buildMidiBindingSummary(expected.midiBinding);

        if (loadedMidiBindingDefinitions[static_cast<std::size_t>(index)] != expected)
            return 31 + index;
    }

    const auto legacyProjectArchive = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                        .getChildFile("dsp_education_stand_legacy_midi_hint.dspedu");

    if (const auto writeLegacyProject = writeLegacyProjectArchive(legacyProjectArchive); writeLegacyProject.failed())
        return 40;

    UserDspProjectManager legacyProjectManager;

    if (const auto loadLegacyProject = legacyProjectManager.loadProjectArchive(legacyProjectArchive); loadLegacyProject.failed())
        return 41;

    const auto& legacyControllers = legacyProjectManager.getControllerDefinitions();

    if (legacyControllers.size() != 1
        || legacyControllers[0].midiBinding != MidiBinding {}
        || legacyControllers[0].midiBindingHint != "CC 74 / Ch 3")
    {
        return 42;
    }

    float mappedValue = 0.0f;

    if (! midi::tryMapMessageToBindingValue(juce::MidiMessage::controllerEvent(1, 21, 96), driveBinding, mappedValue)
        || ! approximatelyEqual(mappedValue, 96.0f / 127.0f))
    {
        return 43;
    }

    if (! midi::tryMapMessageToBindingValue(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100)), fireBinding, mappedValue)
        || ! approximatelyEqual(mappedValue, 1.0f)
        || ! midi::tryMapMessageToBindingValue(juce::MidiMessage::noteOff(1, 60), fireBinding, mappedValue)
        || ! approximatelyEqual(mappedValue, 0.0f))
    {
        return 44;
    }

    const MidiBinding velocityBinding { MidiBindingSource::noteVelocity, 1, 60 };

    if (! midi::tryMapMessageToBindingValue(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(64)), velocityBinding, mappedValue)
        || ! approximatelyEqual(mappedValue, 64.0f / 127.0f)
        || ! midi::tryMapMessageToBindingValue(juce::MidiMessage::noteOff(1, 60), velocityBinding, mappedValue)
        || ! approximatelyEqual(mappedValue, 0.0f))
    {
        return 45;
    }

    const MidiBinding noteNumberBinding { MidiBindingSource::noteNumber, 1, 60 };

    if (! midi::tryMapMessageToBindingValue(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(99)), noteNumberBinding, mappedValue)
        || ! approximatelyEqual(mappedValue, 60.0f / 127.0f)
        || ! midi::tryMapMessageToBindingValue(juce::MidiMessage::noteOff(1, 60), noteNumberBinding, mappedValue)
        || ! approximatelyEqual(mappedValue, 0.0f))
    {
        return 46;
    }

    const MidiBinding pitchWheelBinding { MidiBindingSource::pitchWheel, 3, -1 };

    if (! midi::tryMapMessageToBindingValue(juce::MidiMessage::pitchWheel(3, 8192), pitchWheelBinding, mappedValue)
        || ! approximatelyEqual(mappedValue, 8192.0f / 16383.0f, 1.0e-3f))
    {
        return 47;
    }

    MidiBinding capturedBinding;

    if (! midi::tryCaptureMidiLearnBinding(MidiBindingSource::cc,
                                           juce::MidiMessage::controllerEvent(7, 12, 99),
                                           capturedBinding)
        || capturedBinding != MidiBinding { MidiBindingSource::cc, 7, 12 })
    {
        return 48;
    }

    if (! midi::tryCaptureMidiLearnBinding(MidiBindingSource::noteGate,
                                           juce::MidiMessage::noteOn(2, 61, static_cast<juce::uint8>(90)),
                                           capturedBinding)
        || capturedBinding != MidiBinding { MidiBindingSource::noteGate, 2, 61 })
    {
        return 49;
    }

    if (! midi::tryCaptureMidiLearnBinding(MidiBindingSource::pitchWheel,
                                           juce::MidiMessage::pitchWheel(4, 4096),
                                           capturedBinding)
        || capturedBinding != MidiBinding { MidiBindingSource::pitchWheel, 4, -1 })
    {
        return 50;
    }

    if (midi::tryCaptureMidiLearnBinding(MidiBindingSource::noteVelocity,
                                         juce::MidiMessage::controllerEvent(1, 20, 64),
                                         capturedBinding))
    {
        return 51;
    }

    MidiPerformanceState performanceState;
    performanceState.setPitchWheel(1, -0.25f);
    performanceState.noteOn(1, 60, 0.75f);

    const auto& firstMidiState = performanceState.getState();

    if (firstMidiState.voiceCount != 1
        || firstMidiState.gate != 1
        || firstMidiState.channel != 1
        || firstMidiState.noteNumber != 60
        || ! approximatelyEqual(firstMidiState.velocity, 0.75f)
        || ! approximatelyEqual(firstMidiState.pitchWheel, -0.25f)
        || firstMidiState.voices[0].active != 1
        || firstMidiState.voices[0].noteNumber != 60)
    {
        return 52;
    }

    performanceState.noteOn(1, 64, 0.5f);
    const auto& stackedMidiState = performanceState.getState();

    if (stackedMidiState.voiceCount != 2
        || stackedMidiState.gate != 1
        || stackedMidiState.noteNumber != 64
        || ! approximatelyEqual(stackedMidiState.velocity, 0.5f))
    {
        return 53;
    }

    performanceState.noteOff(1, 64);
    const auto& releasedMidiState = performanceState.getState();

    if (releasedMidiState.voiceCount != 1
        || releasedMidiState.gate != 0
        || releasedMidiState.noteNumber != 64
        || ! approximatelyEqual(releasedMidiState.velocity, 0.0f)
        || (releasedMidiState.voices[0].active == 0 && releasedMidiState.voices[1].active == 0))
    {
        return 54;
    }

    const auto generatedHeader = userdsp::buildGeneratedControlsHeader(
        { makeController(UserDspControllerType::knob, "Cutoff", "cutoff") });

    if (! generatedHeader.contains("const DspEduMidiState& getMidiState() noexcept;")
        || ! generatedHeader.contains("inline const auto& midi = ::dspedu::getMidiState();"))
    {
        return 55;
    }

    const auto wrapperSource = userdsp::buildWrapperSourceSnippet(
        { makeController(UserDspControllerType::knob, "Cutoff", "cutoff") },
        "MyAudioProcessor");

    if (! wrapperSource.contains("DspEduMidiState gDspEduMidiState;")
        || ! wrapperSource.contains("void setMidiState(const DspEduMidiState& state) noexcept")
        || ! wrapperSource.contains("dspedu::setMidiState(state);"))
    {
        return 56;
    }

    return 0;
}
