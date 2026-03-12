#pragma once

#include <JuceHeader.h>

#include "audio/SignalSources.h"
#include "audio/WavFileSource.h"
#include "dsp/BuiltinProcessors.h"
#include "ui/OscilloscopeBuffer.h"
#include "userdsp/UserDspHost.h"

enum class ProcessingMode
{
    builtIn,
    userModule
};

class AudioEngine final : public juce::AudioIODeviceCallback
{
public:
    struct Snapshot
    {
        bool transportRunning = false;
        double sampleRate = 0.0;
        int blockSize = 0;
        SourceType sourceType = SourceType::sine;
        ProcessingMode processingMode = ProcessingMode::builtIn;
        float sourceGain = 0.2f;
        float sineFrequency = 440.0f;
        bool wavLoaded = false;
        bool wavLooping = false;
        double wavPosition = 0.0;
        juce::String wavFileName = "No WAV loaded";
        juce::String deviceError;
        BuiltinProcessorType builtinProcessor = BuiltinProcessorType::bypass;
    };

    AudioEngine();
    ~AudioEngine() override;

    juce::Result initialise();
    Snapshot getSnapshot() const;

    void startTransport() noexcept;
    void stopTransport() noexcept;

    void setSourceType(SourceType type) noexcept;
    void setProcessingMode(ProcessingMode mode) noexcept;
    void setSourceGain(float newGain) noexcept;
    void setSineFrequency(float frequency) noexcept;

    void setBuiltinProcessorType(BuiltinProcessorType type) noexcept;
    BuiltinProcessorType getBuiltinProcessorType() const noexcept;
    void setBuiltinParameter(int index, float value) noexcept;
    float getBuiltinParameter(int index) const noexcept;
    BuiltinPresetData captureBuiltinPreset() const noexcept;
    void applyBuiltinPreset(const BuiltinPresetData& preset) noexcept;

    juce::Result loadWavFile(const juce::File& file);
    void setWavLooping(bool shouldLoop) noexcept;
    void setWavPositionNormalized(double newPosition) noexcept;

    void setUserControlValue(int index, float value) noexcept;

    OscilloscopeBuffer& getOscilloscopeBuffer() noexcept;
    UserDspHost& getUserDspHost() noexcept;

    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceError(const juce::String& errorMessage) override;

private:
    void requestResetForNextBlock() noexcept;
    void resetSourcesForCurrentSelection() noexcept;

    juce::AudioDeviceManager deviceManager;
    juce::AudioFormatManager audioFormatManager;
    OscilloscopeBuffer oscilloscopeBuffer;
    UserDspHost userDspHost;
    BuiltinProcessorChain builtinProcessor;
    SineSource sineSource;
    WhiteNoiseSource whiteNoiseSource;
    ImpulseSource impulseSource;
    WavFileSource wavFileSource;

    juce::AudioBuffer<float> sourceBuffer;
    juce::AudioBuffer<float> processedBuffer;

    std::atomic<bool> transportRunning { false };
    std::atomic<SourceType> sourceType { SourceType::sine };
    std::atomic<ProcessingMode> processingMode { ProcessingMode::builtIn };
    std::atomic<float> sourceGain { 0.2f };
    std::atomic<double> currentSampleRate { 0.0 };
    std::atomic<int> currentBlockSize { 0 };
    std::atomic<bool> resetForNextBlock { true };

    mutable juce::CriticalSection stateLock;
    juce::String lastDeviceError;
};
