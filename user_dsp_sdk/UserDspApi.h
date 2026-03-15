#pragma once

#include <cstdint>

#if defined(_WIN32)
    #define DSP_EDU_EXPORT extern "C" __declspec(dllexport)
#else
    #define DSP_EDU_EXPORT extern "C"
#endif

static constexpr std::uint32_t DSP_EDU_USER_DSP_API_VERSION = 4u;
static constexpr int DSP_EDU_USER_DSP_MAX_CONTROLS = 32;
static constexpr int DSP_EDU_USER_DSP_MAX_PARAMETERS = DSP_EDU_USER_DSP_MAX_CONTROLS;
static constexpr int DSP_EDU_USER_DSP_TEXT_CAPACITY = 32;
static constexpr int DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS = 2;
static constexpr int DSP_EDU_USER_DSP_MAX_MIDI_VOICES = 16;
static constexpr int DSP_EDU_USER_DSP_MAX_MIDI_CHANNELS = 16;

struct DspEduParameterInfo
{
    char id[DSP_EDU_USER_DSP_TEXT_CAPACITY];
    char name[DSP_EDU_USER_DSP_TEXT_CAPACITY];
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float defaultValue = 0.0f;
};

struct DspEduPreferredAudioConfig
{
    std::uint32_t structSize = sizeof(DspEduPreferredAudioConfig);
    double preferredSampleRate = 0.0;
    int preferredBlockSize = 0;
    int preferredInputChannels = 0;
    int preferredOutputChannels = 0;
};

struct DspEduProcessSpec
{
    std::uint32_t structSize = sizeof(DspEduProcessSpec);
    double sampleRate = 44100.0;
    int maximumBlockSize = 512;
    int activeInputChannels = 0;
    int activeOutputChannels = 0;
};

struct DspEduMidiVoice
{
    int active = 0;
    int channel = 1;
    int noteNumber = -1;
    float velocity = 0.0f;
    std::uint32_t order = 0;
};

struct DspEduMidiState
{
    std::uint32_t structSize = sizeof(DspEduMidiState);
    int voiceCount = 0;
    int gate = 0;
    int channel = 1;
    int noteNumber = -1;
    float velocity = 0.0f;
    float pitchWheel = 0.0f;
    float channelPitchWheel[DSP_EDU_USER_DSP_MAX_MIDI_CHANNELS] {};
    DspEduMidiVoice voices[DSP_EDU_USER_DSP_MAX_MIDI_VOICES] {};
};

using DspEduInstanceHandle = void*;

struct DspEduApi
{
    std::uint32_t structSize = sizeof(DspEduApi);
    std::uint32_t apiVersion = DSP_EDU_USER_DSP_API_VERSION;

    DspEduInstanceHandle (*create)() noexcept = nullptr;
    void (*destroy)(DspEduInstanceHandle) noexcept = nullptr;
    const char* (*getProcessorName)(DspEduInstanceHandle) noexcept = nullptr;
    int (*getPreferredAudioConfig)(DspEduInstanceHandle, DspEduPreferredAudioConfig*) noexcept = nullptr;
    int (*getParameterCount)(DspEduInstanceHandle) noexcept = nullptr;
    int (*getParameterInfo)(DspEduInstanceHandle, int, DspEduParameterInfo*) noexcept = nullptr;
    void (*prepare)(DspEduInstanceHandle, const DspEduProcessSpec*) noexcept = nullptr;
    void (*reset)(DspEduInstanceHandle) noexcept = nullptr;
    void (*process)(DspEduInstanceHandle,
                    const float* const* inputs,
                    float* const* outputs,
                    int numInputChannels,
                    int numOutputChannels,
                    int numSamples) noexcept = nullptr;
    void (*setParameter)(DspEduInstanceHandle, int parameterIndex, float value) noexcept = nullptr;
    void (*setControlValue)(DspEduInstanceHandle, int controlIndex, float value) noexcept = nullptr;
    void (*setMidiState)(DspEduInstanceHandle, const DspEduMidiState*) noexcept = nullptr;
};

DSP_EDU_EXPORT const DspEduApi* dspedu_get_api() noexcept;

#ifdef __cplusplus

#include <algorithm>
#include <array>
#include <concepts>
#include <cstring>
#include <new>

namespace dspedu
{
inline void copyText(char* destination, const char* source) noexcept
{
    if (destination == nullptr)
        return;

    std::memset(destination, 0, static_cast<std::size_t>(DSP_EDU_USER_DSP_TEXT_CAPACITY));

    if (source == nullptr)
        return;

#if defined(_MSC_VER)
    strncpy_s(destination,
              static_cast<std::size_t>(DSP_EDU_USER_DSP_TEXT_CAPACITY),
              source,
              static_cast<std::size_t>(DSP_EDU_USER_DSP_TEXT_CAPACITY - 1));
#else
    std::strncpy(destination, source, static_cast<std::size_t>(DSP_EDU_USER_DSP_TEXT_CAPACITY - 1));
#endif
}

namespace detail
{
inline int clampAudioChannelCount(int channelCount) noexcept
{
    return std::clamp(channelCount, 0, DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS);
}

inline void clearOutputs(float* const* outputs, int numOutputChannels, int numSamples) noexcept
{
    if (outputs == nullptr || numSamples <= 0)
        return;

    for (int channel = 0; channel < numOutputChannels; ++channel)
    {
        if (auto* output = outputs[channel]; output != nullptr)
            std::fill_n(output, static_cast<std::size_t>(numSamples), 0.0f);
    }
}

inline void copyInputToOutput(const float* const* inputs,
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
            std::fill_n(output, static_cast<std::size_t>(numSamples), 0.0f);
            continue;
        }

        const auto sourceChannel = std::clamp(channel, 0, numInputChannels - 1);
        const auto* input = inputs[sourceChannel];

        if (input == nullptr)
        {
            std::fill_n(output, static_cast<std::size_t>(numSamples), 0.0f);
            continue;
        }

        std::copy_n(input, static_cast<std::size_t>(numSamples), output);
    }
}

template <typename ProcessorType>
const char* getProcessorName(ProcessorType& processor) noexcept
{
    if constexpr (requires { { processor.getName() } -> std::convertible_to<const char*>; })
    {
        return processor.getName();
    }
    else
    {
        return "User DSP";
    }
}

template <typename ProcessorType>
bool getPreferredAudioConfig(ProcessorType& processor, DspEduPreferredAudioConfig& config) noexcept
{
    if constexpr (requires { { processor.getPreferredAudioConfig(config) } -> std::convertible_to<bool>; })
    {
        return processor.getPreferredAudioConfig(config);
    }
    else if constexpr (requires { processor.getPreferredAudioConfig(config); })
    {
        processor.getPreferredAudioConfig(config);
        return true;
    }
    else
    {
        bool hasPreferredValue = false;

        if constexpr (requires { { processor.getPreferredSampleRate() } -> std::convertible_to<double>; })
        {
            config.preferredSampleRate = processor.getPreferredSampleRate();
            hasPreferredValue = true;
        }

        if constexpr (requires { { processor.getPreferredBlockSize() } -> std::convertible_to<int>; })
        {
            config.preferredBlockSize = processor.getPreferredBlockSize();
            hasPreferredValue = true;
        }

        if constexpr (requires { { processor.getPreferredInputChannels() } -> std::convertible_to<int>; })
        {
            config.preferredInputChannels = processor.getPreferredInputChannels();
            hasPreferredValue = true;
        }

        if constexpr (requires { { processor.getPreferredOutputChannels() } -> std::convertible_to<int>; })
        {
            config.preferredOutputChannels = processor.getPreferredOutputChannels();
            hasPreferredValue = true;
        }

        return hasPreferredValue;
    }
}

template <typename ProcessorType>
int getParameterCount(ProcessorType& processor) noexcept
{
    if constexpr (requires { { processor.getParameterCount() } -> std::convertible_to<int>; })
    {
        return std::clamp(static_cast<int>(processor.getParameterCount()), 0, DSP_EDU_USER_DSP_MAX_PARAMETERS);
    }
    else
    {
        return 0;
    }
}

template <typename ProcessorType>
bool getParameterInfo(ProcessorType& processor, int parameterIndex, DspEduParameterInfo& info) noexcept
{
    if constexpr (requires { { processor.getParameterInfo(parameterIndex, info) } -> std::convertible_to<bool>; })
    {
        return processor.getParameterInfo(parameterIndex, info);
    }
    else if constexpr (requires { processor.getParameterInfo(parameterIndex, info); })
    {
        processor.getParameterInfo(parameterIndex, info);
        return true;
    }
    else
    {
        return false;
    }
}

template <typename ProcessorType>
void prepare(ProcessorType& processor, const DspEduProcessSpec& spec)
{
    if constexpr (requires { processor.prepare(spec); })
    {
        processor.prepare(spec);
    }
    else if constexpr (requires { processor.prepare(spec.sampleRate, spec.maximumBlockSize, spec.activeInputChannels, spec.activeOutputChannels); })
    {
        processor.prepare(spec.sampleRate, spec.maximumBlockSize, spec.activeInputChannels, spec.activeOutputChannels);
    }
    else if constexpr (requires { processor.prepare(spec.sampleRate, spec.maximumBlockSize); })
    {
        processor.prepare(spec.sampleRate, spec.maximumBlockSize);
    }
    else if constexpr (requires { processor.prepareToPlay(spec.sampleRate, spec.maximumBlockSize); })
    {
        processor.prepareToPlay(spec.sampleRate, spec.maximumBlockSize);
    }
    else if constexpr (requires { processor.prepareToPlay(spec.sampleRate); })
    {
        processor.prepareToPlay(spec.sampleRate);
    }
    else if constexpr (requires { processor.init(static_cast<float>(spec.sampleRate)); })
    {
        processor.init(static_cast<float>(spec.sampleRate));
    }
    else
    {
        static_cast<void>(processor);
        static_cast<void>(spec);
    }
}

template <typename ProcessorType>
void reset(ProcessorType& processor)
{
    if constexpr (requires { processor.reset(); })
        processor.reset();
}

template <typename ProcessorType>
void process(ProcessorType& processor,
             const float* const* inputs,
             float* const* outputs,
             int numInputChannels,
             int numOutputChannels,
             int numSamples)
{
    if constexpr (requires { processor.process(inputs, outputs, numInputChannels, numOutputChannels, numSamples); })
    {
        processor.process(inputs, outputs, numInputChannels, numOutputChannels, numSamples);
        return;
    }
    else if constexpr (requires { processor.processAudio(inputs, outputs, numInputChannels, numOutputChannels, numSamples); })
    {
        processor.processAudio(inputs, outputs, numInputChannels, numOutputChannels, numSamples);
        return;
    }
    else if constexpr (requires { processor.process(outputs, numOutputChannels, numSamples); })
    {
        copyInputToOutput(inputs, outputs, numInputChannels, numOutputChannels, numSamples);
        processor.process(outputs, numOutputChannels, numSamples);
        return;
    }
    else if constexpr (requires { processor.processAudio(outputs, numOutputChannels, numSamples); })
    {
        copyInputToOutput(inputs, outputs, numInputChannels, numOutputChannels, numSamples);
        processor.processAudio(outputs, numOutputChannels, numSamples);
        return;
    }

    const auto* input = inputs != nullptr && numInputChannels > 0 ? inputs[0] : nullptr;
    auto* output = outputs != nullptr && numOutputChannels > 0 ? outputs[0] : nullptr;

    if (output == nullptr)
        return;

    if constexpr (requires { processor.process(input, output, numSamples); })
    {
        processor.process(input, output, numSamples);
    }
    else if constexpr (requires { processor.processAudio(input, output, numSamples); })
    {
        processor.processAudio(input, output, numSamples);
    }
    else if constexpr (requires { processor.process(output, numSamples); })
    {
        copyInputToOutput(inputs, outputs, numInputChannels, numOutputChannels, numSamples);
        processor.process(output, numSamples);
    }
    else if constexpr (requires { processor.processAudio(output, numSamples); })
    {
        copyInputToOutput(inputs, outputs, numInputChannels, numOutputChannels, numSamples);
        processor.processAudio(output, numSamples);
    }
    else
    {
        copyInputToOutput(inputs, outputs, numInputChannels, numOutputChannels, numSamples);
    }

    for (int channel = 1; channel < numOutputChannels; ++channel)
    {
        if (outputs[channel] != nullptr)
            std::copy_n(output, static_cast<std::size_t>(numSamples), outputs[channel]);
    }
}

template <typename ProcessorType>
void setParameter(ProcessorType& processor, int parameterIndex, float value)
{
    if constexpr (requires { processor.setParameter(parameterIndex, value); })
        processor.setParameter(parameterIndex, value);
}

template <typename ProcessorType>
void setControlValue(ProcessorType& processor, int controlIndex, float value)
{
    if constexpr (requires { processor.setControlValue(controlIndex, value); })
        processor.setControlValue(controlIndex, value);
    else if constexpr (requires { processor.setParameter(controlIndex, value); })
        processor.setParameter(controlIndex, value);
}

template <typename ProcessorType>
void setMidiState(ProcessorType& processor, const DspEduMidiState& state)
{
    if constexpr (requires { processor.setMidiState(state); })
        processor.setMidiState(state);
}
} // namespace detail

template <typename ProcessorType>
struct Adapter
{
    static DspEduInstanceHandle create() noexcept
    {
        try
        {
            return new (std::nothrow) ProcessorType();
        }
        catch (...)
        {
            return nullptr;
        }
    }

    static void destroy(DspEduInstanceHandle handle) noexcept
    {
        try
        {
            delete static_cast<ProcessorType*> (handle);
        }
        catch (...)
        {
        }
    }

    static const char* getProcessorName(DspEduInstanceHandle handle) noexcept
    {
        auto* processor = static_cast<ProcessorType*> (handle);

        if (processor == nullptr)
            return "Invalid DSP";

        try
        {
            return detail::getProcessorName(*processor);
        }
        catch (...)
        {
            return "Invalid DSP";
        }
    }

    static int getPreferredAudioConfig(DspEduInstanceHandle handle, DspEduPreferredAudioConfig* config) noexcept
    {
        auto* processor = static_cast<ProcessorType*> (handle);

        if (processor == nullptr || config == nullptr)
            return 0;

        auto localConfig = *config;
        localConfig.structSize = sizeof(DspEduPreferredAudioConfig);

        try
        {
            if (! detail::getPreferredAudioConfig(*processor, localConfig))
                return 0;
        }
        catch (...)
        {
            return 0;
        }

        localConfig.preferredBlockSize = std::max(localConfig.preferredBlockSize, 0);
        localConfig.preferredInputChannels = detail::clampAudioChannelCount(localConfig.preferredInputChannels);
        localConfig.preferredOutputChannels = detail::clampAudioChannelCount(localConfig.preferredOutputChannels);
        *config = localConfig;
        return 1;
    }

    static int getParameterCount(DspEduInstanceHandle handle) noexcept
    {
        auto* processor = static_cast<ProcessorType*> (handle);
        if (processor == nullptr)
            return 0;

        try
        {
            return detail::getParameterCount(*processor);
        }
        catch (...)
        {
            return 0;
        }
    }

    static int getParameterInfo(DspEduInstanceHandle handle, int parameterIndex, DspEduParameterInfo* info) noexcept
    {
        auto* processor = static_cast<ProcessorType*> (handle);

        if (processor == nullptr || info == nullptr)
            return 0;

        DspEduParameterInfo localInfo {};

        try
        {
            if (! detail::getParameterInfo(*processor, parameterIndex, localInfo))
                return 0;
        }
        catch (...)
        {
            return 0;
        }

        *info = localInfo;
        return 1;
    }

    static void prepare(DspEduInstanceHandle handle, const DspEduProcessSpec* spec) noexcept
    {
        if (auto* processor = static_cast<ProcessorType*> (handle); processor != nullptr)
        {
            DspEduProcessSpec localSpec {};

            if (spec != nullptr)
                localSpec = *spec;

            localSpec.structSize = sizeof(DspEduProcessSpec);
            localSpec.maximumBlockSize = std::max(localSpec.maximumBlockSize, 1);
            localSpec.activeInputChannels = detail::clampAudioChannelCount(localSpec.activeInputChannels);
            localSpec.activeOutputChannels = detail::clampAudioChannelCount(localSpec.activeOutputChannels);

            try
            {
                detail::prepare(*processor, localSpec);
            }
            catch (...)
            {
            }
        }
    }

    static void reset(DspEduInstanceHandle handle) noexcept
    {
        if (auto* processor = static_cast<ProcessorType*> (handle); processor != nullptr)
        {
            try
            {
                detail::reset(*processor);
            }
            catch (...)
            {
            }
        }
    }

    static void process(DspEduInstanceHandle handle,
                        const float* const* inputs,
                        float* const* outputs,
                        int numInputChannels,
                        int numOutputChannels,
                        int numSamples) noexcept
    {
        if (numSamples <= 0)
            return;

        numInputChannels = detail::clampAudioChannelCount(numInputChannels);
        numOutputChannels = detail::clampAudioChannelCount(numOutputChannels);

        if (auto* processor = static_cast<ProcessorType*> (handle); processor != nullptr)
        {
            try
            {
                detail::process(*processor, inputs, outputs, numInputChannels, numOutputChannels, numSamples);
                return;
            }
            catch (...)
            {
            }
        }

        detail::copyInputToOutput(inputs, outputs, numInputChannels, numOutputChannels, numSamples);
    }

    static void setParameter(DspEduInstanceHandle handle, int parameterIndex, float value) noexcept
    {
        if (auto* processor = static_cast<ProcessorType*> (handle); processor != nullptr)
        {
            try
            {
                detail::setParameter(*processor, parameterIndex, value);
            }
            catch (...)
            {
            }
        }
    }

    static void setControlValue(DspEduInstanceHandle handle, int controlIndex, float value) noexcept
    {
        if (auto* processor = static_cast<ProcessorType*> (handle); processor != nullptr)
        {
            try
            {
                detail::setControlValue(*processor, controlIndex, value);
            }
            catch (...)
            {
            }
        }
    }

    static void setMidiState(DspEduInstanceHandle handle, const DspEduMidiState* state) noexcept
    {
        if (auto* processor = static_cast<ProcessorType*> (handle); processor != nullptr && state != nullptr)
        {
            auto localState = *state;
            localState.structSize = sizeof(DspEduMidiState);

            try
            {
                detail::setMidiState(*processor, localState);
            }
            catch (...)
            {
            }
        }
    }

    static const DspEduApi api;
};

template <typename ProcessorType>
const DspEduApi Adapter<ProcessorType>::api
{
    sizeof(DspEduApi),
    DSP_EDU_USER_DSP_API_VERSION,
    &Adapter<ProcessorType>::create,
    &Adapter<ProcessorType>::destroy,
    &Adapter<ProcessorType>::getProcessorName,
    &Adapter<ProcessorType>::getPreferredAudioConfig,
    &Adapter<ProcessorType>::getParameterCount,
    &Adapter<ProcessorType>::getParameterInfo,
    &Adapter<ProcessorType>::prepare,
    &Adapter<ProcessorType>::reset,
    &Adapter<ProcessorType>::process,
    &Adapter<ProcessorType>::setParameter,
    &Adapter<ProcessorType>::setControlValue,
    &Adapter<ProcessorType>::setMidiState
};
} // namespace dspedu

#define DSP_EDU_DEFINE_PLUGIN(ProcessorType)          \
    DSP_EDU_EXPORT const DspEduApi* dspedu_get_api() noexcept \
    {                                                 \
        return &::dspedu::Adapter<ProcessorType>::api; \
    }

#define DSP_EDU_DEFINE_SIMPLE_PLUGIN(ProcessorType) \
    DSP_EDU_DEFINE_PLUGIN(ProcessorType)

#endif
