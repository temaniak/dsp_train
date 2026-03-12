#pragma once

#include <JuceHeader.h>

#include "UserDspApi.h"

class UserDspHost
{
public:
    struct ParameterState
    {
        bool active = false;
        juce::String id;
        juce::String name;
        float minValue = 0.0f;
        float maxValue = 1.0f;
        float defaultValue = 0.0f;
        float currentValue = 0.0f;
    };

    struct Snapshot
    {
        bool hasActiveModule = false;
        juce::String processorName = "No user DSP loaded";
        juce::String activeModulePath;
        juce::String lastError;
        int parameterCount = 0;
        std::array<ParameterState, DSP_EDU_USER_DSP_MAX_PARAMETERS> parameters;
    };

    UserDspHost();
    ~UserDspHost();

    void prepare(double sampleRate, int maxBlockSize);
    void requestReset() noexcept;
    void process(const float* input, float* output, int numSamples);

    void setParameterValue(int index, float value) noexcept;
    float getParameterValue(int index) const noexcept;

    juce::Result loadModuleFromFile(const juce::File& moduleFile);
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

    std::array<std::atomic<float>, DSP_EDU_USER_DSP_MAX_PARAMETERS> parameterValues;
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
