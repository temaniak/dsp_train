#include "userdsp/UserDspHost.h"

namespace
{
using GetApiFunction = const DspEduApi* (*)() noexcept;

PreferredAudioConfiguration preferredConfigurationFromApi(const DspEduPreferredAudioConfig& config) noexcept
{
    PreferredAudioConfiguration preferred;
    preferred.valid = config.preferredSampleRate > 0.0
                   || config.preferredBlockSize > 0
                   || config.preferredInputChannels > 0
                   || config.preferredOutputChannels > 0;
    preferred.sampleRate = config.preferredSampleRate;
    preferred.blockSize = juce::jmax(0, config.preferredBlockSize);
    preferred.preferredInputChannels = juce::jlimit(0, DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS, config.preferredInputChannels);
    preferred.preferredOutputChannels = juce::jlimit(0, DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS, config.preferredOutputChannels);
    return preferred;
}

void copyInputToOutput(const float* const* inputs,
                       float* const* outputs,
                       int numInputChannels,
                       int numOutputChannels,
                       int numSamples) noexcept
{
    if (outputs == nullptr || numSamples <= 0)
        return;

    for (int channel = 0; channel < numOutputChannels; ++channel)
    {
        auto* output = outputs[channel];

        if (output == nullptr)
            continue;

        if (inputs == nullptr || numInputChannels <= 0)
        {
            juce::FloatVectorOperations::clear(output, numSamples);
            continue;
        }

        const auto sourceChannel = juce::jlimit(0, juce::jmax(0, numInputChannels - 1), channel);
        const auto* input = inputs[sourceChannel];

        if (input != nullptr)
            juce::FloatVectorOperations::copy(output, input, numSamples);
        else
            juce::FloatVectorOperations::clear(output, numSamples);
    }
}
}

struct UserDspHost::Runtime
{
    juce::DynamicLibrary library;
    const DspEduApi* api = nullptr;
    DspEduInstanceHandle instance = nullptr;
    juce::File moduleFile;
    juce::String processorName;
    PreferredAudioConfiguration preferredAudioConfiguration;
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

void UserDspHost::prepare(double sampleRate, int maxBlockSize, int inputChannels, int outputChannels)
{
    currentSampleRate.store(sampleRate > 0.0 ? sampleRate : 44100.0, std::memory_order_relaxed);
    currentMaxBlockSize.store(juce::jmax(1, maxBlockSize), std::memory_order_relaxed);
    currentInputChannels.store(juce::jlimit(0, DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS, inputChannels), std::memory_order_relaxed);
    currentOutputChannels.store(juce::jlimit(0, DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS, outputChannels), std::memory_order_relaxed);

    if (activeRuntime != nullptr && activeRuntime->api != nullptr && activeRuntime->api->prepare != nullptr)
    {
        DspEduProcessSpec spec;
        spec.sampleRate = currentSampleRate.load(std::memory_order_relaxed);
        spec.maximumBlockSize = currentMaxBlockSize.load(std::memory_order_relaxed);
        spec.activeInputChannels = currentInputChannels.load(std::memory_order_relaxed);
        spec.activeOutputChannels = currentOutputChannels.load(std::memory_order_relaxed);
        activeRuntime->api->prepare(activeRuntime->instance, &spec);
    }

    const juce::ScopedLock lock(snapshotLock);
    snapshot.preparedSampleRate = currentSampleRate.load(std::memory_order_relaxed);
    snapshot.preparedBlockSize = currentMaxBlockSize.load(std::memory_order_relaxed);
    snapshot.preparedInputChannels = currentInputChannels.load(std::memory_order_relaxed);
    snapshot.preparedOutputChannels = currentOutputChannels.load(std::memory_order_relaxed);
}

void UserDspHost::requestReset() noexcept
{
    resetRequested.store(true, std::memory_order_relaxed);
}

void UserDspHost::process(const float* const* inputs,
                          float* const* outputs,
                          int numInputChannels,
                          int numOutputChannels,
                          int numSamples)
{
    commitPendingRuntimeSwap();

    if (activeRuntime == nullptr || activeRuntime->api == nullptr)
    {
        copyInputToOutput(inputs, outputs, numInputChannels, numOutputChannels, numSamples);
        return;
    }

    if (resetRequested.exchange(false, std::memory_order_relaxed) && activeRuntime->api->reset != nullptr)
        activeRuntime->api->reset(activeRuntime->instance);

    for (int index = 0; index < activeRuntime->controlCount; ++index)
        activeRuntime->api->setControlValue(activeRuntime->instance, index, controlValues[static_cast<std::size_t>(index)].load(std::memory_order_relaxed));

    activeRuntime->api->process(activeRuntime->instance, inputs, outputs, numInputChannels, numOutputChannels, numSamples);
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

    DspEduPreferredAudioConfig preferredConfig;

    if (runtime->api->getPreferredAudioConfig != nullptr && runtime->api->getPreferredAudioConfig(runtime->instance, &preferredConfig) == 1)
        runtime->preferredAudioConfiguration = preferredConfigurationFromApi(preferredConfig);

    for (int index = 0; index < DSP_EDU_USER_DSP_MAX_CONTROLS; ++index)
        controlValues[static_cast<std::size_t>(index)].store(0.0f, std::memory_order_relaxed);

    DspEduProcessSpec spec;
    spec.sampleRate = currentSampleRate.load(std::memory_order_relaxed);
    spec.maximumBlockSize = currentMaxBlockSize.load(std::memory_order_relaxed);
    spec.activeInputChannels = currentInputChannels.load(std::memory_order_relaxed);
    spec.activeOutputChannels = currentOutputChannels.load(std::memory_order_relaxed);
    runtime->api->prepare(runtime->instance, &spec);
    runtime->api->reset(runtime->instance);

    {
        const juce::ScopedLock lock(snapshotLock);
        snapshot.hasActiveModule = true;
        snapshot.processorName = runtime->processorName;
        snapshot.activeModulePath = moduleFile.getFullPathName();
        snapshot.lastError.clear();
        ++snapshot.moduleGeneration;
        snapshot.preferredAudioConfiguration = runtime->preferredAudioConfiguration;
        snapshot.preparedSampleRate = spec.sampleRate;
        snapshot.preparedBlockSize = spec.maximumBlockSize;
        snapshot.preparedInputChannels = spec.activeInputChannels;
        snapshot.preparedOutputChannels = spec.activeOutputChannels;
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
