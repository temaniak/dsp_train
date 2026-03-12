#pragma once

#include <JuceHeader.h>

#include "userdsp/UserDspControllers.h"

namespace userdsp
{
struct LegacySourceImportResult
{
    juce::String strippedSource;
    juce::String detectedProcessorClassName;
    bool removedSdkInclude = false;
    bool removedApiMacro = false;
};

juce::String getDefaultTemplateProcessorClassName();
juce::String getDefaultTemplateSource();
std::vector<UserDspControllerDefinition> getDefaultTemplateControllers();
juce::String scrubSourceForValidation(const juce::String& sourceCode);
bool containsSdkInclude(const juce::String& scrubbedSource);
bool containsManualApiExport(const juce::String& scrubbedSource);
juce::Result validateProcessorClassName(const juce::String& processorClassName);
juce::Result validateSourceDoesNotContainManualSdk(const juce::String& sourceCode, const juce::String& sourceLabel);
juce::Result validateSupportedProjectFileName(const juce::String& fileName);
juce::Result validateControllerDefinition(const UserDspControllerDefinition& definition);
juce::Result validateControllerDefinitions(const std::vector<UserDspControllerDefinition>& definitions);
bool isSupportedProjectFileName(const juce::String& fileName);
bool isSupportedProjectFilePath(const juce::String& relativePath);
juce::String normaliseProjectPath(const juce::String& relativePath);
juce::String sanitiseProjectItemName(const juce::String& itemName);
juce::String sanitiseControllerCodeName(const juce::String& codeName);
juce::String makeDefaultControllerLabel(UserDspControllerType type, int ordinal);
juce::String makeDefaultControllerCodeName(UserDspControllerType type, int ordinal);
juce::String getLastIdentifierSegment(const juce::String& qualifiedIdentifier);
bool sourceDefinesUnqualifiedType(const juce::String& sourceCode, const juce::String& unqualifiedTypeName);
juce::String injectSdkAndControlsIncludesIntoSource(const juce::String& sourceCode, const juce::String& controlsHeaderRelativePath);
juce::String buildGeneratedControlsHeader(const std::vector<UserDspControllerDefinition>& definitions);
juce::String buildWrapperSourceSnippet(const std::vector<UserDspControllerDefinition>& definitions,
                                       const juce::String& processorClassName);
juce::String buildWrapperTranslationUnit(const juce::String& processorIncludeRelativePath,
                                         const std::vector<UserDspControllerDefinition>& definitions,
                                         const juce::String& processorClassName,
                                         const juce::String& controlsHeaderRelativePath);
LegacySourceImportResult stripLegacySdkWrappers(const juce::String& sourceCode);
} // namespace userdsp
