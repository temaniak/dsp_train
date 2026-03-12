#pragma once

#include "dsp/BuiltinProcessors.h"

class PresetManager
{
public:
    juce::Result savePreset(const juce::File& targetFile, const BuiltinPresetData& preset) const;
    juce::Result loadPreset(const juce::File& sourceFile, BuiltinPresetData& preset) const;
};
