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
    int controlCount = 0;

    ~Runtime()
    {
        if (api != nullptr && api->destroy != nullptr && instance != nullptr)
            api->destroy(instance);

        library.close();
    }
};

UserDspHost::UserDspHost()
{
    for (auto& controlValue : controlValues)
        controlValue.store(0.0f, std::memory_order_relaxed);
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

    for (int index = 0; index < activeRuntime->controlCount; ++index)
        activeRuntime->api->setControlValue(activeRuntime->instance, index, controlValues[static_cast<std::size_t>(index)].load(std::memory_order_relaxed));

    activeRuntime->api->process(activeRuntime->instance, input, output, numSamples);
}

void UserDspHost::setControlValue(int index, float value) noexcept
{
    if (! juce::isPositiveAndBelow(index, DSP_EDU_USER_DSP_MAX_CONTROLS))
        return;

    const auto clampedValue = juce::jlimit(0.0f, 1.0f, value);
    controlValues[static_cast<std::size_t>(index)].store(clampedValue, std::memory_order_relaxed);

    const juce::ScopedLock lock(snapshotLock);

    if (index < snapshot.controlCount)
        snapshot.controls[static_cast<std::size_t>(index)].currentValue = clampedValue;
}

float UserDspHost::getControlValue(int index) const noexcept
{
    if (! juce::isPositiveAndBelow(index, DSP_EDU_USER_DSP_MAX_CONTROLS))
        return 0.0f;

    return controlValues[static_cast<std::size_t>(index)].load(std::memory_order_relaxed);
}

juce::Result UserDspHost::loadModuleFromFile(const juce::File& moduleFile,
                                             const std::vector<UserDspControllerDefinition>& controllerDefinitions)
{
    reclaimRetiredRuntimes();

    if (controllerDefinitions.size() > static_cast<std::size_t>(DSP_EDU_USER_DSP_MAX_CONTROLS))
    {
        return juce::Result::fail("A DSP project can define at most "
                                  + juce::String(DSP_EDU_USER_DSP_MAX_CONTROLS)
                                  + " controllers.");
    }

    auto runtime = std::make_unique<Runtime>();
    runtime->moduleFile = moduleFile;

    if (! runtime->library.open(moduleFile.getFullPathName()))
        return juce::Result::fail("Failed to load module: " + moduleFile.getFullPathName());

    const auto getApi = reinterpret_cast<GetApiFunction>(runtime->library.getFunction("dspedu_get_api"));

    if (getApi == nullptr)
        return juce::Result::fail("The module does not export dspedu_get_api.");

    runtime->api = getApi();

    if (runtime->api == nullptr)
        return juce::Result::fail("The module returned a null API pointer.");

    if (runtime->api->apiVersion != DSP_EDU_USER_DSP_API_VERSION || runtime->api->structSize < sizeof(DspEduApi))
        return juce::Result::fail("The module uses an incompatible user DSP API version.");

    if (runtime->api->create == nullptr
        || runtime->api->destroy == nullptr
        || runtime->api->getProcessorName == nullptr
        || runtime->api->getParameterCount == nullptr
        || runtime->api->getParameterInfo == nullptr
        || runtime->api->prepare == nullptr
        || runtime->api->reset == nullptr
        || runtime->api->process == nullptr
        || runtime->api->setControlValue == nullptr)
    {
        return juce::Result::fail("The module exports an incomplete DSP API.");
    }

    runtime->instance = runtime->api->create();

    if (runtime->instance == nullptr)
        return juce::Result::fail("The module could not create a DSP instance.");

    runtime->processorName = runtime->api->getProcessorName(runtime->instance);
    runtime->controlCount = static_cast<int>(controllerDefinitions.size());

    for (int index = 0; index < DSP_EDU_USER_DSP_MAX_CONTROLS; ++index)
        controlValues[static_cast<std::size_t>(index)].store(0.0f, std::memory_order_relaxed);

    runtime->api->prepare(runtime->instance, currentSampleRate.load(std::memory_order_relaxed), currentMaxBlockSize.load(std::memory_order_relaxed));
    runtime->api->reset(runtime->instance);

    {
        const juce::ScopedLock lock(snapshotLock);
        snapshot.hasActiveModule = true;
        snapshot.processorName = runtime->processorName;
        snapshot.activeModulePath = moduleFile.getFullPathName();
        snapshot.lastError.clear();
        snapshot.controlCount = runtime->controlCount;

        for (int index = 0; index < DSP_EDU_USER_DSP_MAX_CONTROLS; ++index)
        {
            auto& controlState = snapshot.controls[static_cast<std::size_t>(index)];
            controlState = {};

            if (index < runtime->controlCount)
            {
                const auto& definition = controllerDefinitions[static_cast<std::size_t>(index)];
                controlState.active = true;
                controlState.type = definition.type;
                controlState.label = definition.label;
                controlState.codeName = definition.codeName;
                controlState.currentValue = 0.0f;
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
