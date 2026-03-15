#pragma once

#include <JuceHeader.h>

#include "audio/AudioConfiguration.h"
#include "audio/SignalSources.h"
#include "audio/WavFileSource.h"
#include "midi/MidiBinding.h"
#include "midi/MidiPerformanceState.h"
#include "ui/OscilloscopeBuffer.h"
#include "userdsp/UserDspHost.h"

class AudioEngine final : public juce::AudioIODeviceCallback,
                          private juce::MidiInputCallback
{
public:
    struct Snapshot
    {
        bool transportRunning = false;
        double sampleRate = 0.0;
        int blockSize = 0;
        SourceType sourceType = SourceType::sine;
        float sourceGain = 0.2f;
        float sineFrequency = 440.0f;
        bool wavLoaded = false;
        bool wavLooping = false;
        double wavPosition = 0.0;
        juce::String wavFileName = "No WAV loaded";
        juce::String deviceError;

        PreferredAudioConfiguration preferredAudioConfiguration;
        AudioOverrideState overrides;
        SavedActualAudioConfiguration lastKnownActual;
        double requestedSampleRate = 0.0;
        int requestedBlockSize = 0;
        int requestedInputChannels = 1;
        int requestedOutputChannels = 2;
        juce::String inputDeviceName;
        juce::String outputDeviceName;
        juce::StringArray availableInputDevices;
        juce::StringArray availableOutputDevices;
        juce::String midiInputDeviceName;
        juce::StringArray availableMidiInputDevices;
        juce::String midiInputStatusText;
        juce::String midiLastMessageText;
        int midiLastMessageKind = 0;
        int midiLastMessageChannel = 0;
        int midiLastMessageData1 = -1;
        int midiLastMessageData2 = 0;
        juce::Array<double> availableSampleRates;
        juce::Array<int> availableBlockSizes;
        juce::StringArray inputChannelNames;
        juce::StringArray outputChannelNames;
        std::vector<int> enabledInputChannels;
        std::vector<int> enabledOutputChannels;
        std::array<int, DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS> inputRouting { 0, 1 };
        std::array<int, DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS> outputRouting { 0, 1 };
        juce::String preferredStatusText;
        juce::String requestedStatusText;
        juce::String actualStatusText;
        juce::String overrideStatusText;
        juce::String warningStatusText;
        std::vector<AudioStatusMessage> statusMessages;
    };

    AudioEngine();
    ~AudioEngine() override;

    juce::Result initialise();
    Snapshot getSnapshot() const;
    ProjectAudioState getProjectAudioState() const;
    juce::Result applyProjectAudioState(const ProjectAudioState& state);
    juce::Result setSelectedMidiInputDevice(const juce::String& deviceName);
    void refreshMidiInputs();
    void setProjectControllerDefinitions(const std::vector<UserDspControllerDefinition>& definitions);
    float getPreviewControlValue(int index) const noexcept;
    std::uint32_t getPreviewControlGeneration(int index) const noexcept;

    void startTransport() noexcept;
    void stopTransport() noexcept;

    void setSourceType(SourceType type) noexcept;
    void setSourceGain(float newGain) noexcept;
    void setSineFrequency(float frequency) noexcept;

    juce::Result loadWavFile(const juce::File& file);
    void setWavLooping(bool shouldLoop) noexcept;
    void setWavPositionNormalized(double newPosition) noexcept;

    void setUserControlValue(int index, float value) noexcept;
    void armMidiLearn(int controlIndex, MidiBindingSource source) noexcept;
    void cancelMidiLearn() noexcept;
    bool consumeMidiLearnCapture(MidiLearnCapture& capture) noexcept;

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
    struct ResolvedDeviceState
    {
        ProjectAudioState projectAudioState;
        juce::String inputDeviceName;
        juce::String outputDeviceName;
        juce::StringArray availableInputDevices;
        juce::StringArray availableOutputDevices;
        juce::Array<double> availableSampleRates;
        juce::Array<int> availableBlockSizes;
        juce::StringArray inputChannelNames;
        juce::StringArray outputChannelNames;
        std::vector<int> enabledInputChannels;
        std::vector<int> enabledOutputChannels;
        std::array<int, DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS> resolvedInputSlots { -1, -1 };
        std::array<int, DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS> resolvedOutputSlots { -1, -1 };
        std::vector<AudioStatusMessage> statusMessages;
        double requestedSampleRate = 0.0;
        int requestedBlockSize = 0;
        int requestedInputChannels = 1;
        int requestedOutputChannels = 2;
        double activeSampleRate = 0.0;
        int activeBlockSize = 0;
        int activeInputChannels = 0;
        int activeOutputChannels = 0;
    };

    void requestResetForNextBlock() noexcept;
    void resetSourcesForCurrentSelection() noexcept;
    ResolvedDeviceState buildResolvedDeviceState(const ProjectAudioState& state, bool applySetup);
    std::unique_ptr<juce::AudioIODevice> createProbeDevice(const juce::String& inputDeviceName,
                                                           const juce::String& outputDeviceName) const;
    void publishResolvedDeviceState(const ResolvedDeviceState& resolvedState);
    juce::Result applyMidiInputSelection(const juce::String& requestedDeviceName,
                                         bool persistSelection,
                                         bool fallbackFromUnavailableDevice);
    void persistMidiInputSelection(const juce::String& deviceName);
    void writeMidiBindingState(int index, const MidiBinding& binding, bool routeToRuntime) noexcept;
    void clearMidiBindingState() noexcept;
    void publishPreviewControlValue(int index, float value) noexcept;
    void resetMidiPerformanceState() noexcept;
    void publishMidiPerformanceState() noexcept;
    DspEduMidiState buildCurrentMidiState() const noexcept;
    float normalisePitchWheelValue(int rawPitchWheelValue) const noexcept;
    void routeHardwareInput(const float* const* inputChannelData, int numInputChannels, int logicalInputChannels, int numSamples) noexcept;
    void generateInternalSource(int logicalInputChannels, int numSamples) noexcept;
    void routeProcessedToOutputs(float* const* outputChannelData, int numOutputChannels, int logicalOutputChannels, int numSamples) noexcept;
    void updateStatusTexts(Snapshot& snapshot) const;
    std::vector<int> getSelectedChannelsFromMask(const juce::BigInteger& mask, int maxChannels) const;
    juce::BigInteger createChannelMask(const std::vector<int>& indices, int maxChannels) const;
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;

    juce::AudioDeviceManager deviceManager;
    juce::AudioFormatManager audioFormatManager;
    OscilloscopeBuffer oscilloscopeBuffer;
    UserDspHost userDspHost;
    SineSource sineSource;
    WhiteNoiseSource whiteNoiseSource;
    ImpulseSource impulseSource;
    WavFileSource wavFileSource;

    juce::AudioBuffer<float> sourceBuffer;
    juce::AudioBuffer<float> processedBuffer;

    std::atomic<bool> transportRunning { false };
    std::atomic<SourceType> sourceType { SourceType::sine };
    std::atomic<float> sourceGain { 0.2f };
    std::atomic<double> currentSampleRate { 0.0 };
    std::atomic<int> currentBlockSize { 0 };
    std::atomic<int> currentLogicalInputChannels { 1 };
    std::atomic<int> currentLogicalOutputChannels { 2 };
    std::array<std::atomic<int>, DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS> currentResolvedInputSlots;
    std::array<std::atomic<int>, DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS> currentResolvedOutputSlots;
    std::atomic<bool> resetForNextBlock { true };

    mutable juce::CriticalSection stateLock;
    ProjectAudioState currentProjectAudioState;
    juce::StringArray availableInputDevices;
    juce::StringArray availableOutputDevices;
    juce::Array<double> availableSampleRates;
    juce::Array<int> availableBlockSizes;
    juce::StringArray inputChannelNames;
    juce::StringArray outputChannelNames;
    std::vector<int> enabledInputChannels;
    std::vector<int> enabledOutputChannels;
    std::vector<AudioStatusMessage> currentStatusMessages;
    juce::String currentInputDeviceName;
    juce::String currentOutputDeviceName;
    juce::Array<juce::MidiDeviceInfo> availableMidiInputDevices;
    juce::String currentMidiInputDeviceName;
    juce::String currentMidiInputStatusText;
    std::unique_ptr<juce::MidiInput> midiInputDevice;
    MidiPerformanceState midiPerformanceState;
    double requestedSampleRate = 0.0;
    int requestedBlockSize = 0;
    int requestedInputChannels = 1;
    int requestedOutputChannels = 2;
    juce::String lastDeviceError;

    std::array<std::atomic<int>, DSP_EDU_USER_DSP_MAX_CONTROLS> midiBindingSources;
    std::array<std::atomic<int>, DSP_EDU_USER_DSP_MAX_CONTROLS> midiBindingChannels;
    std::array<std::atomic<int>, DSP_EDU_USER_DSP_MAX_CONTROLS> midiBindingData1;
    std::array<std::atomic<int>, DSP_EDU_USER_DSP_MAX_CONTROLS> midiBindingRoutesToRuntime;
    std::array<std::atomic<float>, DSP_EDU_USER_DSP_MAX_CONTROLS> previewControlValues;
    std::array<std::atomic<std::uint32_t>, DSP_EDU_USER_DSP_MAX_CONTROLS> previewControlGenerations;
    std::atomic<int> lastMidiMessageKind { 0 };
    std::atomic<int> lastMidiMessageChannel { 0 };
    std::atomic<int> lastMidiMessageData1 { -1 };
    std::atomic<int> lastMidiMessageData2 { 0 };
    std::atomic<std::uint32_t> midiPerformanceStateRevision { 0 };
    std::atomic<int> midiStateVoiceCount { 0 };
    std::atomic<int> midiStateGate { 0 };
    std::atomic<int> midiStateChannel { 1 };
    std::atomic<int> midiStateNoteNumber { -1 };
    std::atomic<float> midiStateVelocity { 0.0f };
    std::atomic<float> midiStatePitchWheel { 0.0f };
    std::array<std::atomic<float>, DSP_EDU_USER_DSP_MAX_MIDI_CHANNELS> midiStateChannelPitchWheels;
    std::array<std::atomic<int>, DSP_EDU_USER_DSP_MAX_MIDI_VOICES> midiStateVoiceActive;
    std::array<std::atomic<int>, DSP_EDU_USER_DSP_MAX_MIDI_VOICES> midiStateVoiceChannels;
    std::array<std::atomic<int>, DSP_EDU_USER_DSP_MAX_MIDI_VOICES> midiStateVoiceNotes;
    std::array<std::atomic<float>, DSP_EDU_USER_DSP_MAX_MIDI_VOICES> midiStateVoiceVelocities;
    std::array<std::atomic<std::uint32_t>, DSP_EDU_USER_DSP_MAX_MIDI_VOICES> midiStateVoiceOrders;
    std::atomic<int> armedMidiLearnControlIndex { -1 };
    std::atomic<int> armedMidiLearnSource { static_cast<int>(MidiBindingSource::none) };
    std::atomic<bool> midiLearnCapturePending { false };
    std::atomic<int> midiLearnCaptureControlIndex { -1 };
    std::atomic<int> midiLearnCaptureSource { static_cast<int>(MidiBindingSource::none) };
    std::atomic<int> midiLearnCaptureChannel { 1 };
    std::atomic<int> midiLearnCaptureData1 { -1 };
};
