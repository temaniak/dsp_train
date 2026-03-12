#include "presets/PresetManager.h"

juce::Result PresetManager::savePreset(const juce::File& targetFile, const BuiltinPresetData& preset) const
{
    juce::DynamicObject::Ptr root(new juce::DynamicObject());
    root->setProperty("version", 1);
    root->setProperty("processorId", preset.processorId);

    juce::Array<juce::var> values;

    for (const auto value : preset.parameterValues)
        values.add(value);

    root->setProperty("parameterValues", values);

    if (! targetFile.replaceWithText(juce::JSON::toString(juce::var(root.get()), true)))
        return juce::Result::fail("Failed to save preset file.");

    return juce::Result::ok();
}

juce::Result PresetManager::loadPreset(const juce::File& sourceFile, BuiltinPresetData& preset) const
{
    if (! sourceFile.existsAsFile())
        return juce::Result::fail("Preset file does not exist.");

    const auto parsed = juce::JSON::parse(sourceFile);

    if (parsed.isVoid() || ! parsed.isObject())
        return juce::Result::fail("Preset file is not valid JSON.");

    auto* object = parsed.getDynamicObject();

    if (object == nullptr)
        return juce::Result::fail("Preset JSON root object is invalid.");

    preset.processorId = object->getProperty("processorId").toString();

    const auto values = object->getProperty("parameterValues");

    if (! values.isArray())
        return juce::Result::fail("Preset parameter list is invalid.");

    const auto* array = values.getArray();

    for (std::size_t index = 0; index < preset.parameterValues.size(); ++index)
        preset.parameterValues[index] = index < array->size() ? static_cast<float>(array->getReference(static_cast<int>(index))) : 0.0f;

    return juce::Result::ok();
}
