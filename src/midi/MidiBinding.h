#pragma once

#include <JuceHeader.h>

enum class MidiBindingSource
{
    none,
    cc,
    noteGate,
    noteVelocity,
    noteNumber,
    pitchWheel
};

struct MidiBinding
{
    MidiBindingSource source = MidiBindingSource::none;
    int channel = 1;
    int data1 = -1;

    bool operator==(const MidiBinding& other) const noexcept
    {
        return source == other.source
            && channel == other.channel
            && data1 == other.data1;
    }

    bool operator!=(const MidiBinding& other) const noexcept
    {
        return ! (*this == other);
    }
};

struct MidiLearnCapture
{
    int controlIndex = -1;
    MidiBinding binding;
};

namespace midi
{
juce::String midiBindingSourceToStableString(MidiBindingSource source);
juce::String midiBindingSourceToDisplayName(MidiBindingSource source);
MidiBindingSource midiBindingSourceFromStableString(const juce::String& text);

bool isBindingActive(const MidiBinding& binding) noexcept;
juce::Result validateMidiBinding(const MidiBinding& binding);
MidiBinding sanitiseMidiBinding(const MidiBinding& binding) noexcept;
juce::String buildMidiBindingSummary(const MidiBinding& binding);

bool tryCaptureMidiLearnBinding(MidiBindingSource armedSource,
                                const juce::MidiMessage& message,
                                MidiBinding& capturedBinding) noexcept;

bool tryMapMessageToBindingValue(const juce::MidiMessage& message,
                                 const MidiBinding& binding,
                                 float& value) noexcept;
} // namespace midi
