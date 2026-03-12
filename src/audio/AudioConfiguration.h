#pragma once

#include <JuceHeader.h>

#include "UserDspApi.h"

#include <array>
#include <cmath>
#include <vector>

namespace audio_config_detail
{
inline bool approximatelyEqual(double lhs, double rhs, double epsilon = 1.0e-6) noexcept
{
    return std::abs(lhs - rhs) <= epsilon;
}
}

enum class AudioStatusCode
{
    preferredValueProvided,
    preferredValueApplied,
    preferredValueUnsupported,
    activeValueAdjustedByDevice,
    valueManuallyOverridden,
    projectReopenedWithDifferentHardware,
    inputRoutingReduced,
    outputRoutingReduced,
    stereoRequestedButUnavailable,
    monoExpandedToStereo,
    stereoDownmixedToMono
};

enum class AudioStatusSeverity
{
    info,
    warning,
    error
};

struct AudioStatusMessage
{
    AudioStatusCode code = AudioStatusCode::preferredValueProvided;
    AudioStatusSeverity severity = AudioStatusSeverity::info;
    juce::String summary;
    juce::String details;
};

struct PreferredAudioConfiguration
{
    bool valid = false;
    double sampleRate = 0.0;
    int blockSize = 0;
    int preferredInputChannels = 1;
    int preferredOutputChannels = 2;

    bool operator==(const PreferredAudioConfiguration& other) const noexcept
    {
        return valid == other.valid
            && audio_config_detail::approximatelyEqual(sampleRate, other.sampleRate)
            && blockSize == other.blockSize
            && preferredInputChannels == other.preferredInputChannels
            && preferredOutputChannels == other.preferredOutputChannels;
    }

    bool operator!=(const PreferredAudioConfiguration& other) const noexcept
    {
        return ! (*this == other);
    }
};

struct AudioOverrideState
{
    bool sampleRateOverridden = false;
    bool blockSizeOverridden = false;
    bool inputDeviceOverridden = false;
    bool outputDeviceOverridden = false;
    bool inputChannelsOverridden = false;
    bool outputChannelsOverridden = false;
    bool routingOverridden = false;
    double sampleRate = 0.0;
    int blockSize = 0;

    bool operator==(const AudioOverrideState& other) const noexcept
    {
        return sampleRateOverridden == other.sampleRateOverridden
            && blockSizeOverridden == other.blockSizeOverridden
            && inputDeviceOverridden == other.inputDeviceOverridden
            && outputDeviceOverridden == other.outputDeviceOverridden
            && inputChannelsOverridden == other.inputChannelsOverridden
            && outputChannelsOverridden == other.outputChannelsOverridden
            && routingOverridden == other.routingOverridden
            && audio_config_detail::approximatelyEqual(sampleRate, other.sampleRate)
            && blockSize == other.blockSize;
    }

    bool operator!=(const AudioOverrideState& other) const noexcept
    {
        return ! (*this == other);
    }
};

struct AudioDeviceSelectionState
{
    juce::String inputDeviceName;
    juce::String outputDeviceName;
    std::vector<int> enabledInputChannels;
    std::vector<int> enabledOutputChannels;
    std::array<int, DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS> inputRouting { 0, 1 };
    std::array<int, DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS> outputRouting { 0, 1 };

    bool operator==(const AudioDeviceSelectionState& other) const noexcept
    {
        return inputDeviceName == other.inputDeviceName
            && outputDeviceName == other.outputDeviceName
            && enabledInputChannels == other.enabledInputChannels
            && enabledOutputChannels == other.enabledOutputChannels
            && inputRouting == other.inputRouting
            && outputRouting == other.outputRouting;
    }

    bool operator!=(const AudioDeviceSelectionState& other) const noexcept
    {
        return ! (*this == other);
    }
};

struct SavedActualAudioConfiguration
{
    juce::String inputDeviceName;
    juce::String outputDeviceName;
    double sampleRate = 0.0;
    int blockSize = 0;
    int activeInputChannels = 0;
    int activeOutputChannels = 0;

    bool operator==(const SavedActualAudioConfiguration& other) const noexcept
    {
        return inputDeviceName == other.inputDeviceName
            && outputDeviceName == other.outputDeviceName
            && audio_config_detail::approximatelyEqual(sampleRate, other.sampleRate)
            && blockSize == other.blockSize
            && activeInputChannels == other.activeInputChannels
            && activeOutputChannels == other.activeOutputChannels;
    }

    bool operator!=(const SavedActualAudioConfiguration& other) const noexcept
    {
        return ! (*this == other);
    }
};

struct ProjectAudioState
{
    PreferredAudioConfiguration cachedPreferred;
    AudioOverrideState overrides;
    AudioDeviceSelectionState deviceSelection;
    SavedActualAudioConfiguration lastKnownActual;

    bool operator==(const ProjectAudioState& other) const noexcept
    {
        return cachedPreferred == other.cachedPreferred
            && overrides == other.overrides
            && deviceSelection == other.deviceSelection
            && lastKnownActual == other.lastKnownActual;
    }

    bool operator!=(const ProjectAudioState& other) const noexcept
    {
        return ! (*this == other);
    }
};
