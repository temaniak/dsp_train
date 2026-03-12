#pragma once

#include <JuceHeader.h>

class UserCodeDocumentManager
{
public:
    UserCodeDocumentManager();

    juce::CodeDocument& getDocument() noexcept;
    juce::CPlusPlusCodeTokeniser& getTokeniser() noexcept;

    juce::String getCodeText() const;
    void setCodeText(const juce::String& newCode);
    void resetToTemplate();
    juce::String getDefaultProcessorClassName() const;

    juce::Result loadFromFile(const juce::File& file);
    juce::Result saveToFile(const juce::File& file) const;

private:
    juce::String getDefaultTemplate() const;

    juce::CodeDocument document;
    juce::CPlusPlusCodeTokeniser tokeniser;
};
