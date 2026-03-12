#include <JuceHeader.h>

#include "dsp/BuiltinProcessors.h"
#include "presets/PresetManager.h"

namespace
{
bool approximatelyEqual(float a, float b, float epsilon = 1.0e-4f)
{
    return std::abs(a - b) <= epsilon;
}
}

int main()
{
    BuiltinProcessorChain processor;
    processor.prepare(48000.0, 512);
    processor.setSelectedProcessor(BuiltinProcessorType::gain);
    processor.resetParametersToDefaults(BuiltinProcessorType::gain);
    processor.setParameter(0, 0.5f);

    std::array<float, 4> input { 1.0f, -1.0f, 0.25f, -0.25f };
    std::array<float, 4> output {};
    processor.process(input.data(), output.data(), static_cast<int>(input.size()));

    if (! approximatelyEqual(output[0], 0.5f) || ! approximatelyEqual(output[1], -0.5f))
        return 1;

    PresetManager presetManager;
    BuiltinPresetData preset;
    preset.processorId = "simpleDelay";
    preset.parameterValues = { 120.0f, 0.4f, 0.25f, 0.0f };

    const auto tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                            .getChildFile("dsp_education_stand_preset_test.json");

    if (const auto saveResult = presetManager.savePreset(tempFile, preset); saveResult.failed())
        return 2;

    BuiltinPresetData loadedPreset;

    if (const auto loadResult = presetManager.loadPreset(tempFile, loadedPreset); loadResult.failed())
        return 3;

    if (loadedPreset.processorId != preset.processorId || ! approximatelyEqual(loadedPreset.parameterValues[1], preset.parameterValues[1]))
        return 4;

    return 0;
}
