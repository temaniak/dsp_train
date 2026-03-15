#include "midi/MidiPerformanceState.h"

#include <algorithm>

namespace
{
constexpr int minimumMidiChannel = 1;
constexpr int maximumMidiChannel = DSP_EDU_USER_DSP_MAX_MIDI_CHANNELS;
constexpr int minimumMidiNote = 0;
constexpr int maximumMidiNote = 127;
} // namespace

MidiPerformanceState::MidiPerformanceState() noexcept
{
    clear();
}

void MidiPerformanceState::clear() noexcept
{
    state = {};
    state.structSize = sizeof(DspEduMidiState);
    state.channel = 1;
    state.noteNumber = -1;

    for (auto& voice : state.voices)
    {
        voice.active = 0;
        voice.channel = 1;
        voice.noteNumber = -1;
        voice.velocity = 0.0f;
        voice.order = 0;
    }

    nextVoiceOrder = 1;
}

void MidiPerformanceState::noteOn(int channel, int noteNumber, float velocity) noexcept
{
    channel = std::clamp(channel, minimumMidiChannel, maximumMidiChannel);
    noteNumber = std::clamp(noteNumber, minimumMidiNote, maximumMidiNote);
    velocity = std::clamp(velocity, 0.0f, 1.0f);

    auto voiceIndex = findVoiceIndex(channel, noteNumber);

    if (voiceIndex < 0)
        voiceIndex = findAvailableVoiceIndex();

    auto& voice = state.voices[static_cast<std::size_t>(voiceIndex)];
    voice.active = 1;
    voice.channel = channel;
    voice.noteNumber = noteNumber;
    voice.velocity = velocity;
    voice.order = nextVoiceOrder++;

    refreshVoiceCount();

    state.gate = 1;
    state.channel = channel;
    state.noteNumber = noteNumber;
    state.velocity = velocity;
    state.pitchWheel = state.channelPitchWheel[static_cast<std::size_t>(channel - 1)];
}

void MidiPerformanceState::noteOff(int channel, int noteNumber) noexcept
{
    channel = std::clamp(channel, minimumMidiChannel, maximumMidiChannel);
    noteNumber = std::clamp(noteNumber, minimumMidiNote, maximumMidiNote);

    if (const auto voiceIndex = findVoiceIndex(channel, noteNumber); voiceIndex >= 0)
    {
        auto& voice = state.voices[static_cast<std::size_t>(voiceIndex)];
        voice.active = 0;
        voice.channel = channel;
        voice.noteNumber = noteNumber;
        voice.velocity = 0.0f;
        voice.order = 0;
    }

    refreshVoiceCount();

    if (state.gate != 0 && state.channel == channel && state.noteNumber == noteNumber)
    {
        state.gate = 0;
        state.velocity = 0.0f;
        state.pitchWheel = state.channelPitchWheel[static_cast<std::size_t>(channel - 1)];
    }
}

void MidiPerformanceState::setPitchWheel(int channel, float pitchWheel) noexcept
{
    channel = std::clamp(channel, minimumMidiChannel, maximumMidiChannel);
    pitchWheel = std::clamp(pitchWheel, -1.0f, 1.0f);
    state.channelPitchWheel[static_cast<std::size_t>(channel - 1)] = pitchWheel;
    state.pitchWheel = pitchWheel;
}

const DspEduMidiState& MidiPerformanceState::getState() const noexcept
{
    return state;
}

void MidiPerformanceState::refreshVoiceCount() noexcept
{
    int activeVoices = 0;

    for (const auto& voice : state.voices)
        activeVoices += voice.active != 0 ? 1 : 0;

    state.voiceCount = activeVoices;
}

int MidiPerformanceState::findVoiceIndex(int channel, int noteNumber) const noexcept
{
    for (int index = 0; index < DSP_EDU_USER_DSP_MAX_MIDI_VOICES; ++index)
    {
        const auto& voice = state.voices[static_cast<std::size_t>(index)];

        if (voice.active != 0 && voice.channel == channel && voice.noteNumber == noteNumber)
            return index;
    }

    return -1;
}

int MidiPerformanceState::findAvailableVoiceIndex() const noexcept
{
    for (int index = 0; index < DSP_EDU_USER_DSP_MAX_MIDI_VOICES; ++index)
        if (state.voices[static_cast<std::size_t>(index)].active == 0)
            return index;

    int oldestIndex = 0;
    auto oldestOrder = state.voices[0].order;

    for (int index = 1; index < DSP_EDU_USER_DSP_MAX_MIDI_VOICES; ++index)
    {
        const auto order = state.voices[static_cast<std::size_t>(index)].order;

        if (order < oldestOrder)
        {
            oldestOrder = order;
            oldestIndex = index;
        }
    }

    return oldestIndex;
}
