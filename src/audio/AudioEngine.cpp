#include "audio/AudioEngine.h"

#include <set>

#include "util/AppSettings.h"

namespace
{
enum class MidiMessageKind
{
    none = 0,
    cc,
    noteOn,
    noteOff,
    pitchWheel
};

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

std::vector<int> collectRoutedChannels(const std::array<int, DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS>& routing,
                                       int requestedChannels,
                                       int maxChannels)
{
    std::set<int> unique;

    for (int index = 0; index < juce::jmin(requestedChannels, DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS); ++index)
    {
        const auto physicalChannel = routing[static_cast<std::size_t>(index)];

        if (juce::isPositiveAndBelow(physicalChannel, maxChannels))
            unique.insert(physicalChannel);
    }

    return { unique.begin(), unique.end() };
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

juce::String buildMidiMessageSummary(MidiMessageKind kind, int channel, int data1, int data2)
{
    if (kind == MidiMessageKind::none || channel <= 0)
        return {};

    const auto channelText = "Ch " + juce::String(channel);

    switch (kind)
    {
        case MidiMessageKind::cc:
            return "Last MIDI: CC " + juce::String(data1) + " = " + juce::String(data2) + " / " + channelText;

        case MidiMessageKind::noteOn:
        case MidiMessageKind::noteOff:
        {
            const auto noteName = juce::MidiMessage::getMidiNoteName(data1, true, true, 3);
            return "Last MIDI: " + juce::String(kind == MidiMessageKind::noteOn ? "Note On " : "Note Off ")
                 + noteName + " / " + channelText;
        }

        case MidiMessageKind::pitchWheel:
            return "Last MIDI: Pitch Wheel " + juce::String(data2) + " / " + channelText;

        case MidiMessageKind::none:
            break;
    }

    return {};
}

bool controllerDefinitionsMatchRuntime(const std::vector<UserDspControllerDefinition>& definitions,
                                       const UserDspHost::Snapshot& snapshot)
{
    if (! snapshot.hasActiveModule || snapshot.controlCount != static_cast<int>(definitions.size()))
        return false;

    for (int index = 0; index < snapshot.controlCount; ++index)
    {
        const auto& compiledControl = snapshot.controls[static_cast<std::size_t>(index)];
        const auto& projectControl = definitions[static_cast<std::size_t>(index)];

        if (! compiledControl.active
            || compiledControl.type != projectControl.type
            || compiledControl.label != projectControl.label
            || compiledControl.codeName != projectControl.codeName)
        {
            return false;
        }
    }

    return true;
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

    clearMidiBindingState();
    resetMidiPerformanceState();
}

AudioEngine::~AudioEngine()
{
    if (midiInputDevice != nullptr)
        midiInputDevice->stop();

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

    const auto settings = appsettings::loadAppSettings();

    if (const auto midiSelectionResult = applyMidiInputSelection(settings.selectedMidiInputDeviceName, true, true); midiSelectionResult.failed())
    {
        const juce::ScopedLock lock(stateLock);
        currentMidiInputStatusText = midiSelectionResult.getErrorMessage();
    }

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
    snapshot.midiInputDeviceName = currentMidiInputDeviceName;
    snapshot.midiInputStatusText = currentMidiInputStatusText;
    snapshot.midiLastMessageKind = lastMidiMessageKind.load(std::memory_order_relaxed);
    snapshot.midiLastMessageChannel = lastMidiMessageChannel.load(std::memory_order_relaxed);
    snapshot.midiLastMessageData1 = lastMidiMessageData1.load(std::memory_order_relaxed);
    snapshot.midiLastMessageData2 = lastMidiMessageData2.load(std::memory_order_relaxed);
    snapshot.midiLastMessageText = buildMidiMessageSummary(static_cast<MidiMessageKind>(snapshot.midiLastMessageKind),
                                                          snapshot.midiLastMessageChannel,
                                                          snapshot.midiLastMessageData1,
                                                          snapshot.midiLastMessageData2);
    snapshot.availableSampleRates = availableSampleRates;
    snapshot.availableBlockSizes = availableBlockSizes;
    snapshot.inputChannelNames = inputChannelNames;
    snapshot.outputChannelNames = outputChannelNames;
    snapshot.enabledInputChannels = enabledInputChannels;
    snapshot.enabledOutputChannels = enabledOutputChannels;
    for (const auto& deviceInfo : availableMidiInputDevices)
        snapshot.availableMidiInputDevices.add(deviceInfo.name);
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

juce::Result AudioEngine::setSelectedMidiInputDevice(const juce::String& deviceName)
{
    return applyMidiInputSelection(deviceName.trim(), true, false);
}

void AudioEngine::refreshMidiInputs()
{
    const auto availableDevices = juce::MidiInput::getAvailableDevices();
    juce::String currentDeviceName;
    bool hasOpenDevice = false;
    bool currentDeviceAvailable = false;

    {
        const juce::ScopedLock lock(stateLock);
        currentDeviceName = currentMidiInputDeviceName;
        hasOpenDevice = midiInputDevice != nullptr;
        availableMidiInputDevices = availableDevices;
    }

    for (const auto& device : availableDevices)
        if (device.name == currentDeviceName)
            currentDeviceAvailable = true;

    if (currentDeviceName.isEmpty())
    {
        if (! availableDevices.isEmpty())
            (void) applyMidiInputSelection({}, true, true);

        return;
    }

    if (! currentDeviceAvailable)
    {
        (void) applyMidiInputSelection(currentDeviceName, true, true);
        return;
    }

    if (! hasOpenDevice)
        (void) applyMidiInputSelection(currentDeviceName, false, false);
}

void AudioEngine::setProjectControllerDefinitions(const std::vector<UserDspControllerDefinition>& definitions)
{
    const auto runtimeSnapshot = userDspHost.getSnapshot();
    const auto layoutMatchesRuntime = controllerDefinitionsMatchRuntime(definitions, runtimeSnapshot);

    for (int index = 0; index < DSP_EDU_USER_DSP_MAX_CONTROLS; ++index)
    {
        MidiBinding binding;

        if (index < static_cast<int>(definitions.size()))
            binding = definitions[static_cast<std::size_t>(index)].midiBinding;

        writeMidiBindingState(index, binding, layoutMatchesRuntime);
    }
}

float AudioEngine::getPreviewControlValue(int index) const noexcept
{
    if (! juce::isPositiveAndBelow(index, DSP_EDU_USER_DSP_MAX_CONTROLS))
        return 0.0f;

    return previewControlValues[static_cast<std::size_t>(index)].load(std::memory_order_relaxed);
}

std::uint32_t AudioEngine::getPreviewControlGeneration(int index) const noexcept
{
    if (! juce::isPositiveAndBelow(index, DSP_EDU_USER_DSP_MAX_CONTROLS))
        return 0;

    return previewControlGenerations[static_cast<std::size_t>(index)].load(std::memory_order_relaxed);
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

void AudioEngine::armMidiLearn(int controlIndex, MidiBindingSource source) noexcept
{
    if (! juce::isPositiveAndBelow(controlIndex, DSP_EDU_USER_DSP_MAX_CONTROLS))
    {
        cancelMidiLearn();
        return;
    }

    midiLearnCapturePending.store(false, std::memory_order_release);
    armedMidiLearnControlIndex.store(controlIndex, std::memory_order_release);
    armedMidiLearnSource.store(static_cast<int>(source), std::memory_order_release);
}

void AudioEngine::cancelMidiLearn() noexcept
{
    armedMidiLearnControlIndex.store(-1, std::memory_order_release);
    armedMidiLearnSource.store(static_cast<int>(MidiBindingSource::none), std::memory_order_release);
    midiLearnCapturePending.store(false, std::memory_order_release);
}

bool AudioEngine::consumeMidiLearnCapture(MidiLearnCapture& capture) noexcept
{
    if (! midiLearnCapturePending.exchange(false, std::memory_order_acq_rel))
        return false;

    capture.controlIndex = midiLearnCaptureControlIndex.load(std::memory_order_acquire);
    capture.binding.source = static_cast<MidiBindingSource>(midiLearnCaptureSource.load(std::memory_order_acquire));
    capture.binding.channel = midiLearnCaptureChannel.load(std::memory_order_acquire);
    capture.binding.data1 = midiLearnCaptureData1.load(std::memory_order_acquire);
    return true;
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

    userDspHost.setMidiInputState(buildCurrentMidiState());
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
    const auto preferred = state.cachedPreferred;

    if (auto* deviceType = deviceManager.getCurrentDeviceTypeObject(); deviceType != nullptr)
    {
        deviceType->scanForDevices();
        resolvedState.availableInputDevices = deviceType->getDeviceNames(true);
        resolvedState.availableOutputDevices = deviceType->getDeviceNames(false);
    }

    juce::AudioDeviceManager::AudioDeviceSetup currentSetup;
    deviceManager.getAudioDeviceSetup(currentSetup);

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

    resolvedState.enabledInputChannels = collectRoutedChannels(state.deviceSelection.inputRouting,
                                                               resolvedState.requestedInputChannels,
                                                               resolvedState.inputChannelNames.size());
    resolvedState.enabledOutputChannels = collectRoutedChannels(state.deviceSelection.outputRouting,
                                                                resolvedState.requestedOutputChannels,
                                                                resolvedState.outputChannelNames.size());

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
    resolvedState.projectAudioState.overrides.inputChannelsOverridden = false;
    resolvedState.projectAudioState.overrides.outputChannelsOverridden = false;
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

juce::Result AudioEngine::applyMidiInputSelection(const juce::String& requestedDeviceName,
                                                  bool persistSelection,
                                                  bool fallbackFromUnavailableDevice)
{
    const auto availableDevices = juce::MidiInput::getAvailableDevices();
    juce::MidiDeviceInfo selectedDevice;
    juce::String statusText;

    auto findDeviceByName = [&availableDevices] (const juce::String& deviceName) -> juce::MidiDeviceInfo
    {
        for (const auto& device : availableDevices)
            if (device.name == deviceName)
                return device;

        return {};
    };

    const auto requested = requestedDeviceName.trim();

    if (requested.isNotEmpty())
        selectedDevice = findDeviceByName(requested);

    if (selectedDevice.identifier.isEmpty()
        && ! availableDevices.isEmpty()
        && (requested.isEmpty() || fallbackFromUnavailableDevice))
    {
        selectedDevice = availableDevices[0];

        if (requested.isNotEmpty() && fallbackFromUnavailableDevice)
        {
            statusText = requested + " is unavailable. Using " + selectedDevice.name + " instead.";
        }
        else if (requested.isEmpty())
        {
            statusText = "Using " + selectedDevice.name + ".";
        }
    }

    if (availableDevices.isEmpty())
    {
        if (midiInputDevice != nullptr)
            midiInputDevice->stop();

        midiInputDevice.reset();

        {
            const juce::ScopedLock lock(stateLock);
            availableMidiInputDevices = availableDevices;
            currentMidiInputDeviceName.clear();
            currentMidiInputStatusText = "No MIDI input devices available.";
        }

        if (persistSelection)
            persistMidiInputSelection({});

        return juce::Result::ok();
    }

    if (selectedDevice.identifier.isEmpty())
    {
        const juce::ScopedLock lock(stateLock);
        availableMidiInputDevices = availableDevices;
        currentMidiInputStatusText = "The selected MIDI input device is unavailable.";
        return juce::Result::fail("The selected MIDI input device is unavailable.");
    }

    auto newMidiInput = juce::MidiInput::openDevice(selectedDevice.identifier, this);

    if (newMidiInput == nullptr)
    {
        const juce::ScopedLock lock(stateLock);
        availableMidiInputDevices = availableDevices;
        currentMidiInputStatusText = "Failed to open MIDI input " + selectedDevice.name + ".";
        return juce::Result::fail("Failed to open MIDI input " + selectedDevice.name + ".");
    }

    newMidiInput->start();

    if (midiInputDevice != nullptr)
        midiInputDevice->stop();

    midiInputDevice = std::move(newMidiInput);

    {
        const juce::ScopedLock lock(stateLock);
        availableMidiInputDevices = availableDevices;
        currentMidiInputDeviceName = selectedDevice.name;
        currentMidiInputStatusText = statusText;
    }

    if (persistSelection)
        persistMidiInputSelection(selectedDevice.name);

    return juce::Result::ok();
}

void AudioEngine::persistMidiInputSelection(const juce::String& deviceName)
{
    AppSettingsState settings = appsettings::loadAppSettings();
    settings.selectedMidiInputDeviceName = deviceName.trim();

    if (const auto saveResult = appsettings::saveAppSettings(settings); saveResult.failed())
    {
        const juce::ScopedLock lock(stateLock);
        currentMidiInputStatusText = saveResult.getErrorMessage();
    }
}

void AudioEngine::writeMidiBindingState(int index, const MidiBinding& binding, bool routeToRuntime) noexcept
{
    if (! juce::isPositiveAndBelow(index, DSP_EDU_USER_DSP_MAX_CONTROLS))
        return;

    const auto sanitisedBinding = midi::sanitiseMidiBinding(binding);
    midiBindingSources[static_cast<std::size_t>(index)].store(static_cast<int>(MidiBindingSource::none), std::memory_order_release);
    midiBindingChannels[static_cast<std::size_t>(index)].store(sanitisedBinding.channel, std::memory_order_release);
    midiBindingData1[static_cast<std::size_t>(index)].store(sanitisedBinding.data1, std::memory_order_release);
    midiBindingRoutesToRuntime[static_cast<std::size_t>(index)].store(routeToRuntime && midi::isBindingActive(sanitisedBinding) ? 1 : 0,
                                                                      std::memory_order_release);
    midiBindingSources[static_cast<std::size_t>(index)].store(static_cast<int>(sanitisedBinding.source), std::memory_order_release);
}

void AudioEngine::clearMidiBindingState() noexcept
{
    for (int index = 0; index < DSP_EDU_USER_DSP_MAX_CONTROLS; ++index)
    {
        writeMidiBindingState(index, {}, false);
        previewControlValues[static_cast<std::size_t>(index)].store(0.0f, std::memory_order_relaxed);
        previewControlGenerations[static_cast<std::size_t>(index)].store(0, std::memory_order_relaxed);
    }
}

void AudioEngine::publishPreviewControlValue(int index, float value) noexcept
{
    if (! juce::isPositiveAndBelow(index, DSP_EDU_USER_DSP_MAX_CONTROLS))
        return;

    previewControlValues[static_cast<std::size_t>(index)].store(juce::jlimit(0.0f, 1.0f, value), std::memory_order_relaxed);
    previewControlGenerations[static_cast<std::size_t>(index)].fetch_add(1u, std::memory_order_relaxed);
}

void AudioEngine::resetMidiPerformanceState() noexcept
{
    midiPerformanceState.clear();
    publishMidiPerformanceState();
}

void AudioEngine::publishMidiPerformanceState() noexcept
{
    const auto& state = midiPerformanceState.getState();
    midiPerformanceStateRevision.fetch_add(1u, std::memory_order_acq_rel);
    midiStateVoiceCount.store(state.voiceCount, std::memory_order_relaxed);
    midiStateGate.store(state.gate, std::memory_order_relaxed);
    midiStateChannel.store(state.channel, std::memory_order_relaxed);
    midiStateNoteNumber.store(state.noteNumber, std::memory_order_relaxed);
    midiStateVelocity.store(state.velocity, std::memory_order_relaxed);
    midiStatePitchWheel.store(state.pitchWheel, std::memory_order_relaxed);

    for (int channel = 0; channel < DSP_EDU_USER_DSP_MAX_MIDI_CHANNELS; ++channel)
        midiStateChannelPitchWheels[static_cast<std::size_t>(channel)].store(state.channelPitchWheel[static_cast<std::size_t>(channel)],
                                                                             std::memory_order_relaxed);

    for (int voiceIndex = 0; voiceIndex < DSP_EDU_USER_DSP_MAX_MIDI_VOICES; ++voiceIndex)
    {
        const auto& voice = state.voices[static_cast<std::size_t>(voiceIndex)];
        midiStateVoiceActive[static_cast<std::size_t>(voiceIndex)].store(voice.active, std::memory_order_relaxed);
        midiStateVoiceChannels[static_cast<std::size_t>(voiceIndex)].store(voice.channel, std::memory_order_relaxed);
        midiStateVoiceNotes[static_cast<std::size_t>(voiceIndex)].store(voice.noteNumber, std::memory_order_relaxed);
        midiStateVoiceVelocities[static_cast<std::size_t>(voiceIndex)].store(voice.velocity, std::memory_order_relaxed);
        midiStateVoiceOrders[static_cast<std::size_t>(voiceIndex)].store(voice.order, std::memory_order_relaxed);
    }

    midiPerformanceStateRevision.fetch_add(1u, std::memory_order_release);
}

DspEduMidiState AudioEngine::buildCurrentMidiState() const noexcept
{
    DspEduMidiState midiState {};
    midiState.structSize = sizeof(DspEduMidiState);

    while (true)
    {
        const auto revisionBefore = midiPerformanceStateRevision.load(std::memory_order_acquire);

        if ((revisionBefore & 1u) != 0u)
            continue;

        midiState.voiceCount = midiStateVoiceCount.load(std::memory_order_relaxed);
        midiState.gate = midiStateGate.load(std::memory_order_relaxed);
        midiState.channel = midiStateChannel.load(std::memory_order_relaxed);
        midiState.noteNumber = midiStateNoteNumber.load(std::memory_order_relaxed);
        midiState.velocity = midiStateVelocity.load(std::memory_order_relaxed);
        midiState.pitchWheel = midiStatePitchWheel.load(std::memory_order_relaxed);

        for (int channel = 0; channel < DSP_EDU_USER_DSP_MAX_MIDI_CHANNELS; ++channel)
            midiState.channelPitchWheel[static_cast<std::size_t>(channel)] =
                midiStateChannelPitchWheels[static_cast<std::size_t>(channel)].load(std::memory_order_relaxed);

        for (int voiceIndex = 0; voiceIndex < DSP_EDU_USER_DSP_MAX_MIDI_VOICES; ++voiceIndex)
        {
            auto& voice = midiState.voices[static_cast<std::size_t>(voiceIndex)];
            voice.active = midiStateVoiceActive[static_cast<std::size_t>(voiceIndex)].load(std::memory_order_relaxed);
            voice.channel = midiStateVoiceChannels[static_cast<std::size_t>(voiceIndex)].load(std::memory_order_relaxed);
            voice.noteNumber = midiStateVoiceNotes[static_cast<std::size_t>(voiceIndex)].load(std::memory_order_relaxed);
            voice.velocity = midiStateVoiceVelocities[static_cast<std::size_t>(voiceIndex)].load(std::memory_order_relaxed);
            voice.order = midiStateVoiceOrders[static_cast<std::size_t>(voiceIndex)].load(std::memory_order_relaxed);
        }

        const auto revisionAfter = midiPerformanceStateRevision.load(std::memory_order_acquire);

        if (revisionBefore == revisionAfter)
            return midiState;
    }
}

float AudioEngine::normalisePitchWheelValue(int rawPitchWheelValue) const noexcept
{
    rawPitchWheelValue = juce::jlimit(0, 16383, rawPitchWheelValue);

    if (rawPitchWheelValue == 8192)
        return 0.0f;

    if (rawPitchWheelValue < 8192)
        return static_cast<float>(rawPitchWheelValue - 8192) / 8192.0f;

    return static_cast<float>(rawPitchWheelValue - 8192) / 8191.0f;
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

void AudioEngine::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& message)
{
    if (message.isController())
    {
        lastMidiMessageKind.store(static_cast<int>(MidiMessageKind::cc), std::memory_order_relaxed);
        lastMidiMessageChannel.store(message.getChannel(), std::memory_order_relaxed);
        lastMidiMessageData1.store(message.getControllerNumber(), std::memory_order_relaxed);
        lastMidiMessageData2.store(message.getControllerValue(), std::memory_order_relaxed);
    }
    else if (message.isNoteOn())
    {
        lastMidiMessageKind.store(static_cast<int>(MidiMessageKind::noteOn), std::memory_order_relaxed);
        lastMidiMessageChannel.store(message.getChannel(), std::memory_order_relaxed);
        lastMidiMessageData1.store(message.getNoteNumber(), std::memory_order_relaxed);
        lastMidiMessageData2.store(static_cast<int>(message.getVelocity()), std::memory_order_relaxed);
        midiPerformanceState.noteOn(message.getChannel(),
                                    message.getNoteNumber(),
                                    static_cast<float>(message.getVelocity()) / 127.0f);
        publishMidiPerformanceState();
    }
    else if (message.isNoteOff())
    {
        lastMidiMessageKind.store(static_cast<int>(MidiMessageKind::noteOff), std::memory_order_relaxed);
        lastMidiMessageChannel.store(message.getChannel(), std::memory_order_relaxed);
        lastMidiMessageData1.store(message.getNoteNumber(), std::memory_order_relaxed);
        lastMidiMessageData2.store(0, std::memory_order_relaxed);
        midiPerformanceState.noteOff(message.getChannel(), message.getNoteNumber());
        publishMidiPerformanceState();
    }
    else if (message.isPitchWheel())
    {
        lastMidiMessageKind.store(static_cast<int>(MidiMessageKind::pitchWheel), std::memory_order_relaxed);
        lastMidiMessageChannel.store(message.getChannel(), std::memory_order_relaxed);
        lastMidiMessageData1.store(-1, std::memory_order_relaxed);
        lastMidiMessageData2.store(message.getPitchWheelValue(), std::memory_order_relaxed);
        midiPerformanceState.setPitchWheel(message.getChannel(),
                                           normalisePitchWheelValue(message.getPitchWheelValue()));
        publishMidiPerformanceState();
    }

    const auto armedSource = static_cast<MidiBindingSource>(armedMidiLearnSource.load(std::memory_order_acquire));
    const auto armedControlIndex = armedMidiLearnControlIndex.load(std::memory_order_acquire);

    if (juce::isPositiveAndBelow(armedControlIndex, DSP_EDU_USER_DSP_MAX_CONTROLS))
    {
        MidiBinding capturedBinding;

        auto captureMessage = [&]() -> bool
        {
            if (armedSource != MidiBindingSource::none)
                return midi::tryCaptureMidiLearnBinding(armedSource, message, capturedBinding);

            if (message.isController())
                return midi::tryCaptureMidiLearnBinding(MidiBindingSource::cc, message, capturedBinding);

            if (message.isNoteOn())
                return midi::tryCaptureMidiLearnBinding(MidiBindingSource::noteGate, message, capturedBinding);

            if (message.isPitchWheel())
                return midi::tryCaptureMidiLearnBinding(MidiBindingSource::pitchWheel, message, capturedBinding);

            return false;
        };

        if (captureMessage())
        {
            midiLearnCaptureControlIndex.store(armedControlIndex, std::memory_order_release);
            midiLearnCaptureSource.store(static_cast<int>(capturedBinding.source), std::memory_order_release);
            midiLearnCaptureChannel.store(capturedBinding.channel, std::memory_order_release);
            midiLearnCaptureData1.store(capturedBinding.data1, std::memory_order_release);
            armedMidiLearnControlIndex.store(-1, std::memory_order_release);
            armedMidiLearnSource.store(static_cast<int>(MidiBindingSource::none), std::memory_order_release);
            midiLearnCapturePending.store(true, std::memory_order_release);
        }
    }

    for (int index = 0; index < DSP_EDU_USER_DSP_MAX_CONTROLS; ++index)
    {
        MidiBinding binding;
        binding.source = static_cast<MidiBindingSource>(midiBindingSources[static_cast<std::size_t>(index)].load(std::memory_order_acquire));

        if (binding.source == MidiBindingSource::none)
            continue;

        binding.channel = midiBindingChannels[static_cast<std::size_t>(index)].load(std::memory_order_acquire);
        binding.data1 = midiBindingData1[static_cast<std::size_t>(index)].load(std::memory_order_acquire);

        float mappedValue = 0.0f;

        if (midi::tryMapMessageToBindingValue(message, binding, mappedValue))
        {
            publishPreviewControlValue(index, mappedValue);

            if (midiBindingRoutesToRuntime[static_cast<std::size_t>(index)].load(std::memory_order_acquire) != 0)
                userDspHost.setControlValue(index, mappedValue);
        }
    }
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
