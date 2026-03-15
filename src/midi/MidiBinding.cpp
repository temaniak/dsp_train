#include "midi/MidiBinding.h"

namespace
{
constexpr int midiMinimumChannel = 1;
constexpr int midiMaximumChannel = 16;
constexpr int midiMinimumData = 0;
constexpr int midiMaximumData = 127;
constexpr int midiPitchWheelMinimum = 0;
constexpr int midiPitchWheelMaximum = 16383;

bool isNoteBindingSource(MidiBindingSource source) noexcept
{
    return source == MidiBindingSource::noteGate
        || source == MidiBindingSource::noteVelocity
        || source == MidiBindingSource::noteNumber;
}

bool isSupportedLearnMessage(MidiBindingSource source, const juce::MidiMessage& message) noexcept
{
    switch (source)
    {
        case MidiBindingSource::cc:
            return message.isController();

        case MidiBindingSource::noteGate:
        case MidiBindingSource::noteVelocity:
        case MidiBindingSource::noteNumber:
            return message.isNoteOn();

        case MidiBindingSource::pitchWheel:
            return message.isPitchWheel();

        case MidiBindingSource::none:
            break;
    }

    return false;
}
} // namespace

namespace midi
{
juce::String midiBindingSourceToStableString(MidiBindingSource source)
{
    switch (source)
    {
        case MidiBindingSource::cc:           return "cc";
        case MidiBindingSource::noteGate:     return "noteGate";
        case MidiBindingSource::noteVelocity: return "noteVelocity";
        case MidiBindingSource::noteNumber:   return "noteNumber";
        case MidiBindingSource::pitchWheel:   return "pitchWheel";
        case MidiBindingSource::none:         break;
    }

    return "none";
}

juce::String midiBindingSourceToDisplayName(MidiBindingSource source)
{
    switch (source)
    {
        case MidiBindingSource::cc:           return "CC";
        case MidiBindingSource::noteGate:     return "Note Gate";
        case MidiBindingSource::noteVelocity: return "Note Velocity";
        case MidiBindingSource::noteNumber:   return "Note Number";
        case MidiBindingSource::pitchWheel:   return "Pitch Wheel";
        case MidiBindingSource::none:         break;
    }

    return "None";
}

MidiBindingSource midiBindingSourceFromStableString(const juce::String& text)
{
    const auto trimmed = text.trim().toLowerCase();

    if (trimmed == "cc")
        return MidiBindingSource::cc;

    if (trimmed == "notegate")
        return MidiBindingSource::noteGate;

    if (trimmed == "notevelocity")
        return MidiBindingSource::noteVelocity;

    if (trimmed == "notenumber")
        return MidiBindingSource::noteNumber;

    if (trimmed == "pitchwheel")
        return MidiBindingSource::pitchWheel;

    return MidiBindingSource::none;
}

bool isBindingActive(const MidiBinding& binding) noexcept
{
    return binding.source != MidiBindingSource::none;
}

juce::Result validateMidiBinding(const MidiBinding& binding)
{
    if (! isBindingActive(binding))
        return juce::Result::ok();

    if (! juce::isPositiveAndBelow(binding.channel - 1, midiMaximumChannel))
        return juce::Result::fail("MIDI binding channel must be between 1 and 16.");

    if (binding.source == MidiBindingSource::pitchWheel)
    {
        if (binding.data1 != -1)
            return juce::Result::fail("Pitch wheel bindings must not store note or CC data.");

        return juce::Result::ok();
    }

    if (! juce::isPositiveAndBelow(binding.data1, midiMaximumData + 1))
        return juce::Result::fail("MIDI binding note or CC number must be between 0 and 127.");

    return juce::Result::ok();
}

MidiBinding sanitiseMidiBinding(const MidiBinding& binding) noexcept
{
    MidiBinding sanitised = binding;

    if (! isBindingActive(sanitised))
        return {};

    sanitised.channel = juce::jlimit(midiMinimumChannel, midiMaximumChannel, sanitised.channel);

    if (sanitised.source == MidiBindingSource::pitchWheel)
    {
        sanitised.data1 = -1;
        return sanitised;
    }

    sanitised.data1 = juce::jlimit(midiMinimumData, midiMaximumData, sanitised.data1);
    return sanitised;
}

juce::String buildMidiBindingSummary(const MidiBinding& binding)
{
    if (! isBindingActive(binding))
        return {};

    const auto channelText = "Ch " + juce::String(juce::jlimit(midiMinimumChannel, midiMaximumChannel, binding.channel));

    switch (binding.source)
    {
        case MidiBindingSource::cc:
            return "CC " + juce::String(juce::jlimit(midiMinimumData, midiMaximumData, binding.data1)) + " / " + channelText;

        case MidiBindingSource::noteGate:
        case MidiBindingSource::noteVelocity:
        case MidiBindingSource::noteNumber:
        {
            const auto noteName = juce::MidiMessage::getMidiNoteName(juce::jlimit(midiMinimumData, midiMaximumData, binding.data1),
                                                                     true,
                                                                     true,
                                                                     3);
            return midiBindingSourceToDisplayName(binding.source) + " " + noteName + " / " + channelText;
        }

        case MidiBindingSource::pitchWheel:
            return "Pitch Wheel / " + channelText;

        case MidiBindingSource::none:
            break;
    }

    return {};
}

bool tryCaptureMidiLearnBinding(MidiBindingSource armedSource,
                                const juce::MidiMessage& message,
                                MidiBinding& capturedBinding) noexcept
{
    if (! isSupportedLearnMessage(armedSource, message))
        return false;

    capturedBinding = {};
    capturedBinding.source = armedSource;
    capturedBinding.channel = message.getChannel();

    if (armedSource == MidiBindingSource::cc)
    {
        capturedBinding.data1 = message.getControllerNumber();
        return true;
    }

    if (isNoteBindingSource(armedSource))
    {
        capturedBinding.data1 = message.getNoteNumber();
        return true;
    }

    if (armedSource == MidiBindingSource::pitchWheel)
    {
        capturedBinding.data1 = -1;
        return true;
    }

    return false;
}

bool tryMapMessageToBindingValue(const juce::MidiMessage& message,
                                 const MidiBinding& binding,
                                 float& value) noexcept
{
    const auto sanitisedBinding = sanitiseMidiBinding(binding);

    if (! isBindingActive(sanitisedBinding) || message.getChannel() != sanitisedBinding.channel)
        return false;

    switch (sanitisedBinding.source)
    {
        case MidiBindingSource::cc:
            if (! message.isController() || message.getControllerNumber() != sanitisedBinding.data1)
                return false;

            value = static_cast<float>(message.getControllerValue()) / 127.0f;
            return true;

        case MidiBindingSource::noteGate:
            if ((message.isNoteOn() || message.isNoteOff()) && message.getNoteNumber() == sanitisedBinding.data1)
            {
                value = message.isNoteOn() ? 1.0f : 0.0f;
                return true;
            }

            return false;

        case MidiBindingSource::noteVelocity:
            if ((message.isNoteOn() || message.isNoteOff()) && message.getNoteNumber() == sanitisedBinding.data1)
            {
                value = message.isNoteOn() ? static_cast<float>(message.getVelocity()) / 127.0f : 0.0f;
                return true;
            }

            return false;

        case MidiBindingSource::noteNumber:
            if ((message.isNoteOn() || message.isNoteOff()) && message.getNoteNumber() == sanitisedBinding.data1)
            {
                value = message.isNoteOn() ? static_cast<float>(message.getNoteNumber()) / 127.0f : 0.0f;
                return true;
            }

            return false;

        case MidiBindingSource::pitchWheel:
            if (! message.isPitchWheel())
                return false;

            value = static_cast<float>(juce::jlimit(midiPitchWheelMinimum,
                                                    midiPitchWheelMaximum,
                                                    message.getPitchWheelValue()))
                  / static_cast<float>(midiPitchWheelMaximum);
            return true;

        case MidiBindingSource::none:
            break;
    }

    return false;
}
} // namespace midi
