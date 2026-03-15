#pragma once

#include <JuceHeader.h>

#include "audio/AudioConfiguration.h"
#include "UserDspApi.h"
#include "userdsp/UserDspControllers.h"

class UserDspHost
{
public:
    struct ControlState
    {
        bool active = false;
        UserDspControllerType type = UserDspControllerType::knob;
        juce::String label;
        juce::String codeName;
        float currentValue = 0.0f;
    };

    struct Snapshot
    {
        bool hasActiveModule = false;
        juce::String processorName = "No user DSP loaded";
        juce::String activeModulePath;
        juce::String lastError;
        int moduleGeneration = 0;
        PreferredAudioConfiguration preferredAudioConfiguration;
        double preparedSampleRate = 0.0;
        int preparedBlockSize = 0;
        int preparedInputChannels = 0;
        int preparedOutputChannels = 0;
        int controlCount = 0;
        std::array<ControlState, DSP_EDU_USER_DSP_MAX_CONTROLS> controls;
    };

    UserDspHost();
    ~UserDspHost();

    void prepare(double sampleRate, int maxBlockSize, int inputChannels, int outputChannels);
    void requestReset() noexcept;
    void process(const float* const* inputs,
                 float* const* outputs,
                 int numInputChannels,
                 int numOutputChannels,
                 int numSamples);

    void setMidiInputState(const DspEduMidiState& midiState) noexcept;
    void setControlValue(int index, float value) noexcept;
    float getControlValue(int index) const noexcept;

    juce::Result loadModuleFromFile(const juce::File& moduleFile,
                                    const std::vector<UserDspControllerDefinition>& controllerDefinitions);
    void reclaimRetiredRuntimes();
    Snapshot getSnapshot() const;

private:
    struct Runtime;

    void commitPendingRuntimeSwap() noexcept;
    void enqueueRetiredRuntime(Runtime* runtime) noexcept;
    void clearSnapshotError();
    void setSnapshotError(const juce::String& errorText);

    mutable juce::CriticalSection snapshotLock;
    Snapshot snapshot;
    DspEduMidiState currentMidiState;

    std::array<std::atomic<float>, DSP_EDU_USER_DSP_MAX_CONTROLS> controlValues;
    std::atomic<double> currentSampleRate { 44100.0 };
    std::atomic<int> currentMaxBlockSize { 512 };
    std::atomic<int> currentInputChannels { 0 };
    std::atomic<int> currentOutputChannels { 2 };
    std::atomic<bool> resetRequested { false };
    std::atomic<bool> swapPending { false };

    juce::SpinLock pendingRuntimeLock;
    std::unique_ptr<Runtime> activeRuntime;
    std::unique_ptr<Runtime> pendingRuntime;

    juce::AbstractFifo retiredRuntimeFifo { 32 };
    std::array<Runtime*, 32> retiredRuntimeSlots {};
};
