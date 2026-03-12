#include "userdsp/UserDspCompiler.h"

#include "userdsp/UserDspProjectUtils.h"
#include "util/AppPaths.h"

namespace
{
struct UserDspBuildInfo
{
    juce::String moduleExtension;
    juce::String debugSymbolsExtension;
    juce::String scriptRelativePath;
    juce::StringArray launchCommand;
};

struct ValidatedProjectLayout
{
    juce::String classDefinitionFilePath;
    bool classDefinitionInHeader = false;
};

UserDspBuildInfo getUserDspBuildInfo()
{
#if JUCE_WINDOWS
    return { ".dll", ".pdb", "scripts/build_user_dsp.cmd", { "cmd", "/c" } };
#elif JUCE_MAC
    return { ".dylib", ".dSYM", "scripts/build_user_dsp.sh", { "/bin/sh" } };
#else
    return { ".so", ".debug", "scripts/build_user_dsp.sh", { "/bin/sh" } };
#endif
}

juce::String getFileLabelForError(const juce::String& relativePath)
{
    return relativePath.isNotEmpty() ? ("File " + relativePath) : "Project source";
}

juce::Result validateCompileRequest(const UserDspProjectSnapshot& projectSnapshot, ValidatedProjectLayout& layout)
{
    if (const auto validationResult = userdsp::validateProcessorClassName(projectSnapshot.processorClassName); validationResult.failed())
        return validationResult;

    if (const auto validationResult = userdsp::validateControllerDefinitions(projectSnapshot.controllers); validationResult.failed())
        return validationResult;

    if (projectSnapshot.files.empty())
        return juce::Result::fail("The DSP project does not contain any source files.");

    const auto unqualifiedProcessorClass = userdsp::getLastIdentifierSegment(projectSnapshot.processorClassName);
    juce::StringArray definitionMatches;

    for (const auto& fileSnapshot : projectSnapshot.files)
    {
        if (! userdsp::isSupportedProjectFilePath(fileSnapshot.relativePath))
            return juce::Result::fail("Unsupported project file type: " + fileSnapshot.relativePath);

        if (const auto validationResult = userdsp::validateSourceDoesNotContainManualSdk(fileSnapshot.content,
                                                                                          getFileLabelForError(fileSnapshot.relativePath));
            validationResult.failed())
        {
            return validationResult;
        }

        if (userdsp::sourceDefinesUnqualifiedType(fileSnapshot.content, unqualifiedProcessorClass))
            definitionMatches.add(fileSnapshot.relativePath);
    }

    if (definitionMatches.isEmpty())
    {
        return juce::Result::fail("Processor Class " + projectSnapshot.processorClassName
                                  + " was not found in the project source files.");
    }

    if (definitionMatches.size() > 1)
    {
        return juce::Result::fail("Processor Class " + projectSnapshot.processorClassName
                                  + " is defined in multiple files: " + definitionMatches.joinIntoString(", "));
    }

    layout.classDefinitionFilePath = definitionMatches[0];
    const auto definitionExtension = juce::File(layout.classDefinitionFilePath).getFileExtension().toLowerCase();
    layout.classDefinitionInHeader = definitionExtension == ".h" || definitionExtension == ".hpp";
    return juce::Result::ok();
}

juce::Result writeTextFile(const juce::File& file, const juce::String& text)
{
    if (auto parentDirectory = file.getParentDirectory(); ! parentDirectory.exists())
        parentDirectory.createDirectory();

    if (! file.replaceWithText(text))
        return juce::Result::fail("Failed to write " + file.getFullPathName());

    return juce::Result::ok();
}
} // namespace

UserDspCompiler::UserDspCompiler(UserDspHost& hostToUpdate)
    : host(hostToUpdate)
{
}

UserDspCompiler::~UserDspCompiler()
{
    if (worker.joinable())
        worker.join();
}

juce::Result UserDspCompiler::compileAsync(const UserDspProjectSnapshot& projectSnapshot)
{
    if (compileInProgress.exchange(true))
        return juce::Result::fail("A user DSP compilation is already running.");

    if (worker.joinable())
        worker.join();

    ValidatedProjectLayout validatedLayout;

    if (const auto validationResult = validateCompileRequest(projectSnapshot, validatedLayout); validationResult.failed())
    {
        updateState(State::failed, validationResult.getErrorMessage(), validationResult.getErrorMessage());
        compileInProgress.store(false);
        return validationResult;
    }

    updateState(State::compiling, "Compiling user DSP project...", {});
    worker = std::thread([this, projectSnapshot]
    {
        runCompilation(projectSnapshot);
    });

    return juce::Result::ok();
}

UserDspCompiler::Snapshot UserDspCompiler::getSnapshot() const
{
    const juce::ScopedLock lock(snapshotLock);
    return snapshot;
}

void UserDspCompiler::runCompilation(UserDspProjectSnapshot projectSnapshot)
{
    Snapshot result;
    result.state = State::failed;

    ValidatedProjectLayout validatedLayout;

    if (const auto validationResult = validateCompileRequest(projectSnapshot, validatedLayout); validationResult.failed())
    {
        updateState(State::failed, validationResult.getErrorMessage(), validationResult.getErrorMessage());
        compileInProgress.store(false);
        return;
    }

    const auto workspaceRoot = apppaths::getWorkspaceRoot();
    const auto stagingRoot = workspaceRoot.getChildFile("projects");
    const auto outputDirectory = workspaceRoot.getChildFile("bin");
    const auto buildInfo = getUserDspBuildInfo();

    stagingRoot.createDirectory();
    outputDirectory.createDirectory();

    const auto versionTag = createVersionTag();
    const auto stagedProjectDirectory = stagingRoot.getChildFile("user_dsp_" + versionTag);
    const auto outputModule = outputDirectory.getChildFile("user_dsp_" + versionTag + buildInfo.moduleExtension);
    const auto outputDebugSymbols = outputDirectory.getChildFile("user_dsp_" + versionTag + buildInfo.debugSymbolsExtension);

    result.lastSourcePath = stagedProjectDirectory.getFullPathName();
    result.lastModulePath = outputModule.getFullPathName();

    {
        const juce::ScopedLock lock(snapshotLock);
        snapshot.lastSourcePath = result.lastSourcePath;
        snapshot.lastModulePath = result.lastModulePath;
    }

    if (stagedProjectDirectory.exists())
        stagedProjectDirectory.deleteRecursively();

    stagedProjectDirectory.createDirectory();

    const auto generatedControlsHeaderRelativePath = juce::String("__generated__/dspedu_controls.h");
    const auto generatedControlsHeaderFile = stagedProjectDirectory.getChildFile(generatedControlsHeaderRelativePath);
    const auto generatedControlsHeaderText = userdsp::buildGeneratedControlsHeader(projectSnapshot.controllers);

    if (const auto writeResult = writeTextFile(generatedControlsHeaderFile, generatedControlsHeaderText); writeResult.failed())
    {
        updateState(State::failed, "Failed to stage generated controls.", writeResult.getErrorMessage());
        compileInProgress.store(false);
        return;
    }

    for (const auto& fileSnapshot : projectSnapshot.files)
    {
        auto stagedContent = fileSnapshot.content.trimEnd() + "\n";
        const auto fileExtension = juce::File(fileSnapshot.relativePath).getFileExtension().toLowerCase();

        if (fileExtension == ".cpp")
            stagedContent = userdsp::injectSdkAndControlsIncludesIntoSource(fileSnapshot.content, generatedControlsHeaderRelativePath);

        if (! validatedLayout.classDefinitionInHeader && fileSnapshot.relativePath == validatedLayout.classDefinitionFilePath)
            stagedContent = stagedContent.trimEnd() + "\n\n"
                         + userdsp::buildWrapperSourceSnippet(projectSnapshot.controllers, projectSnapshot.processorClassName)
                         + "\n";

        if (const auto writeResult = writeTextFile(stagedProjectDirectory.getChildFile(fileSnapshot.relativePath), stagedContent);
            writeResult.failed())
        {
            updateState(State::failed, "Failed to stage the DSP project.", writeResult.getErrorMessage());
            compileInProgress.store(false);
            return;
        }
    }

    if (validatedLayout.classDefinitionInHeader)
    {
        const auto wrapperFile = stagedProjectDirectory.getChildFile("__generated__/dspedu_entry.cpp");
        const auto wrapperSource = userdsp::buildWrapperTranslationUnit(validatedLayout.classDefinitionFilePath,
                                                                        projectSnapshot.controllers,
                                                                        projectSnapshot.processorClassName,
                                                                        generatedControlsHeaderRelativePath);

        if (const auto writeResult = writeTextFile(wrapperFile, wrapperSource); writeResult.failed())
        {
            updateState(State::failed, "Failed to stage the DSP project wrapper.", writeResult.getErrorMessage());
            compileInProgress.store(false);
            return;
        }
    }

    const auto buildScript = apppaths::findProjectResource(buildInfo.scriptRelativePath);
    const auto sdkHeader = apppaths::findProjectResource("user_dsp_sdk/UserDspApi.h");

    if (! buildScript.existsAsFile() || ! sdkHeader.existsAsFile())
    {
        updateState(State::failed, "Build script or SDK header was not found next to the project.", {});
        compileInProgress.store(false);
        return;
    }

    juce::ChildProcess childProcess;
    auto arguments = buildInfo.launchCommand;
    arguments.add(buildScript.getFullPathName());
    arguments.add(stagedProjectDirectory.getFullPathName());
    arguments.add(sdkHeader.getParentDirectory().getFullPathName());
    arguments.add(outputModule.getFullPathName());
    arguments.add(outputDebugSymbols.getFullPathName());

    if (! childProcess.start(arguments))
    {
        updateState(State::failed, "Failed to start the user DSP compiler process.", {});
        compileInProgress.store(false);
        return;
    }

    const auto buildLog = childProcess.readAllProcessOutput();
    childProcess.waitForProcessToFinish(-1);

    if (childProcess.getExitCode() != 0)
    {
        updateState(State::failed, "User DSP compilation failed.", buildLog);
        compileInProgress.store(false);
        return;
    }

    const auto loadResult = host.loadModuleFromFile(outputModule, projectSnapshot.controllers);

    if (loadResult.failed())
    {
        updateState(State::failed, "Compilation succeeded, but module load failed.", buildLog + "\n\n" + loadResult.getErrorMessage());
        compileInProgress.store(false);
        return;
    }

    updateState(State::succeeded, "Compilation succeeded. New DSP module loaded.", buildLog);
    compileInProgress.store(false);
}

void UserDspCompiler::setSnapshot(const Snapshot& newSnapshot)
{
    const juce::ScopedLock lock(snapshotLock);
    snapshot = newSnapshot;
}

void UserDspCompiler::updateState(State state, const juce::String& statusText, const juce::String& logText)
{
    auto nextSnapshot = getSnapshot();
    nextSnapshot.state = state;
    nextSnapshot.statusText = statusText;
    nextSnapshot.logText = logText;
    setSnapshot(nextSnapshot);
}

juce::String UserDspCompiler::createVersionTag()
{
    const auto timestamp = juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
    return timestamp + "_" + juce::String(compileCounter.fetch_add(1));
}
