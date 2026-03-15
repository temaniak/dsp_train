#include "userdsp/UserDspProjectManager.h"

#include "midi/MidiBinding.h"
#include "userdsp/UserDspProjectUtils.h"

#include <utility>

namespace
{
constexpr int projectArchiveVersion = 4;
constexpr int zipCompressionLevel = 9;

juce::Array<juce::var> intsToVar(const std::vector<int>& values)
{
    juce::Array<juce::var> result;

    for (const auto value : values)
        result.add(value);

    return result;
}

std::vector<int> intsFromVar(const juce::var& value)
{
    std::vector<int> result;

    if (const auto* array = value.getArray(); array != nullptr)
    {
        result.reserve(static_cast<std::size_t>(array->size()));

        for (const auto& entry : *array)
            result.push_back(static_cast<int>(entry));
    }

    return result;
}

juce::Array<juce::var> routingArrayToVar(const std::array<int, DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS>& routing)
{
    juce::Array<juce::var> values;

    for (const auto entry : routing)
        values.add(entry);

    return values;
}

std::array<int, DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS> routingArrayFromVar(const juce::var& value,
                                                                          const std::array<int, DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS>& fallback)
{
    auto routing = fallback;

    if (const auto* array = value.getArray(); array != nullptr)
    {
        for (int index = 0; index < juce::jmin(array->size(), DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS); ++index)
            routing[static_cast<std::size_t>(index)] = static_cast<int>(array->getReference(index));
    }

    return routing;
}

juce::var audioStateToVar(const ProjectAudioState& audioState)
{
    auto* object = new juce::DynamicObject();

    auto* preferredObject = new juce::DynamicObject();
    preferredObject->setProperty("valid", audioState.cachedPreferred.valid);
    preferredObject->setProperty("sampleRate", audioState.cachedPreferred.sampleRate);
    preferredObject->setProperty("blockSize", audioState.cachedPreferred.blockSize);
    preferredObject->setProperty("inputChannels", audioState.cachedPreferred.preferredInputChannels);
    preferredObject->setProperty("outputChannels", audioState.cachedPreferred.preferredOutputChannels);
    object->setProperty("preferred", juce::var(preferredObject));

    auto* overrideObject = new juce::DynamicObject();
    overrideObject->setProperty("sampleRateOverridden", audioState.overrides.sampleRateOverridden);
    overrideObject->setProperty("blockSizeOverridden", audioState.overrides.blockSizeOverridden);
    overrideObject->setProperty("inputDeviceOverridden", audioState.overrides.inputDeviceOverridden);
    overrideObject->setProperty("outputDeviceOverridden", audioState.overrides.outputDeviceOverridden);
    overrideObject->setProperty("inputChannelsOverridden", audioState.overrides.inputChannelsOverridden);
    overrideObject->setProperty("outputChannelsOverridden", audioState.overrides.outputChannelsOverridden);
    overrideObject->setProperty("routingOverridden", audioState.overrides.routingOverridden);
    overrideObject->setProperty("sampleRate", audioState.overrides.sampleRate);
    overrideObject->setProperty("blockSize", audioState.overrides.blockSize);
    object->setProperty("overrides", juce::var(overrideObject));

    auto* selectionObject = new juce::DynamicObject();
    selectionObject->setProperty("inputDeviceName", audioState.deviceSelection.inputDeviceName);
    selectionObject->setProperty("outputDeviceName", audioState.deviceSelection.outputDeviceName);
    selectionObject->setProperty("enabledInputChannels", intsToVar(audioState.deviceSelection.enabledInputChannels));
    selectionObject->setProperty("enabledOutputChannels", intsToVar(audioState.deviceSelection.enabledOutputChannels));
    selectionObject->setProperty("inputRouting", routingArrayToVar(audioState.deviceSelection.inputRouting));
    selectionObject->setProperty("outputRouting", routingArrayToVar(audioState.deviceSelection.outputRouting));
    object->setProperty("selection", juce::var(selectionObject));

    auto* actualObject = new juce::DynamicObject();
    actualObject->setProperty("inputDeviceName", audioState.lastKnownActual.inputDeviceName);
    actualObject->setProperty("outputDeviceName", audioState.lastKnownActual.outputDeviceName);
    actualObject->setProperty("sampleRate", audioState.lastKnownActual.sampleRate);
    actualObject->setProperty("blockSize", audioState.lastKnownActual.blockSize);
    actualObject->setProperty("activeInputChannels", audioState.lastKnownActual.activeInputChannels);
    actualObject->setProperty("activeOutputChannels", audioState.lastKnownActual.activeOutputChannels);
    object->setProperty("lastKnownActual", juce::var(actualObject));

    return juce::var(object);
}

juce::Result audioStateFromVar(const juce::var& value, ProjectAudioState& audioState)
{
    audioState = {};

    const auto* object = value.getDynamicObject();

    if (object == nullptr)
        return juce::Result::ok();

    if (const auto* preferredObject = object->getProperty("preferred").getDynamicObject(); preferredObject != nullptr)
    {
        audioState.cachedPreferred.valid = static_cast<bool>(preferredObject->getProperty("valid"));
        audioState.cachedPreferred.sampleRate = static_cast<double>(preferredObject->getProperty("sampleRate"));
        audioState.cachedPreferred.blockSize = static_cast<int>(preferredObject->getProperty("blockSize"));
        audioState.cachedPreferred.preferredInputChannels = juce::jlimit(0, DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS,
                                                                         static_cast<int>(preferredObject->getProperty("inputChannels")));
        audioState.cachedPreferred.preferredOutputChannels = juce::jlimit(0, DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS,
                                                                          static_cast<int>(preferredObject->getProperty("outputChannels")));
    }

    if (const auto* overrideObject = object->getProperty("overrides").getDynamicObject(); overrideObject != nullptr)
    {
        audioState.overrides.sampleRateOverridden = static_cast<bool>(overrideObject->getProperty("sampleRateOverridden"));
        audioState.overrides.blockSizeOverridden = static_cast<bool>(overrideObject->getProperty("blockSizeOverridden"));
        audioState.overrides.inputDeviceOverridden = static_cast<bool>(overrideObject->getProperty("inputDeviceOverridden"));
        audioState.overrides.outputDeviceOverridden = static_cast<bool>(overrideObject->getProperty("outputDeviceOverridden"));
        audioState.overrides.inputChannelsOverridden = static_cast<bool>(overrideObject->getProperty("inputChannelsOverridden"));
        audioState.overrides.outputChannelsOverridden = static_cast<bool>(overrideObject->getProperty("outputChannelsOverridden"));
        audioState.overrides.routingOverridden = static_cast<bool>(overrideObject->getProperty("routingOverridden"));
        audioState.overrides.sampleRate = static_cast<double>(overrideObject->getProperty("sampleRate"));
        audioState.overrides.blockSize = static_cast<int>(overrideObject->getProperty("blockSize"));
    }

    if (const auto* selectionObject = object->getProperty("selection").getDynamicObject(); selectionObject != nullptr)
    {
        audioState.deviceSelection.inputDeviceName = selectionObject->getProperty("inputDeviceName").toString();
        audioState.deviceSelection.outputDeviceName = selectionObject->getProperty("outputDeviceName").toString();
        audioState.deviceSelection.enabledInputChannels = intsFromVar(selectionObject->getProperty("enabledInputChannels"));
        audioState.deviceSelection.enabledOutputChannels = intsFromVar(selectionObject->getProperty("enabledOutputChannels"));
        audioState.deviceSelection.inputRouting = routingArrayFromVar(selectionObject->getProperty("inputRouting"),
                                                                     audioState.deviceSelection.inputRouting);
        audioState.deviceSelection.outputRouting = routingArrayFromVar(selectionObject->getProperty("outputRouting"),
                                                                      audioState.deviceSelection.outputRouting);
    }

    if (const auto* actualObject = object->getProperty("lastKnownActual").getDynamicObject(); actualObject != nullptr)
    {
        audioState.lastKnownActual.inputDeviceName = actualObject->getProperty("inputDeviceName").toString();
        audioState.lastKnownActual.outputDeviceName = actualObject->getProperty("outputDeviceName").toString();
        audioState.lastKnownActual.sampleRate = static_cast<double>(actualObject->getProperty("sampleRate"));
        audioState.lastKnownActual.blockSize = static_cast<int>(actualObject->getProperty("blockSize"));
        audioState.lastKnownActual.activeInputChannels = static_cast<int>(actualObject->getProperty("activeInputChannels"));
        audioState.lastKnownActual.activeOutputChannels = static_cast<int>(actualObject->getProperty("activeOutputChannels"));
    }

    return juce::Result::ok();
}

juce::var nodeToVar(const UserDspProjectNode& node)
{
    auto* object = new juce::DynamicObject();
    object->setProperty("name", node.name);
    object->setProperty("type", node.type == UserDspProjectNodeType::folder ? "folder" : "file");

    juce::Array<juce::var> children;

    for (const auto& child : node.children)
        children.add(nodeToVar(*child));

    object->setProperty("children", children);
    return juce::var(object);
}

std::unique_ptr<UserDspProjectNode> nodeFromVar(const juce::var& value, UserDspProjectNode* parent)
{
    const auto* object = value.getDynamicObject();

    if (object == nullptr)
        return {};

    auto node = std::make_unique<UserDspProjectNode>();
    node->parent = parent;
    node->name = object->getProperty("name").toString();

    const auto typeText = object->getProperty("type").toString();
    node->type = typeText == "file" ? UserDspProjectNodeType::file : UserDspProjectNodeType::folder;

    if (const auto* childrenArray = object->getProperty("children").getArray(); childrenArray != nullptr)
    {
        for (const auto& childValue : *childrenArray)
        {
            if (auto childNode = nodeFromVar(childValue, node.get()))
                node->children.push_back(std::move(childNode));
        }
    }

    return node;
}

juce::String parentPathFromPath(const juce::String& relativePath)
{
    const auto normalised = userdsp::normaliseProjectPath(relativePath);
    const auto lastSeparator = normalised.lastIndexOfChar('/');

    if (lastSeparator < 0)
        return {};

    return normalised.substring(0, lastSeparator);
}

juce::String joinProjectPath(const juce::String& parentPath, const juce::String& childName)
{
    if (parentPath.isEmpty())
        return childName;

    return parentPath + "/" + childName;
}

juce::String buildNodePath(const UserDspProjectNode& node, const UserDspProjectNode* rootNode)
{
    if (&node == rootNode)
        return {};

    juce::StringArray parts;
    const auto* current = &node;

    while (current != nullptr && current != rootNode)
    {
        parts.insert(0, current->name);
        current = current->parent;
    }

    return parts.joinIntoString("/");
}

void sortChildren(UserDspProjectNode& node)
{
    std::sort(node.children.begin(), node.children.end(),
              [] (const std::unique_ptr<UserDspProjectNode>& left, const std::unique_ptr<UserDspProjectNode>& right)
              {
                  if (left->type != right->type)
                      return left->type == UserDspProjectNodeType::folder;

                  return left->name.compareNatural(right->name) < 0;
              });
}

juce::Result validateFolderName(const juce::String& folderName)
{
    const auto trimmed = userdsp::sanitiseProjectItemName(folderName);

    if (trimmed.isEmpty())
        return juce::Result::fail("Folder name is required.");

    if (trimmed.containsAnyOf(":*?\"<>|"))
        return juce::Result::fail("Folder name contains unsupported characters.");

    return juce::Result::ok();
}

juce::Array<juce::var> controllerDefinitionsToVar(const std::vector<UserDspControllerDefinition>& definitions)
{
    juce::Array<juce::var> values;

    for (const auto& definition : definitions)
    {
        auto* object = new juce::DynamicObject();
        object->setProperty("type", userDspControllerTypeToStableString(definition.type));
        object->setProperty("label", definition.label);
        object->setProperty("codeName", definition.codeName);
        object->setProperty("midiBindingHint", definition.midiBindingHint);

        if (midi::isBindingActive(definition.midiBinding))
        {
            auto* midiBindingObject = new juce::DynamicObject();
            midiBindingObject->setProperty("source", midi::midiBindingSourceToStableString(definition.midiBinding.source));
            midiBindingObject->setProperty("channel", definition.midiBinding.channel);
            midiBindingObject->setProperty("data1", definition.midiBinding.data1);
            object->setProperty("midiBinding", juce::var(midiBindingObject));
        }

        values.add(juce::var(object));
    }

    return values;
}

juce::Result controllerDefinitionsFromVar(const juce::var& value, std::vector<UserDspControllerDefinition>& definitions)
{
    definitions.clear();

    const auto* values = value.getArray();

    if (values == nullptr)
        return juce::Result::ok();

    definitions.reserve(static_cast<std::size_t>(values->size()));

    for (const auto& entry : *values)
    {
        const auto* object = entry.getDynamicObject();

        if (object == nullptr)
            return juce::Result::fail("The controller metadata is invalid.");

        UserDspControllerDefinition definition;
        definition.type = userDspControllerTypeFromStableString(object->getProperty("type").toString());
        definition.label = object->getProperty("label").toString().trim();
        definition.codeName = object->getProperty("codeName").toString().trim();
        definition.midiBindingHint = object->getProperty("midiBindingHint").toString().trim();

        if (const auto* midiBindingObject = object->getProperty("midiBinding").getDynamicObject(); midiBindingObject != nullptr)
        {
            definition.midiBinding.source = midi::midiBindingSourceFromStableString(midiBindingObject->getProperty("source").toString());
            definition.midiBinding.channel = static_cast<int>(midiBindingObject->getProperty("channel"));
            definition.midiBinding.data1 = static_cast<int>(midiBindingObject->getProperty("data1"));
            definition.midiBinding = midi::sanitiseMidiBinding(definition.midiBinding);
        }

        definitions.push_back(std::move(definition));
    }

    return userdsp::validateControllerDefinitions(definitions);
}
} // namespace

UserDspProjectManager::UserDspProjectManager()
{
    document.addListener(this);
    resetToNewProjectState();
}

UserDspProjectManager::~UserDspProjectManager()
{
    document.removeListener(this);
}

juce::CodeDocument& UserDspProjectManager::getDocument() noexcept
{
    return document;
}

juce::CPlusPlusCodeTokeniser& UserDspProjectManager::getTokeniser() noexcept
{
    return tokeniser;
}

const UserDspProjectNode& UserDspProjectManager::getRootNode() const noexcept
{
    jassert(rootNode != nullptr);
    return *rootNode;
}

const UserDspProjectNode* UserDspProjectManager::getNodeForPath(const juce::String& relativePath) const noexcept
{
    return const_cast<UserDspProjectManager*>(this)->findNodeForPath(relativePath);
}

juce::String UserDspProjectManager::getProjectName() const
{
    return projectName;
}

juce::File UserDspProjectManager::getArchiveFile() const
{
    return archiveFile;
}

juce::String UserDspProjectManager::getActiveFilePath() const
{
    return activeFilePath;
}

juce::String UserDspProjectManager::getProcessorClassName() const
{
    return processorClassName;
}

const std::vector<UserDspControllerDefinition>& UserDspProjectManager::getControllerDefinitions() const noexcept
{
    return controllerDefinitions;
}

const ProjectAudioState& UserDspProjectManager::getAudioState() const noexcept
{
    return audioState;
}

bool UserDspProjectManager::hasUnsavedChanges() const noexcept
{
    return dirty;
}

void UserDspProjectManager::setProcessorClassName(const juce::String& newProcessorClassName)
{
    const auto trimmed = newProcessorClassName.trim();

    if (processorClassName == trimmed)
        return;

    processorClassName = trimmed;
    setDirty(true);
}

void UserDspProjectManager::setAudioState(const ProjectAudioState& newAudioState, bool markDirty)
{
    if (audioState == newAudioState)
        return;

    audioState = newAudioState;

    if (markDirty)
        setDirty(true);
}

juce::Result UserDspProjectManager::addController(UserDspControllerType type)
{
    if (controllerDefinitions.size() >= static_cast<std::size_t>(DSP_EDU_USER_DSP_MAX_CONTROLS))
    {
        return juce::Result::fail("A DSP project can define at most "
                                  + juce::String(DSP_EDU_USER_DSP_MAX_CONTROLS)
                                  + " controllers.");
    }

    UserDspControllerDefinition definition;
    definition.type = type;

    int ordinal = 1;

    while (true)
    {
        definition.label = userdsp::makeDefaultControllerLabel(type, ordinal);
        definition.codeName = userdsp::makeDefaultControllerCodeName(type, ordinal);

        const auto duplicate = std::any_of(controllerDefinitions.begin(), controllerDefinitions.end(),
                                           [&definition] (const auto& existing) { return existing.codeName == definition.codeName; });

        if (! duplicate)
            break;

        ++ordinal;
    }

    controllerDefinitions.push_back(std::move(definition));
    setDirty(true);
    return juce::Result::ok();
}

juce::Result UserDspProjectManager::updateController(int index, const UserDspControllerDefinition& definition)
{
    if (! juce::isPositiveAndBelow(index, static_cast<int>(controllerDefinitions.size())))
        return juce::Result::fail("The selected controller does not exist.");

    auto updatedDefinition = definition;
    updatedDefinition.label = updatedDefinition.label.trim();
    updatedDefinition.codeName = updatedDefinition.codeName.trim();
    updatedDefinition.midiBinding = midi::sanitiseMidiBinding(updatedDefinition.midiBinding);
    updatedDefinition.midiBindingHint = updatedDefinition.midiBindingHint.trim();

    if (midi::isBindingActive(updatedDefinition.midiBinding))
        updatedDefinition.midiBindingHint = midi::buildMidiBindingSummary(updatedDefinition.midiBinding);

    if (const auto validationResult = userdsp::validateControllerDefinition(updatedDefinition); validationResult.failed())
        return validationResult;

    for (int otherIndex = 0; otherIndex < static_cast<int>(controllerDefinitions.size()); ++otherIndex)
    {
        if (otherIndex != index && controllerDefinitions[static_cast<std::size_t>(otherIndex)].codeName == updatedDefinition.codeName)
            return juce::Result::fail("Controller code names must be unique.");
    }

    if (controllerDefinitions[static_cast<std::size_t>(index)] == updatedDefinition)
        return juce::Result::ok();

    controllerDefinitions[static_cast<std::size_t>(index)] = std::move(updatedDefinition);
    setDirty(true);
    return juce::Result::ok();
}

juce::Result UserDspProjectManager::removeController(int index)
{
    if (! juce::isPositiveAndBelow(index, static_cast<int>(controllerDefinitions.size())))
        return juce::Result::fail("The selected controller does not exist.");

    controllerDefinitions.erase(controllerDefinitions.begin() + index);
    setDirty(true);
    return juce::Result::ok();
}

juce::Result UserDspProjectManager::moveController(int sourceIndex, int destinationIndex)
{
    if (! juce::isPositiveAndBelow(sourceIndex, static_cast<int>(controllerDefinitions.size()))
        || ! juce::isPositiveAndBelow(destinationIndex, static_cast<int>(controllerDefinitions.size())))
    {
        return juce::Result::fail("The selected controller cannot be moved.");
    }

    if (sourceIndex == destinationIndex)
        return juce::Result::ok();

    auto movedDefinition = controllerDefinitions[static_cast<std::size_t>(sourceIndex)];
    controllerDefinitions.erase(controllerDefinitions.begin() + sourceIndex);
    controllerDefinitions.insert(controllerDefinitions.begin() + destinationIndex, std::move(movedDefinition));
    setDirty(true);
    return juce::Result::ok();
}

juce::Result UserDspProjectManager::createNewProject()
{
    resetToNewProjectState();
    return juce::Result::ok();
}

juce::Result UserDspProjectManager::loadProjectArchive(const juce::File& fileToLoad)
{
    if (! fileToLoad.existsAsFile())
        return juce::Result::fail("The selected project file does not exist.");

    juce::ZipFile zipFile(fileToLoad);
    const auto* projectEntry = zipFile.getEntry("project.json");

    if (projectEntry == nullptr)
        return juce::Result::fail("The selected project archive does not contain project.json.");

    std::unique_ptr<juce::InputStream> projectStream(zipFile.createStreamForEntry(*projectEntry));

    if (projectStream == nullptr)
        return juce::Result::fail("Failed to read project.json from the archive.");

    const auto projectText = projectStream->readEntireStreamAsString();
    const auto projectValue = juce::JSON::parse(projectText);
    const auto* projectObject = projectValue.getDynamicObject();

    if (projectObject == nullptr)
        return juce::Result::fail("project.json is not valid JSON.");

    const auto archiveVersion = static_cast<int>(projectObject->getProperty("version"));

    if (archiveVersion < 1 || archiveVersion > projectArchiveVersion)
        return juce::Result::fail("Unsupported DSP project version.");

    auto loadedRoot = nodeFromVar(projectObject->getProperty("tree"), nullptr);

    if (loadedRoot == nullptr || loadedRoot->type != UserDspProjectNodeType::folder)
        return juce::Result::fail("The project tree is missing or invalid.");

    std::vector<UserDspProjectNode*> fileNodes;
    std::function<void(UserDspProjectNode&)> attachFileContents = [&] (UserDspProjectNode& node)
    {
        if (node.type == UserDspProjectNodeType::file)
        {
            const auto relativePath = buildNodePath(node, loadedRoot.get());
            const auto storedPath = "files/" + userdsp::normaliseProjectPath(relativePath);
            const auto* fileEntry = zipFile.getEntry(storedPath);

            if (fileEntry == nullptr)
                throw juce::Result::fail("Project archive is missing " + storedPath + ".");

            std::unique_ptr<juce::InputStream> fileStream(zipFile.createStreamForEntry(*fileEntry));

            if (fileStream == nullptr)
                throw juce::Result::fail("Failed to read " + storedPath + " from the archive.");

            node.content = fileStream->readEntireStreamAsString();
            fileNodes.push_back(&node);
            return;
        }

        for (auto& child : node.children)
            attachFileContents(*child);
    };

    try
    {
        attachFileContents(*loadedRoot);
    }
    catch (const juce::Result& error)
    {
        return error;
    }

    rootNode = std::move(loadedRoot);
    projectName = projectObject->getProperty("projectName").toString();
    archiveFile = fileToLoad;
    processorClassName = projectObject->getProperty("processorClass").toString().trim();
    activeFilePath = userdsp::normaliseProjectPath(projectObject->getProperty("activeFilePath").toString());

    if (const auto controllerLoadResult = controllerDefinitionsFromVar(projectObject->getProperty("controllers"), controllerDefinitions);
        controllerLoadResult.failed())
    {
        return controllerLoadResult;
    }

    if (const auto audioStateLoadResult = audioStateFromVar(projectObject->getProperty("audioState"), audioState); audioStateLoadResult.failed())
        return audioStateLoadResult;

    if (projectName.isEmpty())
        projectName = archiveFile.getFileNameWithoutExtension();

    if (processorClassName.isEmpty())
        processorClassName = userdsp::getDefaultTemplateProcessorClassName();

    if (activeFilePath.isEmpty() || getNodeForPath(activeFilePath) == nullptr || getNodeForPath(activeFilePath)->type != UserDspProjectNodeType::file)
    {
        activeFilePath.clear();

        if (! fileNodes.empty())
            activeFilePath = getNodePath(*fileNodes.front());
    }

    if (activeFilePath.isNotEmpty())
        loadFileIntoEditor(activeFilePath);
    else
        loadFileIntoEditor({});

    setDirty(false);
    return juce::Result::ok();
}

juce::Result UserDspProjectManager::saveProject()
{
    if (archiveFile == juce::File())
        return juce::Result::fail("Project file is not set. Use Save Project As.");

    return saveProjectAs(archiveFile);
}

juce::Result UserDspProjectManager::saveProjectAs(const juce::File& fileToSave)
{
    syncActiveDocumentToProject();

    juce::ZipFile::Builder builder;
    auto* projectObject = new juce::DynamicObject();
    projectObject->setProperty("version", projectArchiveVersion);
    projectObject->setProperty("projectName", projectName);
    projectObject->setProperty("processorClass", processorClassName);
    projectObject->setProperty("activeFilePath", activeFilePath);
    projectObject->setProperty("controllers", controllerDefinitionsToVar(controllerDefinitions));
    projectObject->setProperty("audioState", audioStateToVar(audioState));
    projectObject->setProperty("tree", nodeToVar(*rootNode));

    const auto projectText = juce::JSON::toString(juce::var(projectObject), true);
    builder.addEntry(new juce::MemoryInputStream(projectText.toRawUTF8(),
                                                 static_cast<size_t>(projectText.getNumBytesAsUTF8()),
                                                 true),
                     zipCompressionLevel,
                     "project.json",
                     juce::Time::getCurrentTime());

    const auto now = juce::Time::getCurrentTime();
    std::vector<UserDspProjectFileSnapshot> files;
    collectFileSnapshots(*rootNode, files);

    for (const auto& fileSnapshot : files)
    {
        builder.addEntry(new juce::MemoryInputStream(fileSnapshot.content.toRawUTF8(),
                                                     static_cast<size_t>(fileSnapshot.content.getNumBytesAsUTF8()),
                                                     true),
                         zipCompressionLevel,
                         "files/" + fileSnapshot.relativePath,
                         now);
    }

    if (auto parentDirectory = fileToSave.getParentDirectory(); ! parentDirectory.exists())
        parentDirectory.createDirectory();

    juce::TemporaryFile temporaryArchive(fileToSave);
    std::unique_ptr<juce::FileOutputStream> outputStream(temporaryArchive.getFile().createOutputStream());

    if (outputStream == nullptr)
        return juce::Result::fail("Failed to create the DSP project archive.");

    if (! builder.writeToStream(*outputStream, nullptr))
        return juce::Result::fail("Failed to write the DSP project archive.");

    outputStream->flush();
    outputStream.reset();

    if (! temporaryArchive.overwriteTargetFileWithTemporary())
        return juce::Result::fail("Failed to replace the DSP project archive.");

    archiveFile = fileToSave;
    projectName = archiveFile.getFileNameWithoutExtension();
    setDirty(false);
    return juce::Result::ok();
}

juce::Result UserDspProjectManager::selectFile(const juce::String& relativePath)
{
    const auto normalisedPath = userdsp::normaliseProjectPath(relativePath);
    auto* node = findNodeForPath(normalisedPath);

    if (node == nullptr || node->type != UserDspProjectNodeType::file)
        return juce::Result::fail("The selected project item is not a file.");

    syncActiveDocumentToProject();
    activeFilePath = normalisedPath;
    loadFileIntoEditor(activeFilePath);
    return juce::Result::ok();
}

juce::Result UserDspProjectManager::createFile(const juce::String& parentFolderPath, const juce::String& fileName)
{
    if (const auto validation = userdsp::validateSupportedProjectFileName(fileName); validation.failed())
        return validation;

    auto* parentNode = findFolderForPath(parentFolderPath);

    if (parentNode == nullptr)
        return juce::Result::fail("The target folder was not found.");

    const auto safeName = makeUniqueChildName(*parentNode, userdsp::sanitiseProjectItemName(fileName));
    auto* fileNode = addFileNode(*parentNode, safeName, {});
    sortChildren(*parentNode);
    const auto newPath = getNodePath(*fileNode);
    setDirty(true);
    return selectFile(newPath);
}

juce::Result UserDspProjectManager::createFolder(const juce::String& parentFolderPath, const juce::String& folderName)
{
    if (const auto validation = validateFolderName(folderName); validation.failed())
        return validation;

    auto* parentNode = findFolderForPath(parentFolderPath);

    if (parentNode == nullptr)
        return juce::Result::fail("The target folder was not found.");

    const auto safeName = makeUniqueChildName(*parentNode, userdsp::sanitiseProjectItemName(folderName));
    addFolderNode(*parentNode, safeName);
    sortChildren(*parentNode);
    setDirty(true);
    return juce::Result::ok();
}

juce::Result UserDspProjectManager::renameItem(const juce::String& relativePath, const juce::String& newName)
{
    const auto normalisedPath = userdsp::normaliseProjectPath(relativePath);
    auto* node = findNodeForPath(normalisedPath);

    if (node == nullptr || node == rootNode.get())
        return juce::Result::fail("The selected project item cannot be renamed.");

    const auto cleanName = userdsp::sanitiseProjectItemName(newName);
    const auto validation = node->type == UserDspProjectNodeType::file
                                ? userdsp::validateSupportedProjectFileName(cleanName)
                                : validateFolderName(cleanName);

    if (validation.failed())
        return validation;

    const auto previousPath = getNodePath(*node);
    const auto parentPath = parentPathFromPath(previousPath);
    const auto candidatePath = joinProjectPath(parentPath, cleanName);

    if (previousPath != candidatePath && pathExists(candidatePath))
        return juce::Result::fail("An item with that name already exists in the destination folder.");

    node->name = cleanName;

    if (activeFilePath == previousPath || pathIsDescendantOf(activeFilePath, previousPath))
        activeFilePath = joinProjectPath(parentPath, cleanName) + activeFilePath.substring(previousPath.length());

    setDirty(true);
    return juce::Result::ok();
}

juce::Result UserDspProjectManager::moveItem(const juce::String& sourcePath, const juce::String& destinationFolderPath)
{
    const auto normalisedSourcePath = userdsp::normaliseProjectPath(sourcePath);
    const auto normalisedDestinationPath = userdsp::normaliseProjectPath(destinationFolderPath);
    auto* sourceNode = findNodeForPath(normalisedSourcePath);
    auto* destinationNode = findFolderForPath(normalisedDestinationPath);

    if (sourceNode == nullptr || sourceNode == rootNode.get())
        return juce::Result::fail("The selected project item cannot be moved.");

    if (destinationNode == nullptr)
        return juce::Result::fail("The destination folder was not found.");

    if (sourceNode->type == UserDspProjectNodeType::folder
        && (normalisedDestinationPath == normalisedSourcePath || pathIsDescendantOf(normalisedDestinationPath, normalisedSourcePath)))
    {
        return juce::Result::fail("A folder cannot be moved into itself or one of its descendants.");
    }

    const auto sourceParentPath = parentPathFromPath(normalisedSourcePath);
    const auto destinationCandidate = joinProjectPath(normalisedDestinationPath, sourceNode->name);

    if (sourceParentPath == normalisedDestinationPath)
        return juce::Result::ok();

    if (pathExists(destinationCandidate))
        return juce::Result::fail("An item with the same name already exists in the destination folder.");

    auto* sourceParent = sourceNode->parent;

    if (sourceParent == nullptr)
        return juce::Result::fail("The selected project item has no parent.");

    auto iterator = std::find_if(sourceParent->children.begin(), sourceParent->children.end(),
                                 [sourceNode] (const std::unique_ptr<UserDspProjectNode>& child) { return child.get() == sourceNode; });

    if (iterator == sourceParent->children.end())
        return juce::Result::fail("The selected project item could not be moved.");

    auto movedNode = std::move(*iterator);
    sourceParent->children.erase(iterator);
    movedNode->parent = destinationNode;
    destinationNode->children.push_back(std::move(movedNode));
    sortChildren(*sourceParent);
    sortChildren(*destinationNode);

    if (activeFilePath == normalisedSourcePath || pathIsDescendantOf(activeFilePath, normalisedSourcePath))
        activeFilePath = destinationCandidate + activeFilePath.substring(normalisedSourcePath.length());

    setDirty(true);
    return juce::Result::ok();
}

juce::Result UserDspProjectManager::deleteItem(const juce::String& relativePath)
{
    const auto normalisedPath = userdsp::normaliseProjectPath(relativePath);
    auto* node = findNodeForPath(normalisedPath);

    if (node == nullptr || node == rootNode.get())
        return juce::Result::fail("The selected project item cannot be deleted.");

    auto* parentNode = node->parent;

    if (parentNode == nullptr)
        return juce::Result::fail("The selected project item has no parent.");

    const auto affectsActiveFile = activeFilePath == normalisedPath || pathIsDescendantOf(activeFilePath, normalisedPath);

    auto iterator = std::find_if(parentNode->children.begin(), parentNode->children.end(),
                                 [node] (const std::unique_ptr<UserDspProjectNode>& child) { return child.get() == node; });

    if (iterator == parentNode->children.end())
        return juce::Result::fail("The selected project item could not be deleted.");

    parentNode->children.erase(iterator);

    if (affectsActiveFile)
    {
        activeFilePath.clear();
        loadFileIntoEditor({});

        std::vector<UserDspProjectFileSnapshot> files;
        collectFileSnapshots(*rootNode, files);

        if (! files.empty())
            selectFile(files.front().relativePath);
    }

    setDirty(true);
    return juce::Result::ok();
}

juce::Result UserDspProjectManager::importFile(const juce::File& fileToImport,
                                               const juce::String& destinationFolderPath,
                                               juce::String* importedRelativePath)
{
    if (! fileToImport.existsAsFile())
        return juce::Result::fail("The selected code file does not exist.");

    if (fileToImport.getFileExtension().toLowerCase() != ".cpp")
        return juce::Result::fail("Only .cpp files can be imported into a DSP project.");

    auto* destinationFolder = findFolderForPath(destinationFolderPath);

    if (destinationFolder == nullptr)
        return juce::Result::fail("The destination folder was not found.");

    const auto importedSource = fileToImport.loadFileAsString();
    auto strippedImport = userdsp::stripLegacySdkWrappers(importedSource);
    const auto importedIntoFreshTemplate = matchesDefaultTemplateProject();

    if (strippedImport.strippedSource.trim().isEmpty())
        return juce::Result::fail("The selected source file became empty after removing legacy SDK wrappers.");

    const auto safeName = makeUniqueChildName(*destinationFolder, userdsp::sanitiseProjectItemName(fileToImport.getFileName()));
    auto* fileNode = addFileNode(*destinationFolder, safeName, strippedImport.strippedSource);
    sortChildren(*destinationFolder);
    const auto relativePath = getNodePath(*fileNode);

    if (importedIntoFreshTemplate && strippedImport.detectedProcessorClassName.isNotEmpty())
        processorClassName = strippedImport.detectedProcessorClassName.trim();

    setDirty(true);

    if (importedRelativePath != nullptr)
        *importedRelativePath = relativePath;

    return selectFile(relativePath);
}

juce::Result UserDspProjectManager::exportFile(const juce::String& relativePath, const juce::File& outputFile)
{
    syncActiveDocumentToProject();
    const auto* node = getNodeForPath(relativePath);

    if (node == nullptr || node->type != UserDspProjectNodeType::file)
        return juce::Result::fail("The selected project item is not a file.");

    if (! outputFile.replaceWithText(node->content))
        return juce::Result::fail("Failed to export the selected file.");

    return juce::Result::ok();
}

std::vector<juce::String> UserDspProjectManager::getAllFolderPaths(const juce::String& excludedRootPath) const
{
    std::vector<juce::String> paths;
    collectFolderPaths(*rootNode, paths);

    if (excludedRootPath.isEmpty())
        return paths;

    std::vector<juce::String> filtered;

    for (const auto& path : paths)
    {
        if (! pathIsDescendantOf(path, excludedRootPath) && path != userdsp::normaliseProjectPath(excludedRootPath))
            filtered.push_back(path);
    }

    return filtered;
}

juce::StringArray UserDspProjectManager::getAllowedMoveDestinations(const juce::String& sourcePath) const
{
    juce::StringArray destinations;
    const auto normalisedSourcePath = userdsp::normaliseProjectPath(sourcePath);
    const auto* sourceNode = getNodeForPath(normalisedSourcePath);

    if (sourceNode == nullptr)
        return destinations;

    for (const auto& folderPath : getAllFolderPaths(sourceNode->type == UserDspProjectNodeType::folder ? normalisedSourcePath : juce::String()))
    {
        if (parentPathFromPath(normalisedSourcePath) != folderPath)
            destinations.add(folderPath);
    }

    return destinations;
}

juce::String UserDspProjectManager::describePath(const juce::String& relativePath) const
{
    const auto normalisedPath = userdsp::normaliseProjectPath(relativePath);

    if (normalisedPath.isEmpty())
        return projectName;

    return normalisedPath;
}

UserDspProjectSnapshot UserDspProjectManager::createCompileSnapshot()
{
    syncActiveDocumentToProject();

    UserDspProjectSnapshot snapshot;
    snapshot.projectName = projectName;
    snapshot.processorClassName = processorClassName;
    snapshot.activeFilePath = activeFilePath;
    snapshot.controllers = controllerDefinitions;
    collectFileSnapshots(*rootNode, snapshot.files);
    return snapshot;
}

void UserDspProjectManager::codeDocumentTextInserted(const juce::String&, int)
{
    if (! suppressDocumentNotifications)
        setDirty(true);
}

void UserDspProjectManager::codeDocumentTextDeleted(int, int)
{
    if (! suppressDocumentNotifications)
        setDirty(true);
}

void UserDspProjectManager::resetToNewProjectState()
{
    rootNode = std::make_unique<UserDspProjectNode>();
    rootNode->type = UserDspProjectNodeType::folder;
    rootNode->name = {};
    rootNode->parent = nullptr;

    projectName = "Untitled Project";
    archiveFile = juce::File();
    processorClassName = userdsp::getDefaultTemplateProcessorClassName();
    controllerDefinitions = userdsp::getDefaultTemplateControllers();
    audioState = {};
    audioState.cachedPreferred.preferredInputChannels = 2;
    audioState.cachedPreferred.preferredOutputChannels = 2;

    auto* srcFolder = addFolderNode(*rootNode, "src");
    auto* mainFile = addFileNode(*srcFolder, "main.cpp", userdsp::getDefaultTemplateSource());
    sortChildren(*rootNode);
    sortChildren(*srcFolder);

    activeFilePath = getNodePath(*mainFile);
    loadFileIntoEditor(activeFilePath);
    setDirty(true);
}

void UserDspProjectManager::setDirty(bool shouldBeDirty)
{
    dirty = shouldBeDirty;
}

void UserDspProjectManager::syncActiveDocumentToProject()
{
    if (activeFilePath.isEmpty())
        return;

    if (auto* activeNode = findNodeForPath(activeFilePath); activeNode != nullptr && activeNode->type == UserDspProjectNodeType::file)
        activeNode->content = document.getAllContent();
}

void UserDspProjectManager::loadFileIntoEditor(const juce::String& relativePath)
{
    suppressDocumentNotifications = true;

    if (relativePath.isEmpty())
    {
        document.replaceAllContent({});
    }
    else if (const auto* node = getNodeForPath(relativePath); node != nullptr && node->type == UserDspProjectNodeType::file)
    {
        document.replaceAllContent(node->content);
    }

    suppressDocumentNotifications = false;
}

juce::String UserDspProjectManager::getNodePath(const UserDspProjectNode& node) const
{
    if (&node == rootNode.get())
        return {};

    juce::StringArray parts;
    const auto* current = &node;

    while (current != nullptr && current != rootNode.get())
    {
        parts.insert(0, current->name);
        current = current->parent;
    }

    return parts.joinIntoString("/");
}

UserDspProjectNode* UserDspProjectManager::findNodeForPath(const juce::String& relativePath) noexcept
{
    const auto normalisedPath = userdsp::normaliseProjectPath(relativePath);

    if (normalisedPath.isEmpty())
        return rootNode.get();

    auto pathParts = juce::StringArray::fromTokens(normalisedPath, "/", {});
    auto* current = rootNode.get();

    for (const auto& part : pathParts)
    {
        auto iterator = std::find_if(current->children.begin(), current->children.end(),
                                     [&part] (const std::unique_ptr<UserDspProjectNode>& child) { return child->name == part; });

        if (iterator == current->children.end())
            return nullptr;

        current = iterator->get();
    }

    return current;
}

UserDspProjectNode* UserDspProjectManager::findFolderForPath(const juce::String& relativePath) noexcept
{
    auto* node = findNodeForPath(relativePath);
    return node != nullptr && node->type == UserDspProjectNodeType::folder ? node : nullptr;
}

const UserDspProjectNode* UserDspProjectManager::getFolderForPath(const juce::String& relativePath) const noexcept
{
    return const_cast<UserDspProjectManager*>(this)->findFolderForPath(relativePath);
}

bool UserDspProjectManager::pathExists(const juce::String& relativePath) const
{
    return getNodeForPath(relativePath) != nullptr;
}

bool UserDspProjectManager::pathIsDescendantOf(const juce::String& candidatePath, const juce::String& parentPath) const
{
    const auto normalisedCandidate = userdsp::normaliseProjectPath(candidatePath);
    const auto normalisedParent = userdsp::normaliseProjectPath(parentPath);

    if (normalisedCandidate.isEmpty() || normalisedParent.isEmpty())
        return false;

    return normalisedCandidate.startsWith(normalisedParent + "/");
}

UserDspProjectNode* UserDspProjectManager::addFolderNode(UserDspProjectNode& parentNode, const juce::String& folderName)
{
    auto node = std::make_unique<UserDspProjectNode>();
    node->type = UserDspProjectNodeType::folder;
    node->name = folderName;
    node->parent = &parentNode;
    auto* result = node.get();
    parentNode.children.push_back(std::move(node));
    return result;
}

UserDspProjectNode* UserDspProjectManager::addFileNode(UserDspProjectNode& parentNode,
                                                       const juce::String& fileName,
                                                       const juce::String& content)
{
    auto node = std::make_unique<UserDspProjectNode>();
    node->type = UserDspProjectNodeType::file;
    node->name = fileName;
    node->content = content;
    node->parent = &parentNode;
    auto* result = node.get();
    parentNode.children.push_back(std::move(node));
    return result;
}

void UserDspProjectManager::collectFileSnapshots(const UserDspProjectNode& node, std::vector<UserDspProjectFileSnapshot>& files) const
{
    if (node.type == UserDspProjectNodeType::file)
    {
        files.push_back({ getNodePath(node), node.content });
        return;
    }

    for (const auto& child : node.children)
        collectFileSnapshots(*child, files);
}

void UserDspProjectManager::collectFolderPaths(const UserDspProjectNode& node, std::vector<juce::String>& paths) const
{
    if (node.type != UserDspProjectNodeType::folder)
        return;

    paths.push_back(getNodePath(node));

    for (const auto& child : node.children)
        collectFolderPaths(*child, paths);
}

bool UserDspProjectManager::matchesDefaultTemplateProject() const
{
    const auto* srcFolder = getFolderForPath("src");
    const auto* mainFile = getNodeForPath("src/main.cpp");

    if (srcFolder == nullptr || mainFile == nullptr || mainFile->type != UserDspProjectNodeType::file)
        return false;

    if (rootNode->children.size() != 1 || srcFolder->children.size() != 1)
        return false;

    if (controllerDefinitions != userdsp::getDefaultTemplateControllers())
        return false;

    return processorClassName == userdsp::getDefaultTemplateProcessorClassName()
        && mainFile->content.trim() == userdsp::getDefaultTemplateSource().trim();
}

juce::String UserDspProjectManager::makeUniqueChildName(const UserDspProjectNode& parentNode, const juce::String& preferredName) const
{
    const auto extension = juce::File(preferredName).getFileExtension();
    const auto stem = extension.isNotEmpty() ? preferredName.dropLastCharacters(extension.length()) : preferredName;

    auto candidate = preferredName;
    int suffix = 2;

    auto exists = [&parentNode] (const juce::String& name)
    {
        return std::any_of(parentNode.children.begin(), parentNode.children.end(),
                           [&name] (const std::unique_ptr<UserDspProjectNode>& child) { return child->name == name; });
    };

    while (exists(candidate))
    {
        candidate = extension.isNotEmpty()
                        ? stem + "_" + juce::String(suffix) + extension
                        : preferredName + "_" + juce::String(suffix);
        ++suffix;
    }

    return candidate;
}
