#pragma once

#include <cstdint>

#if defined(_WIN32)
    #define DSP_EDU_EXPORT extern "C" __declspec(dllexport)
#else
    #define DSP_EDU_EXPORT extern "C"
#endif

static constexpr std::uint32_t DSP_EDU_USER_DSP_API_VERSION = 1u;
static constexpr int DSP_EDU_USER_DSP_MAX_PARAMETERS = 4;
static constexpr int DSP_EDU_USER_DSP_TEXT_CAPACITY = 32;

struct DspEduParameterInfo
{
    char id[DSP_EDU_USER_DSP_TEXT_CAPACITY];
    char name[DSP_EDU_USER_DSP_TEXT_CAPACITY];
    float minValue = 0.0f;
    float maxValue = 1.0f;
    float defaultValue = 0.0f;
};

using DspEduInstanceHandle = void*;

struct DspEduApi
{
    std::uint32_t structSize = sizeof(DspEduApi);
    std::uint32_t apiVersion = DSP_EDU_USER_DSP_API_VERSION;

    DspEduInstanceHandle (*create)() noexcept = nullptr;
    void (*destroy)(DspEduInstanceHandle) noexcept = nullptr;
    const char* (*getProcessorName)(DspEduInstanceHandle) noexcept = nullptr;
    int (*getParameterCount)(DspEduInstanceHandle) noexcept = nullptr;
    int (*getParameterInfo)(DspEduInstanceHandle, int, DspEduParameterInfo*) noexcept = nullptr;
    void (*prepare)(DspEduInstanceHandle, double sampleRate, int maxBlockSize) noexcept = nullptr;
    void (*reset)(DspEduInstanceHandle) noexcept = nullptr;
    void (*process)(DspEduInstanceHandle, const float* input, float* output, int numSamples) noexcept = nullptr;
    void (*setParameter)(DspEduInstanceHandle, int parameterIndex, float value) noexcept = nullptr;
};

DSP_EDU_EXPORT const DspEduApi* dspedu_get_api() noexcept;

#ifdef __cplusplus

#include <algorithm>
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
inline void copyInputToOutput(const float* input, float* output, int numSamples) noexcept
{
    if (output == nullptr || numSamples <= 0)
        return;

    if (input != nullptr && input != output)
    {
        std::copy_n(input, static_cast<std::size_t>(numSamples), output);
        return;
    }

    if (input == nullptr)
        std::fill_n(output, static_cast<std::size_t>(numSamples), 0.0f);
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
    else
    {
        return false;
    }
}

template <typename ProcessorType>
void prepare(ProcessorType& processor, double sampleRate, int maxBlockSize)
{
    if constexpr (requires { processor.prepare(sampleRate, maxBlockSize); })
    {
        processor.prepare(sampleRate, maxBlockSize);
    }
    else if constexpr (requires { processor.prepareToPlay(static_cast<float>(sampleRate), maxBlockSize); })
    {
        processor.prepareToPlay(static_cast<float>(sampleRate), maxBlockSize);
    }
    else if constexpr (requires { processor.prepareToPlay(static_cast<float>(sampleRate)); })
    {
        processor.prepareToPlay(static_cast<float>(sampleRate));
    }
    else if constexpr (requires { processor.init(static_cast<float>(sampleRate)); })
    {
        processor.init(static_cast<float>(sampleRate));
    }
    else
    {
        static_cast<void>(processor);
        static_cast<void>(sampleRate);
        static_cast<void>(maxBlockSize);
    }
}

template <typename ProcessorType>
void reset(ProcessorType& processor)
{
    if constexpr (requires { processor.reset(); })
        processor.reset();
}

template <typename ProcessorType>
void process(ProcessorType& processor, const float* input, float* output, int numSamples)
{
    if constexpr (requires { processor.process(input, output, numSamples); })
    {
        processor.process(input, output, numSamples);
    }
    else if constexpr (requires { processor.processAudio(input, output, numSamples); })
    {
        processor.processAudio(input, output, numSamples);
    }
    else if constexpr (requires { processor.processAudio(output, numSamples); })
    {
        copyInputToOutput(input, output, numSamples);
        processor.processAudio(output, numSamples);
    }
    else
    {
        copyInputToOutput(input, output, numSamples);
    }
}

template <typename ProcessorType>
void setParameter(ProcessorType& processor, int parameterIndex, float value)
{
    if constexpr (requires { processor.setParameter(parameterIndex, value); })
        processor.setParameter(parameterIndex, value);
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

    static void prepare(DspEduInstanceHandle handle, double sampleRate, int maxBlockSize) noexcept
    {
        if (auto* processor = static_cast<ProcessorType*> (handle))
        {
            try
            {
                detail::prepare(*processor, sampleRate, maxBlockSize);
            }
            catch (...)
            {
            }
        }
    }

    static void reset(DspEduInstanceHandle handle) noexcept
    {
        if (auto* processor = static_cast<ProcessorType*> (handle))
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

    static void process(DspEduInstanceHandle handle, const float* input, float* output, int numSamples) noexcept
    {
        if (auto* processor = static_cast<ProcessorType*> (handle))
        {
            try
            {
                detail::process(*processor, input, output, numSamples);
                return;
            }
            catch (...)
            {
            }
        }

        detail::copyInputToOutput(input, output, numSamples);
    }

    static void setParameter(DspEduInstanceHandle handle, int parameterIndex, float value) noexcept
    {
        if (auto* processor = static_cast<ProcessorType*> (handle))
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
    &Adapter<ProcessorType>::getParameterCount,
    &Adapter<ProcessorType>::getParameterInfo,
    &Adapter<ProcessorType>::prepare,
    &Adapter<ProcessorType>::reset,
    &Adapter<ProcessorType>::process,
    &Adapter<ProcessorType>::setParameter
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
