#include "userdsp/UserDspCompiler.h"

#include "util/AppPaths.h"

UserDspCompiler::UserDspCompiler(UserDspHost& hostToUpdate)
    : host(hostToUpdate)
{
}

UserDspCompiler::~UserDspCompiler()
{
    if (worker.joinable())
        worker.join();
}

bool UserDspCompiler::compileAsync(const juce::String& sourceCode)
{
    if (compileInProgress.exchange(true))
        return false;

    if (worker.joinable())
        worker.join();

    updateState(State::compiling, "Compiling user DSP...", {});
    worker = std::thread([this, sourceCode]
    {
        runCompilation(sourceCode);
    });

    return true;
}

UserDspCompiler::Snapshot UserDspCompiler::getSnapshot() const
{
    const juce::ScopedLock lock(snapshotLock);
    return snapshot;
}

void UserDspCompiler::runCompilation(juce::String sourceCode)
{
    Snapshot result;
    result.state = State::failed;

    const auto workspaceRoot = apppaths::getWorkspaceRoot();
    const auto sourceDirectory = workspaceRoot.getChildFile("source");
    const auto outputDirectory = workspaceRoot.getChildFile("bin");

    sourceDirectory.createDirectory();
    outputDirectory.createDirectory();

    const auto versionTag = createVersionTag();
    const auto sourceFile = sourceDirectory.getChildFile("user_dsp_" + versionTag + ".cpp");
    const auto outputDll = outputDirectory.getChildFile("user_dsp_" + versionTag + ".dll");
    const auto outputPdb = outputDirectory.getChildFile("user_dsp_" + versionTag + ".pdb");

    result.lastSourcePath = sourceFile.getFullPathName();
    result.lastDllPath = outputDll.getFullPathName();

    {
        const juce::ScopedLock lock(snapshotLock);
        snapshot.lastSourcePath = result.lastSourcePath;
        snapshot.lastDllPath = result.lastDllPath;
    }

    if (! sourceFile.replaceWithText(sourceCode))
    {
        updateState(State::failed, "Failed to write user DSP source file.", {});
        compileInProgress.store(false);
        return;
    }

    const auto buildScript = apppaths::findProjectResource("scripts/build_user_dsp.cmd");
    const auto sdkHeader = apppaths::findProjectResource("user_dsp_sdk/UserDspApi.h");

    if (! buildScript.existsAsFile() || ! sdkHeader.existsAsFile())
    {
        updateState(State::failed, "Build script or SDK header was not found next to the project.", {});
        compileInProgress.store(false);
        return;
    }

    juce::ChildProcess childProcess;
    juce::StringArray arguments;
    arguments.add("cmd");
    arguments.add("/c");
    arguments.add(buildScript.getFullPathName());
    arguments.add(sourceFile.getFullPathName());
    arguments.add(sdkHeader.getParentDirectory().getFullPathName());
    arguments.add(outputDll.getFullPathName());
    arguments.add(outputPdb.getFullPathName());

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

    const auto loadResult = host.loadModuleFromFile(outputDll);

    if (loadResult.failed())
    {
        updateState(State::failed, "Compilation succeeded, but DLL load failed.", buildLog + "\n\n" + loadResult.getErrorMessage());
        compileInProgress.store(false);
        return;
    }

    updateState(State::succeeded, "Compilation succeeded. New DSP DLL loaded.", buildLog);
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
