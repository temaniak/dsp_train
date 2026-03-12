#include "userdsp/UserDspHost.h"

namespace
{
using GetApiFunction = const DspEduApi* (*)() noexcept;
}

struct UserDspHost::Runtime
{
    juce::DynamicLibrary library;
    const DspEduApi* api = nullptr;
    DspEduInstanceHandle instance = nullptr;
    juce::File moduleFile;
    juce::String processorName;
    std::array<DspEduParameterInfo, DSP_EDU_USER_DSP_MAX_PARAMETERS> parameterInfo {};
    int parameterCount = 0;

    ~Runtime()
    {
        if (api != nullptr && api->destroy != nullptr && instance != nullptr)
            api->destroy(instance);

        library.close();
    }
};

UserDspHost::UserDspHost()
{
    for (auto& parameterValue : parameterValues)
        parameterValue.store(0.0f, std::memory_order_relaxed);
}

UserDspHost::~UserDspHost()
{
    reclaimRetiredRuntimes();
    pendingRuntime.reset();
    activeRuntime.reset();
}

void UserDspHost::prepare(double sampleRate, int maxBlockSize)
{
    currentSampleRate.store(sampleRate > 0.0 ? sampleRate : 44100.0, std::memory_order_relaxed);
    currentMaxBlockSize.store(juce::jmax(1, maxBlockSize), std::memory_order_relaxed);

    if (activeRuntime != nullptr && activeRuntime->api != nullptr && activeRuntime->api->prepare != nullptr)
        activeRuntime->api->prepare(activeRuntime->instance, currentSampleRate.load(std::memory_order_relaxed), currentMaxBlockSize.load(std::memory_order_relaxed));
}

void UserDspHost::requestReset() noexcept
{
    resetRequested.store(true, std::memory_order_relaxed);
}

void UserDspHost::process(const float* input, float* output, int numSamples)
{
    commitPendingRuntimeSwap();

    if (activeRuntime == nullptr || activeRuntime->api == nullptr)
    {
        juce::FloatVectorOperations::copy(output, input, numSamples);
        return;
    }

    if (resetRequested.exchange(false, std::memory_order_relaxed) && activeRuntime->api->reset != nullptr)
        activeRuntime->api->reset(activeRuntime->instance);

    for (int index = 0; index < activeRuntime->parameterCount; ++index)
        activeRuntime->api->setParameter(activeRuntime->instance, index, parameterValues[static_cast<std::size_t>(index)].load(std::memory_order_relaxed));

    activeRuntime->api->process(activeRuntime->instance, input, output, numSamples);
}

void UserDspHost::setParameterValue(int index, float value) noexcept
{
    if (! juce::isPositiveAndBelow(index, DSP_EDU_USER_DSP_MAX_PARAMETERS))
        return;

    parameterValues[static_cast<std::size_t>(index)].store(value, std::memory_order_relaxed);

    const juce::ScopedLock lock(snapshotLock);

    if (index < snapshot.parameterCount)
        snapshot.parameters[static_cast<std::size_t>(index)].currentValue = value;
}

float UserDspHost::getParameterValue(int index) const noexcept
{
    if (! juce::isPositiveAndBelow(index, DSP_EDU_USER_DSP_MAX_PARAMETERS))
        return 0.0f;

    return parameterValues[static_cast<std::size_t>(index)].load(std::memory_order_relaxed);
}

juce::Result UserDspHost::loadModuleFromFile(const juce::File& moduleFile)
{
    reclaimRetiredRuntimes();

    auto runtime = std::make_unique<Runtime>();
    runtime->moduleFile = moduleFile;

    if (! runtime->library.open(moduleFile.getFullPathName()))
        return juce::Result::fail("Failed to load DLL: " + moduleFile.getFullPathName());

    const auto getApi = reinterpret_cast<GetApiFunction>(runtime->library.getFunction("dspedu_get_api"));

    if (getApi == nullptr)
        return juce::Result::fail("The DLL does not export dspedu_get_api.");

    runtime->api = getApi();

    if (runtime->api == nullptr)
        return juce::Result::fail("The DLL returned a null API pointer.");

    if (runtime->api->apiVersion != DSP_EDU_USER_DSP_API_VERSION || runtime->api->structSize < sizeof(DspEduApi))
        return juce::Result::fail("The DLL uses an incompatible user DSP API version.");

    if (runtime->api->create == nullptr
        || runtime->api->destroy == nullptr
        || runtime->api->getProcessorName == nullptr
        || runtime->api->getParameterCount == nullptr
        || runtime->api->getParameterInfo == nullptr
        || runtime->api->prepare == nullptr
        || runtime->api->reset == nullptr
        || runtime->api->process == nullptr
        || runtime->api->setParameter == nullptr)
    {
        return juce::Result::fail("The DLL exports an incomplete DSP API.");
    }

    runtime->instance = runtime->api->create();

    if (runtime->instance == nullptr)
        return juce::Result::fail("The DLL could not create a DSP instance.");

    runtime->processorName = runtime->api->getProcessorName(runtime->instance);
    runtime->parameterCount = juce::jlimit(0, DSP_EDU_USER_DSP_MAX_PARAMETERS, runtime->api->getParameterCount(runtime->instance));

    for (int index = 0; index < runtime->parameterCount; ++index)
    {
        DspEduParameterInfo info {};

        if (! runtime->api->getParameterInfo(runtime->instance, index, &info))
            return juce::Result::fail("Failed to query parameter metadata from the DLL.");

        runtime->parameterInfo[static_cast<std::size_t>(index)] = info;
        parameterValues[static_cast<std::size_t>(index)].store(info.defaultValue, std::memory_order_relaxed);
    }

    runtime->api->prepare(runtime->instance, currentSampleRate.load(std::memory_order_relaxed), currentMaxBlockSize.load(std::memory_order_relaxed));
    runtime->api->reset(runtime->instance);

    {
        const juce::ScopedLock lock(snapshotLock);
        snapshot.hasActiveModule = true;
        snapshot.processorName = runtime->processorName;
        snapshot.activeModulePath = moduleFile.getFullPathName();
        snapshot.lastError.clear();
        snapshot.parameterCount = runtime->parameterCount;

        for (int index = 0; index < DSP_EDU_USER_DSP_MAX_PARAMETERS; ++index)
        {
            auto& parameterState = snapshot.parameters[static_cast<std::size_t>(index)];
            parameterState = {};

            if (index < runtime->parameterCount)
            {
                const auto& info = runtime->parameterInfo[static_cast<std::size_t>(index)];
                parameterState.active = true;
                parameterState.id = info.id;
                parameterState.name = info.name;
                parameterState.minValue = info.minValue;
                parameterState.maxValue = info.maxValue;
                parameterState.defaultValue = info.defaultValue;
                parameterState.currentValue = info.defaultValue;
            }
        }
    }

    {
        const juce::SpinLock::ScopedLockType lock(pendingRuntimeLock);
        pendingRuntime = std::move(runtime);
        swapPending.store(true, std::memory_order_release);
    }

    resetRequested.store(true, std::memory_order_relaxed);
    return juce::Result::ok();
}

void UserDspHost::reclaimRetiredRuntimes()
{
    while (retiredRuntimeFifo.getNumReady() > 0)
    {
        auto scope = retiredRuntimeFifo.read(1);

        if (scope.blockSize1 > 0)
        {
            std::unique_ptr<Runtime> runtime(retiredRuntimeSlots[static_cast<std::size_t>(scope.startIndex1)]);
            retiredRuntimeSlots[static_cast<std::size_t>(scope.startIndex1)] = nullptr;
        }
    }
}

UserDspHost::Snapshot UserDspHost::getSnapshot() const
{
    const juce::ScopedLock lock(snapshotLock);
    return snapshot;
}

void UserDspHost::commitPendingRuntimeSwap() noexcept
{
    if (! swapPending.load(std::memory_order_acquire))
        return;

    const juce::SpinLock::ScopedTryLockType lock(pendingRuntimeLock);

    if (! lock.isLocked() || pendingRuntime == nullptr)
        return;

    auto* displacedRuntime = activeRuntime.release();
    activeRuntime = std::move(pendingRuntime);
    swapPending.store(false, std::memory_order_release);

    if (displacedRuntime != nullptr)
        enqueueRetiredRuntime(displacedRuntime);
}

void UserDspHost::enqueueRetiredRuntime(Runtime* runtime) noexcept
{
    if (runtime == nullptr)
        return;

    auto scope = retiredRuntimeFifo.write(1);

    if (scope.blockSize1 > 0)
    {
        retiredRuntimeSlots[static_cast<std::size_t>(scope.startIndex1)] = runtime;
        return;
    }

    delete runtime;
}

void UserDspHost::clearSnapshotError()
{
    const juce::ScopedLock lock(snapshotLock);
    snapshot.lastError.clear();
}

void UserDspHost::setSnapshotError(const juce::String& errorText)
{
    const juce::ScopedLock lock(snapshotLock);
    snapshot.lastError = errorText;
}
