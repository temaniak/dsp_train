#pragma once

#include <JuceHeader.h>

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
        int controlCount = 0;
        std::array<ControlState, DSP_EDU_USER_DSP_MAX_CONTROLS> controls;
    };

    UserDspHost();
    ~UserDspHost();

    void prepare(double sampleRate, int maxBlockSize);
    void requestReset() noexcept;
    void process(const float* input, float* output, int numSamples);

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

    std::array<std::atomic<float>, DSP_EDU_USER_DSP_MAX_CONTROLS> controlValues;
    std::atomic<double> currentSampleRate { 44100.0 };
    std::atomic<int> currentMaxBlockSize { 512 };
    std::atomic<bool> resetRequested { false };
    std::atomic<bool> swapPending { false };

    juce::SpinLock pendingRuntimeLock;
    std::unique_ptr<Runtime> activeRuntime;
    std::unique_ptr<Runtime> pendingRuntime;

    juce::AbstractFifo retiredRuntimeFifo { 32 };
    std::array<Runtime*, 32> retiredRuntimeSlots {};
};
