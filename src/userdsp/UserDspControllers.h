#pragma once

#include <JuceHeader.h>

#include "midi/MidiBinding.h"
#include "UserDspApi.h"

enum class UserDspControllerType
{
    knob,
    button,
    toggle
};

struct UserDspControllerDefinition
{
    UserDspControllerType type = UserDspControllerType::knob;
    juce::String label;
    juce::String codeName;
    MidiBinding midiBinding;
    juce::String midiBindingHint;

    bool operator==(const UserDspControllerDefinition& other) const noexcept
    {
        return type == other.type
            && label == other.label
            && codeName == other.codeName
            && midiBinding == other.midiBinding
            && midiBindingHint == other.midiBindingHint;
    }

    bool operator!=(const UserDspControllerDefinition& other) const noexcept
    {
        return ! (*this == other);
    }
};

inline juce::String userDspControllerTypeToStableString(UserDspControllerType type)
{
    switch (type)
    {
        case UserDspControllerType::knob:   return "knob";
        case UserDspControllerType::button: return "button";
        case UserDspControllerType::toggle: return "toggle";
    }

    return "knob";
}

inline juce::String userDspControllerTypeToDisplayName(UserDspControllerType type)
{
    switch (type)
    {
        case UserDspControllerType::knob:   return "Knob";
        case UserDspControllerType::button: return "Button";
        case UserDspControllerType::toggle: return "Toggle";
    }

    return "Knob";
}

inline UserDspControllerType userDspControllerTypeFromStableString(const juce::String& text)
{
    const auto trimmed = text.trim().toLowerCase();

    if (trimmed == "button")
        return UserDspControllerType::button;

    if (trimmed == "toggle")
        return UserDspControllerType::toggle;

    return UserDspControllerType::knob;
}
