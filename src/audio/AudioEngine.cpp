#include "audio/AudioEngine.h"

#include <set>

namespace
{
double chooseNearestSampleRate(const juce::Array<double>& availableRates, double requestedRate)
{
    if (availableRates.isEmpty())
        return requestedRate > 0.0 ? requestedRate : 44100.0;

    auto nearest = availableRates[0];
    auto nearestDistance = std::abs(nearest - requestedRate);

    for (const auto rate : availableRates)
    {
        const auto distance = std::abs(rate - requestedRate);

        if (distance < nearestDistance)
        {
            nearest = rate;
            nearestDistance = distance;
        }
    }

    return nearest;
}

int chooseNearestBlockSize(const juce::Array<int>& availableSizes, int requestedSize)
{
    if (availableSizes.isEmpty())
        return requestedSize > 0 ? requestedSize : 512;

    auto nearest = availableSizes[0];
    auto nearestDistance = std::abs(nearest - requestedSize);

    for (const auto size : availableSizes)
    {
        const auto distance = std::abs(size - requestedSize);

        if (distance < nearestDistance)
        {
            nearest = size;
            nearestDistance = distance;
        }
    }

    return nearest;
}

std::vector<int> sanitiseChannelIndices(const std::vector<int>& indices, int maxChannels, int minimumCount)
{
    std::set<int> unique;

    for (const auto index : indices)
        if (juce::isPositiveAndBelow(index, maxChannels))
            unique.insert(index);

    std::vector<int> result(unique.begin(), unique.end());

    for (int index = 0; result.size() < static_cast<std::size_t>(minimumCount) && index < maxChannels; ++index)
    {
        if (! unique.contains(index))
            result.push_back(index);
    }

    return result;
}

juce::String formatRate(double sampleRate)
{
    return sampleRate > 0.0 ? juce::String(sampleRate, 1) + " Hz" : "--";
}

juce::String formatBlockSize(int blockSize)
{
    return blockSize > 0 ? juce::String(blockSize) : "--";
}

juce::String formatChannelPair(int inputChannels, int outputChannels)
{
    return juce::String(inputChannels) + " in / " + juce::String(outputChannels) + " out";
}
}

AudioEngine::AudioEngine()
    : oscilloscopeBuffer(32768)
{
    audioFormatManager.registerBasicFormats();

    for (auto& slot : currentResolvedInputSlots)
        slot.store(-1, std::memory_order_relaxed);

    for (auto& slot : currentResolvedOutputSlots)
        slot.store(-1, std::memory_order_relaxed);
}

AudioEngine::~AudioEngine()
{
    deviceManager.removeAudioCallback(this);
    deviceManager.closeAudioDevice();
}

juce::Result AudioEngine::initialise()
{
    const auto error = deviceManager.initialise(2, 2, nullptr, true);

    if (error.isNotEmpty())
        return juce::Result::fail(error);

    deviceManager.addAudioCallback(this);

    juce::AudioDeviceManager::AudioDeviceSetup initialSetup;
    deviceManager.getAudioDeviceSetup(initialSetup);

    currentProjectAudioState.deviceSelection.inputDeviceName = initialSetup.inputDeviceName;
    currentProjectAudioState.deviceSelection.outputDeviceName = initialSetup.outputDeviceName;
    currentProjectAudioState.cachedPreferred.preferredInputChannels = 1;
    currentProjectAudioState.cachedPreferred.preferredOutputChannels = 2;

    return applyProjectAudioState(currentProjectAudioState);
}

AudioEngine::Snapshot AudioEngine::getSnapshot() const
{
    Snapshot snapshot;
    snapshot.transportRunning = transportRunning.load(std::memory_order_relaxed);
    snapshot.sampleRate = currentSampleRate.load(std::memory_order_relaxed);
    snapshot.blockSize = currentBlockSize.load(std::memory_order_relaxed);
    snapshot.sourceType = sourceType.load(std::memory_order_relaxed);
    snapshot.sourceGain = sourceGain.load(std::memory_order_relaxed);
    snapshot.sineFrequency = sineSource.getFrequency();
    snapshot.wavLoaded = wavFileSource.hasFileLoaded();
    snapshot.wavLooping = wavFileSource.isLooping();
    snapshot.wavPosition = wavFileSource.getPositionNormalized();
    snapshot.wavFileName = wavFileSource.getLoadedFileName();

    const juce::ScopedLock lock(stateLock);
    snapshot.deviceError = lastDeviceError;
    snapshot.preferredAudioConfiguration = currentProjectAudioState.cachedPreferred;
    snapshot.overrides = currentProjectAudioState.overrides;
    snapshot.lastKnownActual = currentProjectAudioState.lastKnownActual;
    snapshot.requestedSampleRate = requestedSampleRate;
    snapshot.requestedBlockSize = requestedBlockSize;
    snapshot.requestedInputChannels = requestedInputChannels;
    snapshot.requestedOutputChannels = requestedOutputChannels;
    snapshot.inputDeviceName = currentInputDeviceName;
    snapshot.outputDeviceName = currentOutputDeviceName;
    snapshot.availableInputDevices = availableInputDevices;
    snapshot.availableOutputDevices = availableOutputDevices;
    snapshot.availableSampleRates = availableSampleRates;
    snapshot.availableBlockSizes = availableBlockSizes;
    snapshot.inputChannelNames = inputChannelNames;
    snapshot.outputChannelNames = outputChannelNames;
    snapshot.enabledInputChannels = enabledInputChannels;
    snapshot.enabledOutputChannels = enabledOutputChannels;
    snapshot.inputRouting = currentProjectAudioState.deviceSelection.inputRouting;
    snapshot.outputRouting = currentProjectAudioState.deviceSelection.outputRouting;
    snapshot.statusMessages = currentStatusMessages;
    updateStatusTexts(snapshot);
    return snapshot;
}

ProjectAudioState AudioEngine::getProjectAudioState() const
{
    const juce::ScopedLock lock(stateLock);
    return currentProjectAudioState;
}

juce::Result AudioEngine::applyProjectAudioState(const ProjectAudioState& state)
{
    auto resolvedState = buildResolvedDeviceState(state, true);

    if (resolvedState.outputDeviceName.isEmpty() && resolvedState.inputDeviceName.isEmpty())
        return juce::Result::fail("No audio devices are available.");

    publishResolvedDeviceState(resolvedState);
    requestResetForNextBlock();
    return juce::Result::ok();
}

void AudioEngine::startTransport() noexcept
{
    transportRunning.store(true, std::memory_order_relaxed);
    requestResetForNextBlock();
}

void AudioEngine::stopTransport() noexcept
{
    transportRunning.store(false, std::memory_order_relaxed);
}

void AudioEngine::setSourceType(SourceType type) noexcept
{
    sourceType.store(type, std::memory_order_relaxed);
    requestResetForNextBlock();
}

void AudioEngine::setSourceGain(float newGain) noexcept
{
    sourceGain.store(juce::jlimit(0.0f, 1.0f, newGain), std::memory_order_relaxed);
}

void AudioEngine::setSineFrequency(float frequency) noexcept
{
    sineSource.setFrequency(juce::jlimit(20.0f, 20000.0f, frequency));
}

juce::Result AudioEngine::loadWavFile(const juce::File& file)
{
    const auto result = wavFileSource.loadFromFile(file, audioFormatManager);

    if (result.wasOk())
        requestResetForNextBlock();

    return result;
}

void AudioEngine::setWavLooping(bool shouldLoop) noexcept
{
    wavFileSource.setLooping(shouldLoop);
}

void AudioEngine::setWavPositionNormalized(double newPosition) noexcept
{
    wavFileSource.setPositionNormalized(newPosition);
}

void AudioEngine::setUserControlValue(int index, float value) noexcept
{
    userDspHost.setControlValue(index, value);
}

OscilloscopeBuffer& AudioEngine::getOscilloscopeBuffer() noexcept
{
    return oscilloscopeBuffer;
}

UserDspHost& AudioEngine::getUserDspHost() noexcept
{
    return userDspHost;
}

void AudioEngine::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                   int numInputChannels,
                                                   float* const* outputChannelData,
                                                   int numOutputChannels,
                                                   int numSamples,
                                                   const juce::AudioIODeviceCallbackContext&)
{
    for (int channel = 0; channel < numOutputChannels; ++channel)
        if (outputChannelData[channel] != nullptr)
            juce::FloatVectorOperations::clear(outputChannelData[channel], numSamples);

    if (numSamples <= 0 || numOutputChannels <= 0)
        return;

    if (numSamples > sourceBuffer.getNumSamples() || numSamples > processedBuffer.getNumSamples())
        return;

    if (resetForNextBlock.exchange(false, std::memory_order_relaxed))
    {
        resetSourcesForCurrentSelection();
        userDspHost.requestReset();
    }

    if (! transportRunning.load(std::memory_order_relaxed))
        return;

    const auto logicalInputChannels = currentLogicalInputChannels.load(std::memory_order_relaxed);
    const auto logicalOutputChannels = currentLogicalOutputChannels.load(std::memory_order_relaxed);

    sourceBuffer.clear();
    processedBuffer.clear();

    if (sourceType.load(std::memory_order_relaxed) == SourceType::hardwareInput)
        routeHardwareInput(inputChannelData, numInputChannels, logicalInputChannels, numSamples);
    else
        generateInternalSource(logicalInputChannels, numSamples);

    for (int channel = 0; channel < logicalInputChannels; ++channel)
        juce::FloatVectorOperations::multiply(sourceBuffer.getWritePointer(channel),
                                              sourceGain.load(std::memory_order_relaxed),
                                              numSamples);

    std::array<const float*, DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS> inputPointers {};
    std::array<float*, DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS> outputPointers {};

    for (int channel = 0; channel < logicalInputChannels; ++channel)
        inputPointers[static_cast<std::size_t>(channel)] = sourceBuffer.getReadPointer(channel);

    for (int channel = 0; channel < logicalOutputChannels; ++channel)
        outputPointers[static_cast<std::size_t>(channel)] = processedBuffer.getWritePointer(channel);

    userDspHost.process(inputPointers.data(), outputPointers.data(), logicalInputChannels, logicalOutputChannels, numSamples);

    oscilloscopeBuffer.pushSamples(reinterpret_cast<const float* const*>(outputPointers.data()), logicalOutputChannels, numSamples);
    routeProcessedToOutputs(outputChannelData, numOutputChannels, logicalOutputChannels, numSamples);
}

void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    const auto sampleRate = device != nullptr ? device->getCurrentSampleRate() : 44100.0;
    const auto blockSize = device != nullptr ? device->getCurrentBufferSizeSamples() : 512;

    currentSampleRate.store(sampleRate, std::memory_order_relaxed);
    currentBlockSize.store(blockSize, std::memory_order_relaxed);

    sourceBuffer.setSize(DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS, juce::jmax(1, blockSize), false, false, true);
    processedBuffer.setSize(DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS, juce::jmax(1, blockSize), false, false, true);
    oscilloscopeBuffer.clear();

    sineSource.prepare(sampleRate, blockSize);
    whiteNoiseSource.prepare(sampleRate, blockSize);
    impulseSource.prepare(sampleRate, blockSize);
    wavFileSource.prepare(sampleRate, blockSize);
    userDspHost.prepare(sampleRate,
                        blockSize,
                        currentLogicalInputChannels.load(std::memory_order_relaxed),
                        currentLogicalOutputChannels.load(std::memory_order_relaxed));

    requestResetForNextBlock();
}

void AudioEngine::audioDeviceStopped()
{
    currentSampleRate.store(0.0, std::memory_order_relaxed);
    currentBlockSize.store(0, std::memory_order_relaxed);
}

void AudioEngine::audioDeviceError(const juce::String& errorMessage)
{
    const juce::ScopedLock lock(stateLock);
    lastDeviceError = errorMessage;
}

void AudioEngine::requestResetForNextBlock() noexcept
{
    resetForNextBlock.store(true, std::memory_order_relaxed);
}

void AudioEngine::resetSourcesForCurrentSelection() noexcept
{
    switch (sourceType.load(std::memory_order_relaxed))
    {
        case SourceType::sine:       sineSource.reset(); break;
        case SourceType::whiteNoise: whiteNoiseSource.reset(); break;
        case SourceType::impulse:    impulseSource.reset(); break;
        case SourceType::wavFile:    wavFileSource.reset(); break;
        case SourceType::hardwareInput: break;
    }
}

AudioEngine::ResolvedDeviceState AudioEngine::buildResolvedDeviceState(const ProjectAudioState& state, bool applySetup)
{
    ResolvedDeviceState resolvedState;
    resolvedState.projectAudioState = state;

    if (auto* deviceType = deviceManager.getCurrentDeviceTypeObject(); deviceType != nullptr)
    {
        deviceType->scanForDevices();
        resolvedState.availableInputDevices = deviceType->getDeviceNames(true);
        resolvedState.availableOutputDevices = deviceType->getDeviceNames(false);
    }

    juce::AudioDeviceManager::AudioDeviceSetup currentSetup;
    deviceManager.getAudioDeviceSetup(currentSetup);

    auto chooseDeviceName = [&resolvedState] (const juce::String& requestedName,
                                              const juce::StringArray& availableNames,
                                              const juce::String& fallbackName,
                                              bool isInput)
    {
        if (requestedName.isNotEmpty() && availableNames.contains(requestedName))
            return requestedName;

        if (requestedName.isNotEmpty() && ! availableNames.contains(requestedName))
        {
            resolvedState.statusMessages.push_back({
                AudioStatusCode::projectReopenedWithDifferentHardware,
                AudioStatusSeverity::warning,
                isInput ? "Saved input device is unavailable." : "Saved output device is unavailable.",
                requestedName + " is not available on the current hardware. Using a fallback device instead."
            });
        }

        if (fallbackName.isNotEmpty() && availableNames.contains(fallbackName))
            return fallbackName;

        return availableNames.isEmpty() ? juce::String() : availableNames[0];
    };

    resolvedState.inputDeviceName = chooseDeviceName(state.deviceSelection.inputDeviceName,
                                                     resolvedState.availableInputDevices,
                                                     currentSetup.inputDeviceName,
                                                     true);
    resolvedState.outputDeviceName = chooseDeviceName(state.deviceSelection.outputDeviceName,
                                                      resolvedState.availableOutputDevices,
                                                      currentSetup.outputDeviceName,
                                                      false);

    if (auto probeDevice = createProbeDevice(resolvedState.inputDeviceName, resolvedState.outputDeviceName))
    {
        resolvedState.availableSampleRates = probeDevice->getAvailableSampleRates();
        resolvedState.availableBlockSizes = probeDevice->getAvailableBufferSizes();
        resolvedState.inputChannelNames = probeDevice->getInputChannelNames();
        resolvedState.outputChannelNames = probeDevice->getOutputChannelNames();
    }

    const auto preferred = state.cachedPreferred;
    resolvedState.requestedSampleRate = state.overrides.sampleRateOverridden && state.overrides.sampleRate > 0.0
                                      ? state.overrides.sampleRate
                                      : (preferred.valid && preferred.sampleRate > 0.0 ? preferred.sampleRate
                                                                                        : (currentSetup.sampleRate > 0.0 ? currentSetup.sampleRate : 44100.0));
    resolvedState.requestedBlockSize = state.overrides.blockSizeOverridden && state.overrides.blockSize > 0
                                     ? state.overrides.blockSize
                                     : (preferred.valid && preferred.blockSize > 0 ? preferred.blockSize
                                                                                   : (currentSetup.bufferSize > 0 ? currentSetup.bufferSize : 512));
    resolvedState.requestedInputChannels = preferred.valid ? juce::jlimit(0, DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS, preferred.preferredInputChannels) : 1;
    resolvedState.requestedOutputChannels = preferred.valid ? juce::jlimit(1, DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS,
                                                                           juce::jmax(1, preferred.preferredOutputChannels))
                                                            : 2;

    if (preferred.valid)
    {
        resolvedState.statusMessages.push_back({
            AudioStatusCode::preferredValueProvided,
            AudioStatusSeverity::info,
            "Preferred audio configuration provided by DSP.",
            formatRate(preferred.sampleRate) + ", block " + formatBlockSize(preferred.blockSize)
            + ", " + formatChannelPair(preferred.preferredInputChannels, preferred.preferredOutputChannels)
        });
    }

    if (state.overrides.sampleRateOverridden || state.overrides.blockSizeOverridden
        || state.overrides.inputDeviceOverridden || state.overrides.outputDeviceOverridden
        || state.overrides.inputChannelsOverridden || state.overrides.outputChannelsOverridden
        || state.overrides.routingOverridden)
    {
        resolvedState.statusMessages.push_back({
            AudioStatusCode::valueManuallyOverridden,
            AudioStatusSeverity::info,
            "Manual audio overrides are active.",
            "Current project settings differ from the DSP module preferred configuration."
        });
    }

    const auto chosenRate = chooseNearestSampleRate(resolvedState.availableSampleRates, resolvedState.requestedSampleRate);
    const auto chosenBlockSize = chooseNearestBlockSize(resolvedState.availableBlockSizes, resolvedState.requestedBlockSize);

    if (resolvedState.requestedSampleRate > 0.0 && std::abs(chosenRate - resolvedState.requestedSampleRate) > 0.5)
    {
        resolvedState.statusMessages.push_back({
            AudioStatusCode::preferredValueUnsupported,
            AudioStatusSeverity::warning,
            "Requested sample rate is unsupported by the selected device.",
            "Requested " + formatRate(resolvedState.requestedSampleRate) + ", using nearest supported " + formatRate(chosenRate) + "."
        });
    }

    if (resolvedState.requestedBlockSize > 0 && chosenBlockSize != resolvedState.requestedBlockSize)
    {
        resolvedState.statusMessages.push_back({
            AudioStatusCode::preferredValueUnsupported,
            AudioStatusSeverity::warning,
            "Requested block size is unsupported by the selected device.",
            "Requested " + formatBlockSize(resolvedState.requestedBlockSize) + ", using nearest supported " + formatBlockSize(chosenBlockSize) + "."
        });
    }

    resolvedState.enabledInputChannels = sanitiseChannelIndices(state.deviceSelection.enabledInputChannels,
                                                                resolvedState.inputChannelNames.size(),
                                                                juce::jmin(resolvedState.requestedInputChannels,
                                                                           resolvedState.inputChannelNames.size()));
    resolvedState.enabledOutputChannels = sanitiseChannelIndices(state.deviceSelection.enabledOutputChannels,
                                                                 resolvedState.outputChannelNames.size(),
                                                                 juce::jmax(1, juce::jmin(resolvedState.requestedOutputChannels,
                                                                                           resolvedState.outputChannelNames.size())));

    juce::AudioDeviceManager::AudioDeviceSetup nextSetup = currentSetup;
    nextSetup.inputDeviceName = resolvedState.inputDeviceName;
    nextSetup.outputDeviceName = resolvedState.outputDeviceName;
    nextSetup.sampleRate = chosenRate;
    nextSetup.bufferSize = chosenBlockSize;
    nextSetup.useDefaultInputChannels = false;
    nextSetup.useDefaultOutputChannels = false;
    nextSetup.inputChannels = createChannelMask(resolvedState.enabledInputChannels, resolvedState.inputChannelNames.size());
    nextSetup.outputChannels = createChannelMask(resolvedState.enabledOutputChannels, resolvedState.outputChannelNames.size());

    currentLogicalInputChannels.store(resolvedState.requestedInputChannels, std::memory_order_relaxed);
    currentLogicalOutputChannels.store(resolvedState.requestedOutputChannels, std::memory_order_relaxed);

    if (applySetup)
    {
        const auto error = deviceManager.setAudioDeviceSetup(nextSetup, true);

        if (error.isNotEmpty())
        {
            resolvedState.statusMessages.push_back({
                AudioStatusCode::activeValueAdjustedByDevice,
                AudioStatusSeverity::error,
                "Failed to apply requested audio device configuration.",
                error
            });
        }
    }

    juce::AudioDeviceManager::AudioDeviceSetup appliedSetup;
    deviceManager.getAudioDeviceSetup(appliedSetup);

    if (auto* activeDevice = deviceManager.getCurrentAudioDevice(); activeDevice != nullptr)
    {
        resolvedState.activeSampleRate = activeDevice->getCurrentSampleRate();
        resolvedState.activeBlockSize = activeDevice->getCurrentBufferSizeSamples();
        resolvedState.inputChannelNames = activeDevice->getInputChannelNames();
        resolvedState.outputChannelNames = activeDevice->getOutputChannelNames();
    }
    else
    {
        resolvedState.activeSampleRate = nextSetup.sampleRate;
        resolvedState.activeBlockSize = nextSetup.bufferSize;
    }

    resolvedState.enabledInputChannels = getSelectedChannelsFromMask(appliedSetup.inputChannels, resolvedState.inputChannelNames.size());
    resolvedState.enabledOutputChannels = getSelectedChannelsFromMask(appliedSetup.outputChannels, resolvedState.outputChannelNames.size());
    resolvedState.activeInputChannels = static_cast<int>(resolvedState.enabledInputChannels.size());
    resolvedState.activeOutputChannels = static_cast<int>(resolvedState.enabledOutputChannels.size());

    if (std::abs(resolvedState.activeSampleRate - chosenRate) > 0.5 || resolvedState.activeBlockSize != chosenBlockSize)
    {
        resolvedState.statusMessages.push_back({
            AudioStatusCode::activeValueAdjustedByDevice,
            AudioStatusSeverity::warning,
            "Active device settings differ from the requested values.",
            "Active " + formatRate(resolvedState.activeSampleRate) + ", block " + formatBlockSize(resolvedState.activeBlockSize)
            + "; requested " + formatRate(chosenRate) + ", block " + formatBlockSize(chosenBlockSize) + "."
        });
    }

    auto resolvedInputRouting = state.deviceSelection.inputRouting;
    auto resolvedOutputRouting = state.deviceSelection.outputRouting;

    for (int channel = 0; channel < DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS; ++channel)
    {
        const auto desiredInputPhysical = channel < resolvedState.requestedInputChannels
                                        ? resolvedInputRouting[static_cast<std::size_t>(channel)] : -1;
        const auto desiredOutputPhysical = channel < resolvedState.requestedOutputChannels
                                         ? resolvedOutputRouting[static_cast<std::size_t>(channel)] : -1;

        auto inputIterator = std::find(resolvedState.enabledInputChannels.begin(),
                                       resolvedState.enabledInputChannels.end(),
                                       desiredInputPhysical);
        auto outputIterator = std::find(resolvedState.enabledOutputChannels.begin(),
                                        resolvedState.enabledOutputChannels.end(),
                                        desiredOutputPhysical);

        resolvedState.resolvedInputSlots[static_cast<std::size_t>(channel)] = inputIterator != resolvedState.enabledInputChannels.end()
                                                                             ? static_cast<int>(std::distance(resolvedState.enabledInputChannels.begin(), inputIterator))
                                                                             : -1;
        resolvedState.resolvedOutputSlots[static_cast<std::size_t>(channel)] = outputIterator != resolvedState.enabledOutputChannels.end()
                                                                              ? static_cast<int>(std::distance(resolvedState.enabledOutputChannels.begin(), outputIterator))
                                                                              : -1;
    }

    if (resolvedState.requestedInputChannels > 1 && resolvedState.activeInputChannels < resolvedState.requestedInputChannels)
    {
        resolvedState.statusMessages.push_back({
            AudioStatusCode::inputRoutingReduced,
            AudioStatusSeverity::warning,
            "Stereo input requested but the selected routing provides fewer channels.",
            "Missing DSP inputs will mirror the first available input or be filled with silence."
        });
    }

    if (resolvedState.requestedOutputChannels > 1 && resolvedState.activeOutputChannels < resolvedState.requestedOutputChannels)
    {
        resolvedState.statusMessages.push_back({
            AudioStatusCode::stereoRequestedButUnavailable,
            AudioStatusSeverity::warning,
            "Stereo output requested but the selected device routing is narrower.",
            "The processed stereo signal will be downmixed to the available output channels."
        });
    }

    if (resolvedState.requestedOutputChannels == 1 && resolvedState.activeOutputChannels > 1)
    {
        resolvedState.statusMessages.push_back({
            AudioStatusCode::monoExpandedToStereo,
            AudioStatusSeverity::info,
            "Mono processing is expanded to stereo outputs.",
            "The mono output will be copied to each active output channel."
        });
    }

    if (resolvedState.requestedOutputChannels > 1 && resolvedState.activeOutputChannels == 1)
    {
        resolvedState.statusMessages.push_back({
            AudioStatusCode::stereoDownmixedToMono,
            AudioStatusSeverity::warning,
            "Stereo processing is downmixed to mono.",
            "Only one active output channel is available, so left and right are mixed together."
        });
    }

    resolvedState.projectAudioState.deviceSelection.inputDeviceName = resolvedState.inputDeviceName;
    resolvedState.projectAudioState.deviceSelection.outputDeviceName = resolvedState.outputDeviceName;
    resolvedState.projectAudioState.deviceSelection.enabledInputChannels = resolvedState.enabledInputChannels;
    resolvedState.projectAudioState.deviceSelection.enabledOutputChannels = resolvedState.enabledOutputChannels;
    resolvedState.projectAudioState.lastKnownActual.inputDeviceName = resolvedState.inputDeviceName;
    resolvedState.projectAudioState.lastKnownActual.outputDeviceName = resolvedState.outputDeviceName;
    resolvedState.projectAudioState.lastKnownActual.sampleRate = resolvedState.activeSampleRate;
    resolvedState.projectAudioState.lastKnownActual.blockSize = resolvedState.activeBlockSize;
    resolvedState.projectAudioState.lastKnownActual.activeInputChannels = resolvedState.activeInputChannels;
    resolvedState.projectAudioState.lastKnownActual.activeOutputChannels = resolvedState.activeOutputChannels;

    return resolvedState;
}

std::unique_ptr<juce::AudioIODevice> AudioEngine::createProbeDevice(const juce::String& inputDeviceName,
                                                                    const juce::String& outputDeviceName) const
{
    if (auto* deviceType = deviceManager.getCurrentDeviceTypeObject(); deviceType != nullptr)
        return std::unique_ptr<juce::AudioIODevice>(deviceType->createDevice(outputDeviceName, inputDeviceName));

    return {};
}

void AudioEngine::publishResolvedDeviceState(const ResolvedDeviceState& resolvedState)
{
    for (int index = 0; index < DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS; ++index)
    {
        currentResolvedInputSlots[static_cast<std::size_t>(index)].store(resolvedState.resolvedInputSlots[static_cast<std::size_t>(index)],
                                                                         std::memory_order_relaxed);
        currentResolvedOutputSlots[static_cast<std::size_t>(index)].store(resolvedState.resolvedOutputSlots[static_cast<std::size_t>(index)],
                                                                          std::memory_order_relaxed);
    }

    const juce::ScopedLock lock(stateLock);
    currentProjectAudioState = resolvedState.projectAudioState;
    currentInputDeviceName = resolvedState.inputDeviceName;
    currentOutputDeviceName = resolvedState.outputDeviceName;
    availableInputDevices = resolvedState.availableInputDevices;
    availableOutputDevices = resolvedState.availableOutputDevices;
    availableSampleRates = resolvedState.availableSampleRates;
    availableBlockSizes = resolvedState.availableBlockSizes;
    inputChannelNames = resolvedState.inputChannelNames;
    outputChannelNames = resolvedState.outputChannelNames;
    enabledInputChannels = resolvedState.enabledInputChannels;
    enabledOutputChannels = resolvedState.enabledOutputChannels;
    currentStatusMessages = resolvedState.statusMessages;
    requestedSampleRate = resolvedState.requestedSampleRate;
    requestedBlockSize = resolvedState.requestedBlockSize;
    requestedInputChannels = resolvedState.requestedInputChannels;
    requestedOutputChannels = resolvedState.requestedOutputChannels;
}

void AudioEngine::routeHardwareInput(const float* const* inputChannelData,
                                     int numInputChannels,
                                     int logicalInputChannels,
                                     int numSamples) noexcept
{
    for (int channel = 0; channel < logicalInputChannels; ++channel)
    {
        auto* destination = sourceBuffer.getWritePointer(channel);
        const auto slot = currentResolvedInputSlots[static_cast<std::size_t>(channel)].load(std::memory_order_relaxed);

        if (juce::isPositiveAndBelow(slot, numInputChannels) && inputChannelData[slot] != nullptr)
        {
            juce::FloatVectorOperations::copy(destination, inputChannelData[slot], numSamples);
            continue;
        }

        if (channel > 0)
        {
            const auto fallbackSlot = currentResolvedInputSlots[0].load(std::memory_order_relaxed);

            if (juce::isPositiveAndBelow(fallbackSlot, numInputChannels) && inputChannelData[fallbackSlot] != nullptr)
            {
                juce::FloatVectorOperations::copy(destination, inputChannelData[fallbackSlot], numSamples);
                continue;
            }
        }

        juce::FloatVectorOperations::clear(destination, numSamples);
    }
}

void AudioEngine::generateInternalSource(int logicalInputChannels, int numSamples) noexcept
{
    if (logicalInputChannels <= 0)
        return;

    std::array<float*, DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS> sourcePointers {};

    for (int channel = 0; channel < logicalInputChannels; ++channel)
        sourcePointers[static_cast<std::size_t>(channel)] = sourceBuffer.getWritePointer(channel);

    switch (sourceType.load(std::memory_order_relaxed))
    {
        case SourceType::sine:       sineSource.generate(sourcePointers.data(), logicalInputChannels, numSamples); break;
        case SourceType::whiteNoise: whiteNoiseSource.generate(sourcePointers.data(), logicalInputChannels, numSamples); break;
        case SourceType::impulse:    impulseSource.generate(sourcePointers.data(), logicalInputChannels, numSamples); break;
        case SourceType::wavFile:    wavFileSource.generate(sourcePointers.data(), logicalInputChannels, numSamples); break;
        case SourceType::hardwareInput: break;
    }
}

void AudioEngine::routeProcessedToOutputs(float* const* outputChannelData,
                                          int numOutputChannels,
                                          int logicalOutputChannels,
                                          int numSamples) noexcept
{
    if (outputChannelData == nullptr || numOutputChannels <= 0 || logicalOutputChannels <= 0)
        return;

    if (logicalOutputChannels == 1)
    {
        const auto* mono = processedBuffer.getReadPointer(0);

        for (int channel = 0; channel < numOutputChannels; ++channel)
            if (outputChannelData[channel] != nullptr)
                juce::FloatVectorOperations::copy(outputChannelData[channel], mono, numSamples);

        return;
    }

    const auto leftSlot = currentResolvedOutputSlots[0].load(std::memory_order_relaxed);
    const auto rightSlot = currentResolvedOutputSlots[1].load(std::memory_order_relaxed);
    const auto* left = processedBuffer.getReadPointer(0);
    const auto* right = processedBuffer.getReadPointer(1);

    if (juce::isPositiveAndBelow(leftSlot, numOutputChannels) && outputChannelData[leftSlot] != nullptr)
        juce::FloatVectorOperations::copy(outputChannelData[leftSlot], left, numSamples);

    if (juce::isPositiveAndBelow(rightSlot, numOutputChannels) && rightSlot != leftSlot && outputChannelData[rightSlot] != nullptr)
    {
        juce::FloatVectorOperations::copy(outputChannelData[rightSlot], right, numSamples);
        return;
    }

    const auto mixSlot = juce::isPositiveAndBelow(leftSlot, numOutputChannels) ? leftSlot : 0;

    if (outputChannelData[mixSlot] == nullptr)
        return;

    for (int sample = 0; sample < numSamples; ++sample)
        outputChannelData[mixSlot][sample] = 0.5f * (left[sample] + right[sample]);
}

void AudioEngine::updateStatusTexts(Snapshot& snapshot) const
{
    snapshot.preferredStatusText = "Preferred: ";

    if (snapshot.preferredAudioConfiguration.valid)
    {
        snapshot.preferredStatusText << formatRate(snapshot.preferredAudioConfiguration.sampleRate)
                                     << " / " << formatBlockSize(snapshot.preferredAudioConfiguration.blockSize)
                                     << " / "
                                     << formatChannelPair(snapshot.preferredAudioConfiguration.preferredInputChannels,
                                                          snapshot.preferredAudioConfiguration.preferredOutputChannels);
    }
    else
    {
        snapshot.preferredStatusText << "No preferred DSP configuration";
    }

    snapshot.requestedStatusText = "Requested: " + formatRate(snapshot.requestedSampleRate)
                                 + " / " + formatBlockSize(snapshot.requestedBlockSize)
                                 + " / " + formatChannelPair(snapshot.requestedInputChannels, snapshot.requestedOutputChannels);
    snapshot.actualStatusText = "Active: " + formatRate(snapshot.lastKnownActual.sampleRate)
                              + " / " + formatBlockSize(snapshot.lastKnownActual.blockSize)
                              + " / " + formatChannelPair(snapshot.lastKnownActual.activeInputChannels,
                                                          snapshot.lastKnownActual.activeOutputChannels);

    juce::StringArray overrideFlags;

    if (snapshot.overrides.sampleRateOverridden)
        overrideFlags.add("sample rate");
    if (snapshot.overrides.blockSizeOverridden)
        overrideFlags.add("block size");
    if (snapshot.overrides.inputDeviceOverridden)
        overrideFlags.add("input device");
    if (snapshot.overrides.outputDeviceOverridden)
        overrideFlags.add("output device");
    if (snapshot.overrides.inputChannelsOverridden)
        overrideFlags.add("input channels");
    if (snapshot.overrides.outputChannelsOverridden)
        overrideFlags.add("output channels");
    if (snapshot.overrides.routingOverridden)
        overrideFlags.add("routing");

    snapshot.overrideStatusText = overrideFlags.isEmpty()
                                ? "Overrides: none"
                                : "Overrides: " + overrideFlags.joinIntoString(", ");

    juce::StringArray warnings;

    for (const auto& statusMessage : snapshot.statusMessages)
        if (statusMessage.severity != AudioStatusSeverity::info)
            warnings.add(statusMessage.summary);

    snapshot.warningStatusText = warnings.isEmpty()
                               ? "Warnings: none"
                               : "Warnings: " + warnings.joinIntoString(" | ");
}

std::vector<int> AudioEngine::getSelectedChannelsFromMask(const juce::BigInteger& mask, int maxChannels) const
{
    std::vector<int> result;

    for (int index = 0; index < maxChannels; ++index)
        if (mask[index])
            result.push_back(index);

    return result;
}

juce::BigInteger AudioEngine::createChannelMask(const std::vector<int>& indices, int maxChannels) const
{
    juce::BigInteger mask;

    for (const auto index : indices)
        if (juce::isPositiveAndBelow(index, maxChannels))
            mask.setBit(index);

    return mask;
}
