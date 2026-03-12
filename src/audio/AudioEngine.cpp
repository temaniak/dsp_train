#include "audio/AudioEngine.h"

AudioEngine::AudioEngine()
    : oscilloscopeBuffer(32768)
{
    audioFormatManager.registerBasicFormats();
}

AudioEngine::~AudioEngine()
{
    deviceManager.removeAudioCallback(this);
    deviceManager.closeAudioDevice();
}

juce::Result AudioEngine::initialise()
{
    const auto error = deviceManager.initialise(0, 2, nullptr, true);

    if (error.isNotEmpty())
        return juce::Result::fail(error);

    deviceManager.addAudioCallback(this);
    return juce::Result::ok();
}

AudioEngine::Snapshot AudioEngine::getSnapshot() const
{
    Snapshot snapshot;
    snapshot.transportRunning = transportRunning.load(std::memory_order_relaxed);
    snapshot.sampleRate = currentSampleRate.load(std::memory_order_relaxed);
    snapshot.blockSize = currentBlockSize.load(std::memory_order_relaxed);
    snapshot.sourceType = sourceType.load(std::memory_order_relaxed);
    snapshot.processingMode = processingMode.load(std::memory_order_relaxed);
    snapshot.sourceGain = sourceGain.load(std::memory_order_relaxed);
    snapshot.sineFrequency = sineSource.getFrequency();
    snapshot.wavLoaded = wavFileSource.hasFileLoaded();
    snapshot.wavLooping = wavFileSource.isLooping();
    snapshot.wavPosition = wavFileSource.getPositionNormalized();
    snapshot.wavFileName = wavFileSource.getLoadedFileName();
    snapshot.builtinProcessor = builtinProcessor.getSelectedProcessor();

    const juce::ScopedLock lock(stateLock);
    snapshot.deviceError = lastDeviceError;
    return snapshot;
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

void AudioEngine::setProcessingMode(ProcessingMode mode) noexcept
{
    processingMode.store(mode, std::memory_order_relaxed);
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

void AudioEngine::setBuiltinProcessorType(BuiltinProcessorType type) noexcept
{
    builtinProcessor.setSelectedProcessor(type);
    builtinProcessor.resetParametersToDefaults(type);
    requestResetForNextBlock();
}

BuiltinProcessorType AudioEngine::getBuiltinProcessorType() const noexcept
{
    return builtinProcessor.getSelectedProcessor();
}

void AudioEngine::setBuiltinParameter(int index, float value) noexcept
{
    builtinProcessor.setParameter(index, value);
}

float AudioEngine::getBuiltinParameter(int index) const noexcept
{
    return builtinProcessor.getParameter(index);
}

BuiltinPresetData AudioEngine::captureBuiltinPreset() const noexcept
{
    return builtinProcessor.capturePresetData();
}

void AudioEngine::applyBuiltinPreset(const BuiltinPresetData& preset) noexcept
{
    builtinProcessor.applyPresetData(preset);
    requestResetForNextBlock();
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

void AudioEngine::audioDeviceIOCallbackWithContext(const float* const*,
                                                   int,
                                                   float* const* outputChannelData,
                                                   int numOutputChannels,
                                                   int numSamples,
                                                   const juce::AudioIODeviceCallbackContext&)
{
    for (int channel = 0; channel < numOutputChannels; ++channel)
        if (outputChannelData[channel] != nullptr)
            juce::FloatVectorOperations::clear(outputChannelData[channel], numSamples);

    if (numOutputChannels <= 0 || numSamples <= 0)
        return;

    if (numSamples > sourceBuffer.getNumSamples() || numSamples > processedBuffer.getNumSamples())
        return;

    if (resetForNextBlock.exchange(false, std::memory_order_relaxed))
    {
        resetSourcesForCurrentSelection();
        builtinProcessor.reset();
        userDspHost.requestReset();
    }

    if (! transportRunning.load(std::memory_order_relaxed))
        return;

    auto* generated = sourceBuffer.getWritePointer(0);
    auto* processed = processedBuffer.getWritePointer(0);
    const auto currentSource = sourceType.load(std::memory_order_relaxed);

    switch (currentSource)
    {
        case SourceType::sine:       sineSource.generate(generated, numSamples); break;
        case SourceType::whiteNoise: whiteNoiseSource.generate(generated, numSamples); break;
        case SourceType::impulse:    impulseSource.generate(generated, numSamples); break;
        case SourceType::wavFile:    wavFileSource.generate(generated, numSamples); break;
    }

    juce::FloatVectorOperations::multiply(generated, sourceGain.load(std::memory_order_relaxed), numSamples);

    if (processingMode.load(std::memory_order_relaxed) == ProcessingMode::builtIn)
        builtinProcessor.process(generated, processed, numSamples);
    else
        userDspHost.process(generated, processed, numSamples);

    oscilloscopeBuffer.pushSamples(processed, numSamples);

    for (int channel = 0; channel < numOutputChannels; ++channel)
        if (outputChannelData[channel] != nullptr)
            juce::FloatVectorOperations::copy(outputChannelData[channel], processed, numSamples);
}

void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    const auto sampleRate = device != nullptr ? device->getCurrentSampleRate() : 44100.0;
    const auto blockSize = device != nullptr ? device->getCurrentBufferSizeSamples() : 512;

    currentSampleRate.store(sampleRate, std::memory_order_relaxed);
    currentBlockSize.store(blockSize, std::memory_order_relaxed);

    sourceBuffer.setSize(1, juce::jmax(1, blockSize), false, false, true);
    processedBuffer.setSize(1, juce::jmax(1, blockSize), false, false, true);
    oscilloscopeBuffer.clear();

    sineSource.prepare(sampleRate, blockSize);
    whiteNoiseSource.prepare(sampleRate, blockSize);
    impulseSource.prepare(sampleRate, blockSize);
    wavFileSource.prepare(sampleRate, blockSize);
    builtinProcessor.prepare(sampleRate, blockSize);
    userDspHost.prepare(sampleRate, blockSize);

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
        case SourceType::wavFile:    break;
    }
}
