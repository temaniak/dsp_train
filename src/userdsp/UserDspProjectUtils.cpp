#include "userdsp/UserDspProjectUtils.h"

#include <cctype>
#include <regex>

namespace
{
const char* defaultProcessorClassName = "MyAudioProcessor";

const char* defaultTemplateText = R"CPP(// UserDspApi.h, the generated controls header, and the entry wrapper
// are injected automatically by the application.
//
// Start by editing this class and pressing Compile.
// Add runtime controls from the Controls window when you need them.

class MyAudioProcessor
{
public:
    const char* getName() const
    {
        return "New User DSP Project";
    }

    bool getPreferredAudioConfig(DspEduPreferredAudioConfig& config) const
    {
        config.preferredInputChannels = 2;
        config.preferredOutputChannels = 2;
        return true;
    }

    void prepare(const DspEduProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
    }

    void process(const float* const* inputs,
                 float* const* outputs,
                 int numInputChannels,
                 int numOutputChannels,
                 int numSamples)
    {
        for (int channel = 0; channel < numOutputChannels; ++channel)
        {
            auto* output = outputs[channel];

            if (output == nullptr)
                continue;

            const auto* input = getInputChannel(inputs, numInputChannels, channel);

            for (int sample = 0; sample < numSamples; ++sample)
                output[sample] = input != nullptr ? input[sample] : 0.0f;
        }
    }

private:
    const float* getInputChannel(const float* const* inputs, int numInputChannels, int channel) const
    {
        if (inputs == nullptr || numInputChannels <= 0)
            return nullptr;

        if (channel < numInputChannels && inputs[channel] != nullptr)
            return inputs[channel];

        return inputs[0];
    }

    double sampleRate = 44100.0;
};
)CPP";

bool isIdentifierStart(char character)
{
    return std::isalpha(static_cast<unsigned char>(character)) != 0 || character == '_';
}

bool isIdentifierBody(char character)
{
    return std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '_';
}

bool isValidQualifiedIdentifier(const juce::String& text)
{
    const auto trimmed = text.trim();

    if (trimmed.isEmpty())
        return false;

    const auto bytes = trimmed.toStdString();
    std::size_t index = 0;

    while (index < bytes.size())
    {
        if (! isIdentifierStart(bytes[index]))
            return false;

        ++index;

        while (index < bytes.size() && isIdentifierBody(bytes[index]))
            ++index;

        if (index == bytes.size())
            return true;

        if (bytes[index] != ':' || index + 1 >= bytes.size() || bytes[index + 1] != ':')
            return false;

        index += 2;
    }

    return true;
}

bool isValidIdentifier(const juce::String& text)
{
    const auto trimmed = text.trim();

    if (trimmed.isEmpty())
        return false;

    const auto bytes = trimmed.toStdString();

    if (! isIdentifierStart(bytes.front()))
        return false;

    for (std::size_t index = 1; index < bytes.size(); ++index)
        if (! isIdentifierBody(bytes[index]))
            return false;

    return true;
}

juce::String trimLeadingBlankLines(const juce::String& sourceCode)
{
    int start = 0;

    while (start < sourceCode.length())
    {
        const auto character = sourceCode[start];

        if (character == '\n' || character == '\r')
        {
            ++start;
            continue;
        }

        break;
    }

    return sourceCode.substring(start);
}

std::string escapeRegexLiteral(const juce::String& text)
{
    std::string escaped;

    for (const auto character : text.toStdString())
    {
        switch (character)
        {
            case '\\':
            case '.':
            case '^':
            case '$':
            case '|':
            case '(':
            case ')':
            case '[':
            case ']':
            case '{':
            case '}':
            case '*':
            case '+':
            case '?':
                escaped.push_back('\\');
                escaped.push_back(character);
                break;

            default:
                escaped.push_back(character);
                break;
        }
    }

    return escaped;
}
} // namespace

namespace userdsp
{
juce::String getDefaultTemplateProcessorClassName()
{
    return defaultProcessorClassName;
}

juce::String getDefaultTemplateSource()
{
    return defaultTemplateText;
}

std::vector<UserDspControllerDefinition> getDefaultTemplateControllers()
{
    return {};
}

juce::String scrubSourceForValidation(const juce::String& sourceCode)
{
    enum class State
    {
        normal,
        lineComment,
        blockComment,
        stringLiteral,
        charLiteral
    };

    juce::String scrubbed;
    scrubbed.preallocateBytes(sourceCode.getNumBytesAsUTF8());

    auto appendMasked = [&scrubbed] (juce_wchar character)
    {
        scrubbed += (character == '\n' || character == '\r') ? character : ' ';
    };

    State state = State::normal;
    bool escaped = false;

    for (int index = 0; index < sourceCode.length(); ++index)
    {
        const auto character = sourceCode[index];
        const auto nextCharacter = index + 1 < sourceCode.length() ? sourceCode[index + 1] : 0;

        switch (state)
        {
            case State::normal:
                if (character == '/' && nextCharacter == '/')
                {
                    scrubbed += ' ';
                    scrubbed += ' ';
                    ++index;
                    state = State::lineComment;
                }
                else if (character == '/' && nextCharacter == '*')
                {
                    scrubbed += ' ';
                    scrubbed += ' ';
                    ++index;
                    state = State::blockComment;
                }
                else if (character == '"')
                {
                    scrubbed += ' ';
                    state = State::stringLiteral;
                    escaped = false;
                }
                else if (character == '\'')
                {
                    scrubbed += ' ';
                    state = State::charLiteral;
                    escaped = false;
                }
                else
                {
                    scrubbed += character;
                }
                break;

            case State::lineComment:
                if (character == '\n' || character == '\r')
                {
                    scrubbed += character;
                    state = State::normal;
                }
                else
                {
                    scrubbed += ' ';
                }
                break;

            case State::blockComment:
                if (character == '*' && nextCharacter == '/')
                {
                    scrubbed += ' ';
                    scrubbed += ' ';
                    ++index;
                    state = State::normal;
                }
                else
                {
                    appendMasked(character);
                }
                break;

            case State::stringLiteral:
                appendMasked(character);

                if (escaped)
                {
                    escaped = false;
                }
                else if (character == '\\')
                {
                    escaped = true;
                }
                else if (character == '"')
                {
                    state = State::normal;
                }
                break;

            case State::charLiteral:
                appendMasked(character);

                if (escaped)
                {
                    escaped = false;
                }
                else if (character == '\\')
                {
                    escaped = true;
                }
                else if (character == '\'')
                {
                    state = State::normal;
                }
                break;
        }
    }

    return scrubbed;
}

bool containsSdkInclude(const juce::String& scrubbedSource)
{
    juce::StringArray lines;
    lines.addLines(scrubbedSource);

    for (const auto& line : lines)
    {
        const auto trimmed = line.trimStart();

        if (trimmed.startsWithChar('#')
            && trimmed.containsIgnoreCase("include")
            && trimmed.containsIgnoreCase("UserDspApi.h"))
        {
            return true;
        }
    }

    return false;
}

bool containsManualApiExport(const juce::String& scrubbedSource)
{
    return scrubbedSource.contains("DSP_EDU_DEFINE_SIMPLE_PLUGIN(")
        || scrubbedSource.contains("DSP_EDU_DEFINE_PLUGIN(")
        || scrubbedSource.contains("dspedu_get_api");
}

juce::Result validateProcessorClassName(const juce::String& processorClassName)
{
    const auto trimmedClassName = processorClassName.trim();

    if (trimmedClassName.isEmpty())
        return juce::Result::fail("Processor Class is required. Enter the class name to export, for example MyAudioProcessor.");

    if (! isValidQualifiedIdentifier(trimmedClassName))
        return juce::Result::fail("Processor Class must be a valid C++ class name.");

    return juce::Result::ok();
}

juce::Result validateSourceDoesNotContainManualSdk(const juce::String& sourceCode, const juce::String& sourceLabel)
{
    const auto scrubbedSource = scrubSourceForValidation(sourceCode);

    if (containsSdkInclude(scrubbedSource))
        return juce::Result::fail(sourceLabel + " includes UserDspApi.h manually. The application adds it automatically.");

    if (containsManualApiExport(scrubbedSource))
        return juce::Result::fail(sourceLabel + " exports the DSP API manually. Enter the processor class name in the Processor Class field instead.");

    return juce::Result::ok();
}

juce::Result validateSupportedProjectFileName(const juce::String& fileName)
{
    const auto trimmed = fileName.trim();

    if (trimmed.isEmpty())
        return juce::Result::fail("File name is required.");

    if (trimmed.containsAnyOf("\\/:*?\"<>|"))
        return juce::Result::fail("File name contains unsupported characters.");

    if (! isSupportedProjectFileName(trimmed))
        return juce::Result::fail("Only .cpp, .h and .hpp files are supported in DSP projects.");

    return juce::Result::ok();
}

juce::Result validateControllerDefinition(const UserDspControllerDefinition& definition)
{
    if (definition.label.trim().isEmpty())
        return juce::Result::fail("Controller label is required.");

    if (! isValidIdentifier(definition.codeName))
        return juce::Result::fail("Controller code name must be a valid C++ identifier.");

    if (const auto midiValidation = midi::validateMidiBinding(definition.midiBinding); midiValidation.failed())
        return midiValidation;

    return juce::Result::ok();
}

juce::Result validateControllerDefinitions(const std::vector<UserDspControllerDefinition>& definitions)
{
    if (definitions.size() > static_cast<std::size_t>(DSP_EDU_USER_DSP_MAX_CONTROLS))
    {
        return juce::Result::fail("A DSP project can define at most "
                                  + juce::String(DSP_EDU_USER_DSP_MAX_CONTROLS)
                                  + " controllers.");
    }

    juce::StringArray seenCodeNames;

    for (const auto& definition : definitions)
    {
        if (const auto validationResult = validateControllerDefinition(definition); validationResult.failed())
            return validationResult;

        if (seenCodeNames.contains(definition.codeName))
            return juce::Result::fail("Controller code names must be unique.");

        seenCodeNames.add(definition.codeName);
    }

    return juce::Result::ok();
}

bool isSupportedProjectFileName(const juce::String& fileName)
{
    const auto extension = juce::File(fileName).getFileExtension().toLowerCase();
    return extension == ".cpp" || extension == ".h" || extension == ".hpp";
}

bool isSupportedProjectFilePath(const juce::String& relativePath)
{
    return isSupportedProjectFileName(juce::File(relativePath).getFileName());
}

juce::String normaliseProjectPath(const juce::String& relativePath)
{
    auto normalised = relativePath.trim().replaceCharacter('\\', '/');

    while (normalised.startsWithChar('/'))
        normalised = normalised.substring(1);

    while (normalised.endsWithChar('/'))
        normalised = normalised.dropLastCharacters(1);

    return normalised;
}

juce::String sanitiseProjectItemName(const juce::String& itemName)
{
    auto cleaned = itemName.trim();
    cleaned = cleaned.replaceCharacter('\\', '_');
    cleaned = cleaned.replaceCharacter('/', '_');
    return cleaned;
}

juce::String sanitiseControllerCodeName(const juce::String& codeName)
{
    juce::String result;
    const auto trimmed = codeName.trim();

    for (int index = 0; index < trimmed.length(); ++index)
    {
        auto character = trimmed[index];

        if (character == ' ' || character == '-')
            character = '_';

        if (result.isEmpty())
        {
            if (isIdentifierStart(static_cast<char>(character)))
                result += character;
            else if (isIdentifierBody(static_cast<char>(character)))
                result += '_';
        }
        else if (isIdentifierBody(static_cast<char>(character)))
        {
            result += character;
        }
    }

    return result.trim();
}

juce::String makeDefaultControllerLabel(UserDspControllerType type, int ordinal)
{
    return userDspControllerTypeToDisplayName(type) + " " + juce::String(ordinal);
}

juce::String makeDefaultControllerCodeName(UserDspControllerType type, int ordinal)
{
    auto stem = userDspControllerTypeToStableString(type);

    if (type == UserDspControllerType::toggle)
        stem = "toggle";

    return stem + juce::String(ordinal);
}

juce::String getLastIdentifierSegment(const juce::String& qualifiedIdentifier)
{
    const auto parts = juce::StringArray::fromTokens(qualifiedIdentifier, ":", {});

    if (parts.isEmpty())
        return qualifiedIdentifier.trim();

    return parts[parts.size() - 1].trim();
}

bool sourceDefinesUnqualifiedType(const juce::String& sourceCode, const juce::String& unqualifiedTypeName)
{
    if (unqualifiedTypeName.isEmpty())
        return false;

    const auto scrubbedSource = scrubSourceForValidation(sourceCode).toStdString();
    const auto escapedName = escapeRegexLiteral(unqualifiedTypeName);
    const std::regex typeDefinitionPattern("\\b(?:class|struct)\\s+" + escapedName + "\\b[^;\\{]*\\{");

    return std::regex_search(scrubbedSource, typeDefinitionPattern);
}

juce::String injectSdkAndControlsIncludesIntoSource(const juce::String& sourceCode, const juce::String& controlsHeaderRelativePath)
{
    juce::String wrappedSource;
    wrappedSource << "#include \"UserDspApi.h\"\n";
    wrappedSource << "#include \"" << normaliseProjectPath(controlsHeaderRelativePath) << "\"\n\n";
    wrappedSource << trimLeadingBlankLines(sourceCode).trimEnd() << "\n";
    return wrappedSource;
}

juce::String buildGeneratedControlsHeader(const std::vector<UserDspControllerDefinition>& definitions)
{
    juce::String header;
    header << "#pragma once\n\n";
    header << "#include <algorithm>\n";
    header << "#include \"UserDspApi.h\"\n\n";
    header << "struct DspEduProjectControls\n{\n";

    for (const auto& definition : definitions)
    {
        if (definition.type == UserDspControllerType::knob)
            header << "    float " << definition.codeName << " = 0.0f;\n";
        else
            header << "    bool " << definition.codeName << " = false;\n";
    }

    header << "};\n\n";
    header << "namespace dspedu\n{\n";
    header << "const DspEduProjectControls& getProjectControls() noexcept;\n";
    header << "void setProjectControlValue(int controlIndex, float value) noexcept;\n";
    header << "const DspEduMidiState& getMidiState() noexcept;\n";
    header << "void setMidiState(const DspEduMidiState& state) noexcept;\n";
    header << "} // namespace dspedu\n\n";
    header << "inline const auto& controls = ::dspedu::getProjectControls();\n";
    header << "inline const auto& midi = ::dspedu::getMidiState();\n";
    return header;
}

juce::String buildWrapperSourceSnippet(const std::vector<UserDspControllerDefinition>& definitions,
                                       const juce::String& processorClassName)
{
    juce::String wrappedSource;
    wrappedSource << "namespace\n{\n";
    wrappedSource << "DspEduProjectControls gDspEduProjectControls;\n";
    wrappedSource << "DspEduMidiState gDspEduMidiState;\n";
    wrappedSource << "} // namespace\n\n";
    wrappedSource << "namespace dspedu\n{\n";
    wrappedSource << "const DspEduProjectControls& getProjectControls() noexcept\n";
    wrappedSource << "{\n";
    wrappedSource << "    return ::gDspEduProjectControls;\n";
    wrappedSource << "}\n\n";
    wrappedSource << "const DspEduMidiState& getMidiState() noexcept\n";
    wrappedSource << "{\n";
    wrappedSource << "    return ::gDspEduMidiState;\n";
    wrappedSource << "}\n\n";
    wrappedSource << "void setProjectControlValue(int controlIndex, float value) noexcept\n";
    wrappedSource << "{\n";
    wrappedSource << "    switch (controlIndex)\n";
    wrappedSource << "    {\n";

    for (int index = 0; index < static_cast<int>(definitions.size()); ++index)
    {
        const auto& definition = definitions[static_cast<std::size_t>(index)];
        wrappedSource << "        case " << index << ": ";

        if (definition.type == UserDspControllerType::knob)
            wrappedSource << "::gDspEduProjectControls." << definition.codeName << " = std::clamp(value, 0.0f, 1.0f); break;\n";
        else
            wrappedSource << "::gDspEduProjectControls." << definition.codeName << " = value >= 0.5f; break;\n";
    }

    wrappedSource << "        default: break;\n";
    wrappedSource << "    }\n";
    wrappedSource << "}\n";
    wrappedSource << "\n";
    wrappedSource << "void setMidiState(const DspEduMidiState& state) noexcept\n";
    wrappedSource << "{\n";
    wrappedSource << "    ::gDspEduMidiState = state;\n";
    wrappedSource << "    ::gDspEduMidiState.structSize = sizeof(DspEduMidiState);\n";
    wrappedSource << "}\n";
    wrappedSource << "} // namespace dspedu\n\n";
    wrappedSource << "class DspEduGeneratedProcessorWrapper\n";
    wrappedSource << "{\n";
    wrappedSource << "public:\n";
    wrappedSource << "    const char* getName()\n";
    wrappedSource << "    {\n";
    wrappedSource << "        return dspedu::detail::getProcessorName(processor);\n";
    wrappedSource << "    }\n\n";
    wrappedSource << "    bool getPreferredAudioConfig(DspEduPreferredAudioConfig& config)\n";
    wrappedSource << "    {\n";
    wrappedSource << "        return dspedu::detail::getPreferredAudioConfig(processor, config);\n";
    wrappedSource << "    }\n\n";
    wrappedSource << "    void prepare(const DspEduProcessSpec& spec)\n";
    wrappedSource << "    {\n";
    wrappedSource << "        dspedu::detail::prepare(processor, spec);\n";
    wrappedSource << "    }\n\n";
    wrappedSource << "    void reset()\n";
    wrappedSource << "    {\n";
    wrappedSource << "        dspedu::detail::reset(processor);\n";
    wrappedSource << "    }\n\n";
    wrappedSource << "    void process(const float* const* inputs, float* const* outputs, int numInputChannels, int numOutputChannels, int numSamples)\n";
    wrappedSource << "    {\n";
    wrappedSource << "        dspedu::detail::process(processor, inputs, outputs, numInputChannels, numOutputChannels, numSamples);\n";
    wrappedSource << "    }\n\n";
    wrappedSource << "    void setControlValue(int controlIndex, float value)\n";
    wrappedSource << "    {\n";
    wrappedSource << "        dspedu::setProjectControlValue(controlIndex, value);\n";
    wrappedSource << "    }\n\n";
    wrappedSource << "    void setMidiState(const DspEduMidiState& state)\n";
    wrappedSource << "    {\n";
    wrappedSource << "        dspedu::setMidiState(state);\n";
    wrappedSource << "    }\n\n";
    wrappedSource << "private:\n";
    wrappedSource << "    " << processorClassName.trim() << " processor;\n";
    wrappedSource << "};\n\n";
    wrappedSource << "DSP_EDU_DEFINE_SIMPLE_PLUGIN(DspEduGeneratedProcessorWrapper)\n";
    return wrappedSource;
}

juce::String buildWrapperTranslationUnit(const juce::String& processorIncludeRelativePath,
                                         const std::vector<UserDspControllerDefinition>& definitions,
                                         const juce::String& processorClassName,
                                         const juce::String& controlsHeaderRelativePath)
{
    juce::String wrappedSource;
    wrappedSource << "#include \"UserDspApi.h\"\n";
    wrappedSource << "#include \"" << normaliseProjectPath(controlsHeaderRelativePath) << "\"\n";
    wrappedSource << "#include \"" << normaliseProjectPath(processorIncludeRelativePath) << "\"\n\n";
    wrappedSource << buildWrapperSourceSnippet(definitions, processorClassName);
    return wrappedSource;
}

LegacySourceImportResult stripLegacySdkWrappers(const juce::String& sourceCode)
{
    LegacySourceImportResult result;

    juce::StringArray lines;
    lines.addLines(sourceCode);

    juce::StringArray outputLines;
    for (const auto& line : lines)
    {
        const auto trimmed = line.trim();

        if (trimmed.startsWith("#include") && trimmed.contains("UserDspApi.h"))
        {
            result.removedSdkInclude = true;
            continue;
        }

        if (trimmed.contains("DSP_EDU_DEFINE_SIMPLE_PLUGIN("))
        {
            const auto openParen = trimmed.indexOfChar('(');
            const auto closeParen = trimmed.lastIndexOfChar(')');

            if (openParen >= 0 && closeParen > openParen)
                result.detectedProcessorClassName = trimmed.substring(openParen + 1, closeParen).trim();

            result.removedApiMacro = true;
            continue;
        }

        if (trimmed.contains("DSP_EDU_DEFINE_PLUGIN(") || trimmed.contains("dspedu_get_api"))
        {
            result.removedApiMacro = true;
            continue;
        }

        outputLines.add(line);
    }

    result.strippedSource = outputLines.joinIntoString("\n").trimEnd() + "\n";
    return result;
}
} // namespace userdsp
