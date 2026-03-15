#include "ui/RealtimeControlsComponent.h"

#include "ui/DarkIdeLookAndFeel.h"
#include "ui/ScrollPassthroughControls.h"
#include "userdsp/UserDspProjectUtils.h"

namespace
{
constexpr int tileWidth = 172;
constexpr int tileHeight = 206;
constexpr int tileGap = 10;
constexpr int textCommitDelayMs = 280;

constexpr int typeMenuKnobId = 101;
constexpr int typeMenuButtonId = 102;
constexpr int typeMenuToggleId = 103;

constexpr int midiSourceBaseId = 200;
constexpr int midiChannelBaseId = 300;
constexpr int midiDataBaseId = 400;
constexpr int midiClearId = 900;

void configureSectionLabel(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::Font(juce::FontOptions(17.0f, juce::Font::bold)));
    label.setJustificationType(juce::Justification::centredLeft);
    label.setColour(juce::Label::textColourId, ide::text);
}

void configureBodyLabel(juce::Label& label,
                        const juce::String& text,
                        juce::Justification justification = juce::Justification::topLeft,
                        float fontHeight = 14.0f,
                        bool bold = false)
{
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(justification);
    label.setColour(juce::Label::textColourId, ide::textSecondary);
    label.setFont(juce::Font(juce::FontOptions(fontHeight, bold ? juce::Font::bold : juce::Font::plain)));
}

void configureTextButton(juce::TextButton& button)
{
    button.setColour(juce::TextButton::buttonColourId, ide::active);
    button.setColour(juce::TextButton::buttonOnColourId, ide::active.brighter(0.15f));
    button.setColour(juce::TextButton::textColourOffId, ide::text);
    button.setColour(juce::TextButton::textColourOnId, ide::text);
}

void configureInlineTextEditor(juce::TextEditor& editor, float fontHeight)
{
    editor.setMultiLine(false);
    editor.setReturnKeyStartsNewLine(false);
    editor.setJustification(juce::Justification::centredLeft);
    editor.setScrollbarsShown(false);
    editor.setFont(juce::Font(juce::FontOptions(fontHeight, juce::Font::plain)));
    editor.setIndents(8, 6);
    editor.setColour(juce::TextEditor::backgroundColourId, ide::active);
    editor.setColour(juce::TextEditor::textColourId, ide::text);
    editor.setColour(juce::TextEditor::highlightColourId, ide::border);
    editor.setColour(juce::TextEditor::highlightedTextColourId, ide::text);
    editor.setColour(juce::TextEditor::outlineColourId, ide::border);
    editor.setColour(juce::TextEditor::focusedOutlineColourId, ide::constant);
    editor.setColour(juce::TextEditor::shadowColourId, juce::Colours::transparentBlack);
}

UserDspControllerType menuIdToControllerType(int menuId)
{
    switch (menuId)
    {
        case typeMenuButtonId: return UserDspControllerType::button;
        case typeMenuToggleId: return UserDspControllerType::toggle;
        default: return UserDspControllerType::knob;
    }
}

bool isNoteBindingSource(MidiBindingSource source) noexcept
{
    return source == MidiBindingSource::noteGate
        || source == MidiBindingSource::noteVelocity
        || source == MidiBindingSource::noteNumber;
}

MidiBinding defaultMidiBindingForSource(MidiBindingSource source) noexcept
{
    MidiBinding binding;
    binding.source = source;
    binding.channel = 1;

    if (source == MidiBindingSource::pitchWheel)
        binding.data1 = -1;
    else if (isNoteBindingSource(source))
        binding.data1 = 60;
    else
        binding.data1 = 0;

    return binding;
}

MidiBinding sourceMenuIdToBinding(int menuId, const MidiBinding& currentBinding)
{
    const auto requestedSource = [menuId]() noexcept
    {
        switch (menuId)
        {
            case midiSourceBaseId + 1: return MidiBindingSource::none;
            case midiSourceBaseId + 2: return MidiBindingSource::cc;
            case midiSourceBaseId + 3: return MidiBindingSource::noteGate;
            case midiSourceBaseId + 4: return MidiBindingSource::noteVelocity;
            case midiSourceBaseId + 5: return MidiBindingSource::noteNumber;
            case midiSourceBaseId + 6: return MidiBindingSource::pitchWheel;
            default:                   return MidiBindingSource::none;
        }
    }();

    if (requestedSource == MidiBindingSource::none)
        return {};

    auto binding = midi::isBindingActive(currentBinding)
                     ? midi::sanitiseMidiBinding(currentBinding)
                     : defaultMidiBindingForSource(requestedSource);

    if (binding.source != requestedSource)
    {
        const bool preservingNoteNumber = isNoteBindingSource(binding.source) && isNoteBindingSource(requestedSource);
        binding = defaultMidiBindingForSource(requestedSource);
        binding.channel = midi::isBindingActive(currentBinding) ? currentBinding.channel : 1;

        if (preservingNoteNumber)
            binding.data1 = juce::jlimit(0, 127, currentBinding.data1);
    }

    binding.source = requestedSource;
    return midi::sanitiseMidiBinding(binding);
}

juce::String buildEditValuePreview(UserDspControllerType type, float value)
{
    switch (type)
    {
        case UserDspControllerType::knob:
            return juce::String(value, 3);

        case UserDspControllerType::button:
        case UserDspControllerType::toggle:
            return value >= 0.5f ? "On" : "Off";
    }

    return {};
}

bool definitionsMatchSnapshot(const std::vector<UserDspControllerDefinition>& definitions,
                              const UserDspHost::Snapshot& snapshot)
{
    if (! snapshot.hasActiveModule || snapshot.controlCount != static_cast<int>(definitions.size()))
        return false;

    for (int index = 0; index < snapshot.controlCount; ++index)
    {
        const auto& compiledControl = snapshot.controls[static_cast<std::size_t>(index)];
        const auto& projectControl = definitions[static_cast<std::size_t>(index)];

        if (! compiledControl.active
            || compiledControl.type != projectControl.type
            || compiledControl.label != projectControl.label
            || compiledControl.codeName != projectControl.codeName)
        {
            return false;
        }
    }

    return true;
}

std::vector<float> remapPreviewValues(const std::vector<UserDspControllerDefinition>& previousDefinitions,
                                      const std::vector<float>& previousValues,
                                      const std::vector<UserDspControllerDefinition>& nextDefinitions)
{
    std::vector<float> nextValues(nextDefinitions.size(), 0.0f);

    for (std::size_t nextIndex = 0; nextIndex < nextDefinitions.size(); ++nextIndex)
    {
        const auto& nextDefinition = nextDefinitions[nextIndex];

        for (std::size_t previousIndex = 0; previousIndex < previousDefinitions.size(); ++previousIndex)
        {
            const auto& previousDefinition = previousDefinitions[previousIndex];

            if (nextDefinition.codeName == previousDefinition.codeName
                && nextDefinition.type == previousDefinition.type
                && previousIndex < previousValues.size())
            {
                nextValues[nextIndex] = previousValues[previousIndex];
                break;
            }
        }
    }

    return nextValues;
}

juce::String buildDisplayedMidiSummary(const UserDspControllerDefinition& definition)
{
    if (midi::isBindingActive(definition.midiBinding))
        return midi::buildMidiBindingSummary(definition.midiBinding);

    return definition.midiBindingHint;
}

void populateMidiDataMenu(juce::PopupMenu& parentMenu, MidiBindingSource source, int selectedData1)
{
    for (int groupStart = 0; groupStart < 128; groupStart += 16)
    {
        juce::PopupMenu groupMenu;
        const int groupEnd = juce::jmin(127, groupStart + 15);

        for (int value = groupStart; value <= groupEnd; ++value)
        {
            juce::String itemLabel;

            if (source == MidiBindingSource::cc)
            {
                itemLabel = "CC " + juce::String(value);
            }
            else
            {
                itemLabel = juce::MidiMessage::getMidiNoteName(value, true, true, 3)
                          + " (" + juce::String(value) + ")";
            }

            groupMenu.addItem(midiDataBaseId + value + 1, itemLabel, true, value == selectedData1);
        }

        juce::String groupLabel;

        if (source == MidiBindingSource::cc)
        {
            groupLabel = juce::String(groupStart) + "-" + juce::String(groupEnd);
        }
        else
        {
            groupLabel = juce::MidiMessage::getMidiNoteName(groupStart, true, true, 3)
                       + " - "
                       + juce::MidiMessage::getMidiNoteName(groupEnd, true, true, 3);
        }

        parentMenu.addSubMenu(groupLabel, groupMenu);
    }
}

enum class ControlsDisplayMode
{
    edit,
    play
};

bool showDeleteDialog(const juce::String& label, juce::LookAndFeel* lookAndFeel)
{
    juce::AlertWindow window("Delete Controller",
                             "Delete controller \"" + label + "\"?",
                             juce::MessageBoxIconType::WarningIcon);

    if (lookAndFeel != nullptr)
        window.setLookAndFeel(lookAndFeel);

    window.addButton("Delete", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    return window.runModalLoop() == 1;
}

class ForwardingSlider final : public ScrollPassthroughSlider
{
public:
    bool isValueEditorActive() const
    {
        return isMouseButtonDown() || hasKeyboardFocus(true);
    }
};

class ActionTextButton : public juce::TextButton
{
public:
    using juce::TextButton::TextButton;
};

class MomentaryTextButton final : public ActionTextButton
{
public:
    using ActionTextButton::ActionTextButton;

    std::function<void(bool)> onMomentaryValueChanged;

    void setMomentaryState(bool shouldBeDown, bool notify)
    {
        if (getToggleState() == shouldBeDown)
            return;

        setToggleState(shouldBeDown, juce::dontSendNotification);
        setButtonText(shouldBeDown ? "Pressed" : "Hold");

        if (notify && onMomentaryValueChanged != nullptr)
            onMomentaryValueChanged(shouldBeDown);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        ActionTextButton::mouseDown(event);

        if (event.mods.isLeftButtonDown())
            setMomentaryState(true, true);
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        ActionTextButton::mouseUp(event);
        setMomentaryState(false, true);
    }

    void mouseExit(const juce::MouseEvent& event) override
    {
        ActionTextButton::mouseExit(event);

        if (event.mods.isAnyMouseButtonDown())
            setMomentaryState(false, true);
    }
};

class ToggleTextButton final : public ActionTextButton
{
public:
    std::function<void(bool)> onLatchedValueChanged;

    ToggleTextButton(const juce::String& buttonText = {})
        : ActionTextButton(buttonText)
    {
        setClickingTogglesState(true);
        setButtonText("Off");
        onClick = [this]
        {
            setButtonText(getToggleState() ? "On" : "Off");

            if (onLatchedValueChanged != nullptr)
                onLatchedValueChanged(getToggleState());
        };
    }

    void setLatchedState(bool shouldBeOn)
    {
        if (getToggleState() != shouldBeOn)
            setToggleState(shouldBeOn, juce::dontSendNotification);

        setButtonText(getToggleState() ? "On" : "Off");
    }
};

} // namespace

class ControllerTileComponent final : public juce::Component,
                                      private juce::Timer
{
public:
    ControllerTileComponent(std::function<juce::Result(int, const UserDspControllerDefinition&)> onDefinitionChanged,
                            std::function<void(int, MidiBindingSource)> onMidiLearnRequested,
                            std::function<void(int)> onMidiLearnCancelled,
                            std::function<void(int, int)> onMove,
                            std::function<void(int)> onDelete,
                            std::function<void(int, float)> onValueChanged)
        : definitionChanged(std::move(onDefinitionChanged)),
          midiLearnRequested(std::move(onMidiLearnRequested)),
          midiLearnCancelled(std::move(onMidiLearnCancelled)),
          moveRequested(std::move(onMove)),
          deleteRequested(std::move(onDelete)),
          valueChanged(std::move(onValueChanged))
    {
        configureBodyLabel(labelLabel, {}, juce::Justification::centredLeft, 15.0f, true);
        labelLabel.setColour(juce::Label::textColourId, ide::text);

        configureBodyLabel(codePrefixLabel, "controls.", juce::Justification::centredLeft, 12.0f, false);
        codePrefixLabel.setColour(juce::Label::textColourId, ide::textMuted);

        configureBodyLabel(valuePreviewLabel, {}, juce::Justification::centred, 14.0f, false);
        valuePreviewLabel.setColour(juce::Label::textColourId, ide::text);
        valuePreviewLabel.setColour(juce::Label::backgroundColourId, ide::active);
        valuePreviewLabel.setColour(juce::Label::outlineColourId, ide::border);

        configureBodyLabel(runtimeLabel, {}, juce::Justification::centredLeft, 12.0f, false);

        configureInlineTextEditor(labelEditor, 15.0f);
        configureInlineTextEditor(codeNameEditor, 12.5f);
        labelEditor.setTextToShowWhenEmpty("Controller name", ide::textMuted);
        codeNameEditor.setTextToShowWhenEmpty("name", ide::textMuted);

        configureTextButton(typeButton);
        configureTextButton(midiButton);
        configureTextButton(learnButton);
        configureTextButton(clearButton);
        configureTextButton(moveLeftButton);
        configureTextButton(moveRightButton);
        configureTextButton(deleteButton);
        configureTextButton(momentaryButton);
        configureTextButton(toggleButton);

        moveLeftButton.setButtonText("<");
        moveRightButton.setButtonText(">");
        deleteButton.setButtonText("Del");
        learnButton.setButtonText("Learn");
        clearButton.setButtonText("Clear");

        knobSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knobSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 88, 24);
        knobSlider.setTextBoxIsEditable(true);
        knobSlider.setRange(0.0, 1.0, 0.001);
        knobSlider.setNumDecimalPlacesToDisplay(3);
        knobSlider.setScrollWheelEnabled(false);
        knobSlider.setColour(juce::Slider::rotarySliderFillColourId, ide::constant);
        knobSlider.setColour(juce::Slider::rotarySliderOutlineColourId, ide::border.brighter(0.1f));
        knobSlider.setColour(juce::Slider::textBoxTextColourId, ide::text);
        knobSlider.setColour(juce::Slider::textBoxBackgroundColourId, ide::active);
        knobSlider.setColour(juce::Slider::textBoxOutlineColourId, ide::border);
        knobSlider.onValueChange = [this]
        {
            if (! suppressValueCallbacks && valueChanged != nullptr)
                valueChanged(controllerIndex, static_cast<float>(knobSlider.getValue()));
        };

        momentaryButton.setButtonText("Hold");
        momentaryButton.onMomentaryValueChanged = [this] (bool isDown)
        {
            if (! suppressValueCallbacks && valueChanged != nullptr)
                valueChanged(controllerIndex, isDown ? 1.0f : 0.0f);
        };

        toggleButton.onLatchedValueChanged = [this] (bool isOn)
        {
            if (! suppressValueCallbacks && valueChanged != nullptr)
                valueChanged(controllerIndex, isOn ? 1.0f : 0.0f);
        };

        labelEditor.onTextChange = [this] { scheduleTextCommit(); };
        codeNameEditor.onTextChange = [this] { scheduleTextCommit(); };
        labelEditor.onReturnKey = [this] { commitTextEditorsNow(); };
        codeNameEditor.onReturnKey = [this] { commitTextEditorsNow(); };
        labelEditor.onFocusLost = [this] { commitTextEditorsNow(); };
        codeNameEditor.onFocusLost = [this] { commitTextEditorsNow(); };
        labelEditor.onEscapeKey = [this] { revertTextEditorsToDefinition(); };
        codeNameEditor.onEscapeKey = [this] { revertTextEditorsToDefinition(); };

        typeButton.onClick = [this] { showTypeMenu(); };
        midiButton.onClick = [this] { showMidiMenu(); };

        learnButton.onClick = [this]
        {
            if (learnArmed)
            {
                if (midiLearnCancelled != nullptr)
                    midiLearnCancelled(controllerIndex);

                return;
            }

            if (midiLearnRequested != nullptr)
                midiLearnRequested(controllerIndex, definition.midiBinding.source);
        };

        clearButton.onClick = [this]
        {
            auto updatedDefinition = definition;
            updatedDefinition.midiBinding = {};
            updatedDefinition.midiBindingHint.clear();
            applyDefinitionChange(updatedDefinition);
        };

        moveLeftButton.onClick = [this]
        {
            if (moveRequested != nullptr)
                moveRequested(controllerIndex, controllerIndex - 1);
        };

        moveRightButton.onClick = [this]
        {
            if (moveRequested != nullptr)
                moveRequested(controllerIndex, controllerIndex + 1);
        };

        deleteButton.onClick = [this]
        {
            if (deleteRequested != nullptr)
                deleteRequested(controllerIndex);
        };

        for (auto* component : std::initializer_list<juce::Component*>
             { &labelLabel, &codePrefixLabel, &valuePreviewLabel, &runtimeLabel,
               &labelEditor, &codeNameEditor, &typeButton, &midiButton, &learnButton, &clearButton,
               &moveLeftButton, &moveRightButton, &deleteButton,
               &knobSlider, &momentaryButton, &toggleButton })
        {
            addAndMakeVisible(component);
        }
    }

    ~ControllerTileComponent() override
    {
        stopTimer();
    }

    void updateFromState(int index,
                         int totalCount,
                         const UserDspControllerDefinition& controllerDefinition,
                         ControlsDisplayMode nextMode,
                         bool isRuntimeLinked,
                         float displayValue,
                         bool compileRequired,
                         bool isMidiLearnArmed,
                         bool hasMidiInput)
    {
        controllerIndex = index;
        definition = controllerDefinition;
        displayMode = nextMode;
        runtimeLinked = isRuntimeLinked;
        layoutStale = compileRequired;
        learnArmed = isMidiLearnArmed;
        midiInputAvailable = hasMidiInput;
        currentDisplayValue = displayValue;

        if (! textEditorsDirty && ! labelEditor.hasKeyboardFocus(true))
            setEditorTextSilently(labelEditor, definition.label);

        if (! textEditorsDirty && ! codeNameEditor.hasKeyboardFocus(true))
            setEditorTextSilently(codeNameEditor, definition.codeName);

        labelLabel.setText(definition.label, juce::dontSendNotification);
        moveLeftButton.setEnabled(index > 0);
        moveRightButton.setEnabled(index + 1 < totalCount);

        suppressValueCallbacks = true;

        if (definition.type == UserDspControllerType::knob && ! knobSlider.isValueEditorActive())
            knobSlider.setValue(displayValue, juce::dontSendNotification);

        if (definition.type == UserDspControllerType::button)
            momentaryButton.setMomentaryState(displayValue >= 0.5f, false);

        if (definition.type == UserDspControllerType::toggle)
            toggleButton.setLatchedState(displayValue >= 0.5f);

        suppressValueCallbacks = false;

        updateVisibleControls();
        updateInlineTexts();
        resized();
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(1.0f);
        auto fillColour = runtimeLinked ? ide::panel.brighter(0.03f) : ide::panel;

        if (layoutStale)
            fillColour = fillColour.interpolatedWith(ide::warning, 0.08f);

        g.setColour(fillColour);
        g.fillRoundedRectangle(bounds, 12.0f);
        g.setColour(runtimeLinked ? ide::border.brighter(0.18f) : ide::border);
        g.drawRoundedRectangle(bounds, 12.0f, 1.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(9);

        if (displayMode == ControlsDisplayMode::edit)
        {
            auto headerRow = area.removeFromTop(24);
            auto actions = headerRow.removeFromRight(82);
            labelEditor.setBounds(headerRow);
            moveLeftButton.setBounds(actions.removeFromLeft(22).reduced(1, 0));
            actions.removeFromLeft(4);
            moveRightButton.setBounds(actions.removeFromLeft(22).reduced(1, 0));
            actions.removeFromLeft(4);
            deleteButton.setBounds(actions.reduced(1, 0));

            area.removeFromTop(6);
            auto codeRow = area.removeFromTop(22);
            codePrefixLabel.setBounds(codeRow.removeFromLeft(50));
            codeNameEditor.setBounds(codeRow);

            area.removeFromTop(6);
            typeButton.setBounds(area.removeFromTop(24));
            area.removeFromTop(4);
            midiButton.setBounds(area.removeFromTop(24));
            area.removeFromTop(6);

            auto midiActions = area.removeFromTop(24);
            learnButton.setBounds(midiActions.removeFromLeft((midiActions.getWidth() - 6) / 2));
            midiActions.removeFromLeft(6);
            clearButton.setBounds(midiActions);

            area.removeFromTop(8);
            valuePreviewLabel.setBounds(area.removeFromTop(24));
            area.removeFromTop(6);
            runtimeLabel.setBounds(area.removeFromTop(18));

            labelLabel.setBounds({});
            knobSlider.setBounds({});
            momentaryButton.setBounds({});
            toggleButton.setBounds({});
            return;
        }

        auto headerRow = area.removeFromTop(24);
        labelLabel.setBounds(headerRow);

        area.removeFromTop(2);
        auto runtimeArea = area.removeFromBottom(18);
        auto widgetArea = area.reduced(2, 0);

        knobSlider.setBounds(widgetArea);
        momentaryButton.setBounds(widgetArea.withSizeKeepingCentre(widgetArea.getWidth(), 44).translated(0, 10));
        toggleButton.setBounds(widgetArea.withSizeKeepingCentre(widgetArea.getWidth(), 44).translated(0, 10));
        runtimeLabel.setBounds(runtimeArea);

        labelEditor.setBounds({});
        codePrefixLabel.setBounds({});
        codeNameEditor.setBounds({});
        typeButton.setBounds({});
        midiButton.setBounds({});
        learnButton.setBounds({});
        clearButton.setBounds({});
        moveLeftButton.setBounds({});
        moveRightButton.setBounds({});
        deleteButton.setBounds({});
        valuePreviewLabel.setBounds({});
    }

private:
    void timerCallback() override
    {
        stopTimer();
        commitPendingTextEdits();
    }

    void scheduleTextCommit()
    {
        if (suppressEditorCallbacks)
            return;

        textEditorsDirty = true;
        pendingTextCommit = true;
        localStatusOverride.clear();
        localStatusIsError = false;
        startTimer(textCommitDelayMs);
        updateInlineTexts();
    }

    void commitTextEditorsNow()
    {
        if (! textEditorsDirty)
            return;

        stopTimer();
        commitPendingTextEdits();
    }

    void commitPendingTextEdits()
    {
        pendingTextCommit = false;
        auto updatedDefinition = definition;

        const auto labelText = labelEditor.getText().trim();
        const auto rawCodeName = codeNameEditor.getText();
        auto sanitisedCodeName = userdsp::sanitiseControllerCodeName(rawCodeName);

        if (labelText.isEmpty())
        {
            localStatusOverride = "Controller name is required.";
            localStatusIsError = true;
            updateInlineTexts();
            return;
        }

        if (sanitisedCodeName.isEmpty() && codeNameEditor.hasKeyboardFocus(true))
        {
            localStatusOverride = "DSP name is required.";
            localStatusIsError = true;
            updateInlineTexts();
            return;
        }

        if (sanitisedCodeName.isEmpty())
            sanitisedCodeName = userdsp::sanitiseControllerCodeName(labelText);

        updatedDefinition.label = labelText;
        updatedDefinition.codeName = sanitisedCodeName;
        applyDefinitionChange(updatedDefinition);
    }

    void revertTextEditorsToDefinition()
    {
        stopTimer();
        pendingTextCommit = false;
        textEditorsDirty = false;
        localStatusOverride.clear();
        localStatusIsError = false;
        setEditorTextSilently(labelEditor, definition.label);
        setEditorTextSilently(codeNameEditor, definition.codeName);
        updateInlineTexts();
    }

    void setEditorTextSilently(juce::TextEditor& editor, const juce::String& text)
    {
        const juce::ScopedValueSetter<bool> scopedSetter(suppressEditorCallbacks, true);
        editor.setText(text, false);
    }

    void updateVisibleControls()
    {
        const auto isEditMode = displayMode == ControlsDisplayMode::edit;

        labelLabel.setVisible(! isEditMode);
        labelEditor.setVisible(isEditMode);
        codePrefixLabel.setVisible(isEditMode);
        codeNameEditor.setVisible(isEditMode);
        typeButton.setVisible(isEditMode);
        midiButton.setVisible(isEditMode);
        learnButton.setVisible(isEditMode);
        clearButton.setVisible(isEditMode);
        moveLeftButton.setVisible(isEditMode);
        moveRightButton.setVisible(isEditMode);
        deleteButton.setVisible(isEditMode);
        valuePreviewLabel.setVisible(isEditMode);

        knobSlider.setVisible(! isEditMode && definition.type == UserDspControllerType::knob);
        momentaryButton.setVisible(! isEditMode && definition.type == UserDspControllerType::button);
        toggleButton.setVisible(! isEditMode && definition.type == UserDspControllerType::toggle);

        knobSlider.setEnabled(! isEditMode);
        momentaryButton.setEnabled(! isEditMode);
        toggleButton.setEnabled(! isEditMode);
    }

    void updateInlineTexts()
    {
        typeButton.setButtonText("Type: " + userDspControllerTypeToDisplayName(definition.type));

        const auto midiSummary = buildDisplayedMidiSummary(definition);
        midiButton.setButtonText(midiSummary.isNotEmpty() ? ("MIDI: " + midiSummary)
                                                          : "MIDI: Not assigned");
        valuePreviewLabel.setText(buildEditValuePreview(definition.type, currentDisplayValue), juce::dontSendNotification);
        learnButton.setButtonText(learnArmed ? "Cancel" : "Learn");
        learnButton.setEnabled(midiInputAvailable || learnArmed);
        clearButton.setEnabled(midi::isBindingActive(definition.midiBinding) || definition.midiBindingHint.isNotEmpty());

        juce::String statusText;
        juce::Colour statusColour = ide::textMuted;

        if (localStatusOverride.isNotEmpty())
        {
            statusText = localStatusOverride;
            statusColour = localStatusIsError ? ide::warning : ide::textSecondary;
        }
        else if (displayMode == ControlsDisplayMode::edit && learnArmed)
        {
            statusText = midiInputAvailable ? "Waiting for MIDI..." : "No MIDI input";
            statusColour = midiInputAvailable ? ide::warning : ide::textMuted;
        }
        else
        {
            statusText = runtimeLinked ? "Linked to DSP" : (layoutStale ? "Compile to relink" : "Preview only");
            statusColour = runtimeLinked ? ide::success : (layoutStale ? ide::warning : ide::textMuted);
        }

        runtimeLabel.setText(statusText, juce::dontSendNotification);
        runtimeLabel.setColour(juce::Label::textColourId, statusColour);
    }

    void showTypeMenu()
    {
        juce::PopupMenu menu;
        menu.addItem(typeMenuKnobId, "Knob", true, definition.type == UserDspControllerType::knob);
        menu.addItem(typeMenuButtonId, "Button", true, definition.type == UserDspControllerType::button);
        menu.addItem(typeMenuToggleId, "Toggle", true, definition.type == UserDspControllerType::toggle);

        auto safeThis = juce::Component::SafePointer<ControllerTileComponent>(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&typeButton),
                           [safeThis] (int result)
                           {
                               if (safeThis == nullptr || result == 0)
                                   return;

                               auto updatedDefinition = safeThis->definition;
                               updatedDefinition.type = menuIdToControllerType(result);
                               safeThis->applyDefinitionChange(updatedDefinition);
                           });
    }

    void showMidiMenu()
    {
        juce::PopupMenu menu;
        juce::PopupMenu sourceMenu;

        const auto currentBinding = midi::isBindingActive(definition.midiBinding)
                                      ? midi::sanitiseMidiBinding(definition.midiBinding)
                                      : MidiBinding {};

        sourceMenu.addItem(midiSourceBaseId + 1, "Not assigned", true, ! midi::isBindingActive(currentBinding));
        sourceMenu.addItem(midiSourceBaseId + 2, "CC", true, currentBinding.source == MidiBindingSource::cc);
        sourceMenu.addItem(midiSourceBaseId + 3, "Note Gate", true, currentBinding.source == MidiBindingSource::noteGate);
        sourceMenu.addItem(midiSourceBaseId + 4, "Note Velocity", true, currentBinding.source == MidiBindingSource::noteVelocity);
        sourceMenu.addItem(midiSourceBaseId + 5, "Note Number", true, currentBinding.source == MidiBindingSource::noteNumber);
        sourceMenu.addItem(midiSourceBaseId + 6, "Pitch Wheel", true, currentBinding.source == MidiBindingSource::pitchWheel);
        menu.addSubMenu("Source", sourceMenu);

        if (midi::isBindingActive(currentBinding))
        {
            juce::PopupMenu channelMenu;

            for (int channel = 1; channel <= 16; ++channel)
                channelMenu.addItem(midiChannelBaseId + channel, "Ch " + juce::String(channel), true, currentBinding.channel == channel);

            menu.addSubMenu("Channel", channelMenu);

            if (currentBinding.source != MidiBindingSource::pitchWheel)
            {
                juce::PopupMenu dataMenu;
                populateMidiDataMenu(dataMenu, currentBinding.source, currentBinding.data1);
                menu.addSubMenu(currentBinding.source == MidiBindingSource::cc ? "CC Number" : "Note", dataMenu);
            }

            menu.addSeparator();
            menu.addItem(midiClearId, "Clear MIDI");
        }

        auto safeThis = juce::Component::SafePointer<ControllerTileComponent>(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&midiButton),
                           [safeThis, currentBinding] (int result)
                           {
                               if (safeThis == nullptr || result == 0)
                                   return;

                               auto updatedDefinition = safeThis->definition;

                               if (result >= midiSourceBaseId + 1 && result <= midiSourceBaseId + 6)
                               {
                                   updatedDefinition.midiBinding = sourceMenuIdToBinding(result, currentBinding);
                                   updatedDefinition.midiBindingHint.clear();
                                   safeThis->applyDefinitionChange(updatedDefinition);
                                   return;
                               }

                               if (result >= midiChannelBaseId + 1 && result <= midiChannelBaseId + 16)
                               {
                                   updatedDefinition.midiBinding = currentBinding;
                                   updatedDefinition.midiBinding.channel = result - midiChannelBaseId;
                                   safeThis->applyDefinitionChange(updatedDefinition);
                                   return;
                               }

                               if (result >= midiDataBaseId + 1 && result <= midiDataBaseId + 128)
                               {
                                   updatedDefinition.midiBinding = currentBinding;
                                   updatedDefinition.midiBinding.data1 = result - midiDataBaseId - 1;
                                   safeThis->applyDefinitionChange(updatedDefinition);
                                   return;
                               }

                               if (result == midiClearId)
                               {
                                   updatedDefinition.midiBinding = {};
                                   updatedDefinition.midiBindingHint.clear();
                                   safeThis->applyDefinitionChange(updatedDefinition);
                               }
                           });
    }

    void applyDefinitionChange(UserDspControllerDefinition updatedDefinition)
    {
        updatedDefinition.label = updatedDefinition.label.trim();
        updatedDefinition.codeName = userdsp::sanitiseControllerCodeName(updatedDefinition.codeName);
        updatedDefinition.midiBinding = midi::sanitiseMidiBinding(updatedDefinition.midiBinding);
        updatedDefinition.midiBindingHint = midi::isBindingActive(updatedDefinition.midiBinding)
                                              ? midi::buildMidiBindingSummary(updatedDefinition.midiBinding)
                                              : juce::String {};

        if (definitionChanged == nullptr)
            return;

        const auto result = definitionChanged(controllerIndex, updatedDefinition);

        if (result.wasOk())
        {
            definition = updatedDefinition;
            textEditorsDirty = false;
            pendingTextCommit = false;
            localStatusOverride.clear();
            localStatusIsError = false;
            stopTimer();
        }
        else
        {
            localStatusOverride = result.getErrorMessage();
            localStatusIsError = true;
        }

        updateInlineTexts();
        repaint();
    }

    std::function<juce::Result(int, const UserDspControllerDefinition&)> definitionChanged;
    std::function<void(int, MidiBindingSource)> midiLearnRequested;
    std::function<void(int)> midiLearnCancelled;
    std::function<void(int, int)> moveRequested;
    std::function<void(int)> deleteRequested;
    std::function<void(int, float)> valueChanged;

    int controllerIndex = -1;
    ControlsDisplayMode displayMode = ControlsDisplayMode::play;
    bool runtimeLinked = false;
    bool layoutStale = false;
    bool learnArmed = false;
    bool midiInputAvailable = false;
    bool suppressValueCallbacks = false;
    bool suppressEditorCallbacks = false;
    bool textEditorsDirty = false;
    bool pendingTextCommit = false;
    bool localStatusIsError = false;
    float currentDisplayValue = 0.0f;
    juce::String localStatusOverride;
    UserDspControllerDefinition definition;

    juce::Label labelLabel;
    juce::Label codePrefixLabel;
    juce::Label valuePreviewLabel;
    juce::Label runtimeLabel;
    ScrollPassthroughTextEditor labelEditor;
    ScrollPassthroughTextEditor codeNameEditor;
    ActionTextButton typeButton;
    ActionTextButton midiButton;
    ActionTextButton learnButton;
    ActionTextButton clearButton;
    ActionTextButton moveLeftButton;
    ActionTextButton moveRightButton;
    ActionTextButton deleteButton;
    ForwardingSlider knobSlider;
    MomentaryTextButton momentaryButton;
    ToggleTextButton toggleButton;
};

RealtimeControlsComponent::RealtimeControlsComponent(AudioEngine& audioEngineToControl,
                                                     UserDspProjectManager& projectManagerToEdit)
    : audioEngine(audioEngineToControl),
      projectManager(projectManagerToEdit)
{
    configureSectionLabel(titleLabel, "Controls");
    configureBodyLabel(statusLabel, {}, juce::Justification::topLeft, 14.0f, false);
    configureBodyLabel(hintLabel, {}, juce::Justification::topLeft, 12.5f, false);
    configureBodyLabel(modeLabel, "Mode", juce::Justification::centredLeft, 13.0f, true);
    configureBodyLabel(midiModeHintLabel,
                       "Edit mode changes controller metadata inline on each card. "
                       "Play mode is for performance only.",
                       juce::Justification::topLeft,
                       12.0f,
                       false);

    configureTextButton(addKnobButton);
    configureTextButton(addButtonButton);
    configureTextButton(addToggleButton);
    configureTextButton(editModeButton);
    configureTextButton(playModeButton);

    editModeButton.setClickingTogglesState(true);
    playModeButton.setClickingTogglesState(true);

    addKnobButton.onClick = [this] { addController(UserDspControllerType::knob); };
    addButtonButton.onClick = [this] { addController(UserDspControllerType::button); };
    addToggleButton.onClick = [this] { addController(UserDspControllerType::toggle); };
    editModeButton.onClick = [this] { setMode(Mode::edit); };
    playModeButton.onClick = [this] { setMode(Mode::play); };

    tilesViewport.setViewedComponentWithMouseWheelPassthrough(&tilesContent, false);
    tilesViewport.setScrollBarsShown(true, false);
    tilesViewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::never);

    for (auto* component : std::initializer_list<juce::Component*>
         { &titleLabel, &statusLabel, &hintLabel, &modeLabel, &midiModeHintLabel,
           &editModeButton, &playModeButton, &addKnobButton, &addButtonButton, &addToggleButton, &tilesViewport })
    {
        addAndMakeVisible(component);
    }

    setMode(Mode::play);
    syncToProjectAndRuntime();
    startTimerHz(20);
}

RealtimeControlsComponent::~RealtimeControlsComponent() = default;

void RealtimeControlsComponent::setRefreshSuspended(bool shouldSuspend)
{
    if (shouldSuspend)
    {
        stopTimer();
        return;
    }

    syncToProjectAndRuntime();
    startTimerHz(20);
}

void RealtimeControlsComponent::paint(juce::Graphics& g)
{
    g.fillAll(ide::background);

    auto panelBounds = getLocalBounds().toFloat().reduced(10.0f);
    g.setColour(ide::panel);
    g.fillRoundedRectangle(panelBounds, 12.0f);
    g.setColour(ide::border);
    g.drawRoundedRectangle(panelBounds, 12.0f, 1.0f);
}

void RealtimeControlsComponent::resized()
{
    auto area = getLocalBounds().reduced(18, 16);

    titleLabel.setBounds(area.removeFromTop(24));
    area.removeFromTop(4);
    statusLabel.setBounds(area.removeFromTop(34));
    area.removeFromTop(2);
    hintLabel.setBounds(area.removeFromTop(32));
    area.removeFromTop(8);

    auto toolbarRow = area.removeFromTop(30);
    modeLabel.setBounds(toolbarRow.removeFromLeft(44));
    toolbarRow.removeFromLeft(6);
    editModeButton.setBounds(toolbarRow.removeFromLeft(72));
    toolbarRow.removeFromLeft(6);
    playModeButton.setBounds(toolbarRow.removeFromLeft(72));
    toolbarRow.removeFromLeft(12);

    if (mode == Mode::edit)
    {
        addKnobButton.setBounds(toolbarRow.removeFromLeft(108));
        toolbarRow.removeFromLeft(6);
        addButtonButton.setBounds(toolbarRow.removeFromLeft(112));
        toolbarRow.removeFromLeft(6);
        addToggleButton.setBounds(toolbarRow.removeFromLeft(112));
        area.removeFromTop(6);
        midiModeHintLabel.setBounds(area.removeFromTop(30));
        area.removeFromTop(8);
    }
    else
    {
        addKnobButton.setBounds({});
        addButtonButton.setBounds({});
        addToggleButton.setBounds({});
        midiModeHintLabel.setBounds({});
        area.removeFromTop(8);
    }

    tilesViewport.setBounds(area);
    updateTileLayout();
}

void RealtimeControlsComponent::timerCallback()
{
    handleMidiLearnCapture();
    syncToProjectAndRuntime();
}

void RealtimeControlsComponent::setMode(Mode nextMode)
{
    if (mode != nextMode)
    {
        audioEngine.cancelMidiLearn();
        midiLearnControllerIndex = -1;
    }

    mode = nextMode;
    const auto isEditMode = mode == Mode::edit;

    editModeButton.setToggleState(isEditMode, juce::dontSendNotification);
    playModeButton.setToggleState(! isEditMode, juce::dontSendNotification);
    addKnobButton.setVisible(isEditMode);
    addButtonButton.setVisible(isEditMode);
    addToggleButton.setVisible(isEditMode);
    midiModeHintLabel.setVisible(isEditMode);

    hintLabel.setText(isEditMode
                          ? "Edit mode changes each controller inline inside its card. Changes save automatically."
                          : "Play mode is for performance. Controller widgets only send values to DSP.",
                      juce::dontSendNotification);

    syncToProjectAndRuntime();
    resized();
    repaint();
}

void RealtimeControlsComponent::addController(UserDspControllerType type)
{
    audioEngine.cancelMidiLearn();
    midiLearnControllerIndex = -1;

    if (const auto result = projectManager.addController(type); result.failed())
    {
        showErrorMessage("Add Controller Failed", result.getErrorMessage());
        return;
    }

    syncToProjectAndRuntime();
}

void RealtimeControlsComponent::deleteController(int index)
{
    const auto& definitions = projectManager.getControllerDefinitions();

    if (! juce::isPositiveAndBelow(index, static_cast<int>(definitions.size())))
        return;

    if (! showDeleteDialog(definitions[static_cast<std::size_t>(index)].label, &getLookAndFeel()))
        return;

    audioEngine.cancelMidiLearn();
    midiLearnControllerIndex = -1;

    if (const auto result = projectManager.removeController(index); result.failed())
    {
        showErrorMessage("Delete Controller Failed", result.getErrorMessage());
        return;
    }

    syncToProjectAndRuntime();
}

void RealtimeControlsComponent::moveController(int sourceIndex, int destinationIndex)
{
    audioEngine.cancelMidiLearn();
    midiLearnControllerIndex = -1;

    if (const auto result = projectManager.moveController(sourceIndex, destinationIndex); result.failed())
    {
        showErrorMessage("Move Controller Failed", result.getErrorMessage());
        return;
    }

    syncToProjectAndRuntime();
}

void RealtimeControlsComponent::applyControllerValue(int index, float value)
{
    if (mode != Mode::play)
        return;

    if (! juce::isPositiveAndBelow(index, static_cast<int>(previewValues.size())))
        return;

    const auto clampedValue = juce::jlimit(0.0f, 1.0f, value);
    previewValues[static_cast<std::size_t>(index)] = clampedValue;

    if (layoutMatchesRuntime)
        audioEngine.setUserControlValue(index, clampedValue);
    else
        previewValuesNeedPush = true;
}

juce::Result RealtimeControlsComponent::updateControllerDefinition(int index,
                                                                   const UserDspControllerDefinition& definition,
                                                                   bool showFailureDialog)
{
    const auto result = projectManager.updateController(index, definition);

    if (result.failed())
    {
        if (showFailureDialog)
            showErrorMessage("Update Controller Failed", result.getErrorMessage());

        return result;
    }

    syncToProjectAndRuntime();
    return result;
}

void RealtimeControlsComponent::armMidiLearnForController(int index, MidiBindingSource preferredSource)
{
    if (! juce::isPositiveAndBelow(index, static_cast<int>(projectManager.getControllerDefinitions().size())))
        return;

    midiLearnControllerIndex = index;
    audioEngine.armMidiLearn(index, preferredSource);
    syncToProjectAndRuntime();
}

void RealtimeControlsComponent::cancelMidiLearnForController(int index)
{
    if (midiLearnControllerIndex != index)
        return;

    audioEngine.cancelMidiLearn();
    midiLearnControllerIndex = -1;
    syncToProjectAndRuntime();
}

void RealtimeControlsComponent::handleMidiLearnCapture()
{
    MidiLearnCapture capture;

    if (! audioEngine.consumeMidiLearnCapture(capture))
        return;

    midiLearnControllerIndex = -1;
    const auto& definitions = projectManager.getControllerDefinitions();

    if (! juce::isPositiveAndBelow(capture.controlIndex, static_cast<int>(definitions.size())))
        return;

    auto updatedDefinition = definitions[static_cast<std::size_t>(capture.controlIndex)];
    updatedDefinition.midiBinding = capture.binding;
    updatedDefinition.midiBindingHint = midi::buildMidiBindingSummary(capture.binding);

    if (const auto result = updateControllerDefinition(capture.controlIndex, updatedDefinition, false); result.failed())
        showErrorMessage("MIDI Learn Failed", result.getErrorMessage());
}

void RealtimeControlsComponent::syncToProjectAndRuntime()
{
    const auto& definitionsRef = projectManager.getControllerDefinitions();
    std::vector<UserDspControllerDefinition> definitions(definitionsRef.begin(), definitionsRef.end());
    const auto snapshot = audioEngine.getUserDspHost().getSnapshot();

    if (definitions != lastDefinitions)
    {
        previewValues = remapPreviewValues(lastDefinitions, previewValues, definitions);
        lastDefinitions = definitions;
        lastMidiPreviewGenerations.assign(lastDefinitions.size(), 0);
        previewValuesNeedPush = ! previewValues.empty();

        if (tiles.size() != lastDefinitions.size())
            rebuildTiles();
    }

    if (tiles.size() != lastDefinitions.size())
        rebuildTiles();

    layoutMatchesRuntime = definitionsMatchSnapshot(lastDefinitions, snapshot);

    if (layoutMatchesRuntime && previewValuesNeedPush)
    {
        for (int index = 0; index < static_cast<int>(previewValues.size()); ++index)
            audioEngine.setUserControlValue(index, previewValues[static_cast<std::size_t>(index)]);

        previewValuesNeedPush = false;
    }

    juce::String statusText;
    const auto isEditMode = mode == Mode::edit;

    if (lastDefinitions.empty())
    {
        statusText = isEditMode
                         ? "No custom controls yet. Add knobs, buttons, or toggles for your DSP project."
                         : "No custom controls yet. Switch to Edit mode to add them.";
    }
    else if (! snapshot.hasActiveModule)
    {
        statusText = isEditMode
                         ? "Edit mode is active. Compile the project to link "
                           + juce::String(lastDefinitions.size())
                           + " control" + (lastDefinitions.size() == 1 ? "" : "s") + " to DSP."
                         : "Play mode is active, but the current controller layout is only a preview. Compile to link it to DSP.";
    }
    else if (! layoutMatchesRuntime)
    {
        statusText = isEditMode
                         ? "Edit mode is active. Controller layout changed since the last compile, so compile again to relink DSP."
                         : "Play mode is active, but controller metadata changed. Compile again before using the live layout.";
    }
    else
    {
        statusText = isEditMode
                         ? "Edit mode is active. Metadata edits stay inline and save automatically."
                         : "Loaded module: " + snapshot.processorName + ". Runtime controls are live.";
    }

    statusLabel.setText(statusText, juce::dontSendNotification);

    const auto audioSnapshot = audioEngine.getSnapshot();
    const auto hasMidiInput = ! audioSnapshot.availableMidiInputDevices.isEmpty();

    for (int index = 0; index < static_cast<int>(tiles.size()); ++index)
    {
        auto displayValue = index < static_cast<int>(previewValues.size())
                                ? previewValues[static_cast<std::size_t>(index)]
                                : 0.0f;

        const auto previewGeneration = audioEngine.getPreviewControlGeneration(index);

        if (index < static_cast<int>(lastMidiPreviewGenerations.size())
            && previewGeneration != lastMidiPreviewGenerations[static_cast<std::size_t>(index)])
        {
            lastMidiPreviewGenerations[static_cast<std::size_t>(index)] = previewGeneration;

            if (! layoutMatchesRuntime)
            {
                displayValue = audioEngine.getPreviewControlValue(index);
                previewValues[static_cast<std::size_t>(index)] = displayValue;
                previewValuesNeedPush = true;
            }
        }

        if (layoutMatchesRuntime && index < snapshot.controlCount)
        {
            displayValue = snapshot.controls[static_cast<std::size_t>(index)].currentValue;
            previewValues[static_cast<std::size_t>(index)] = displayValue;
        }

        tiles[static_cast<std::size_t>(index)]->updateFromState(index,
                                                                static_cast<int>(tiles.size()),
                                                                lastDefinitions[static_cast<std::size_t>(index)],
                                                                mode == Mode::edit ? ControlsDisplayMode::edit : ControlsDisplayMode::play,
                                                                layoutMatchesRuntime,
                                                                displayValue,
                                                                ! layoutMatchesRuntime && snapshot.hasActiveModule,
                                                                midiLearnControllerIndex == index,
                                                                hasMidiInput);
    }

    updateTileLayout();
}

void RealtimeControlsComponent::rebuildTiles()
{
    tiles.clear();

    for (int index = tilesContent.getNumChildComponents() - 1; index >= 0; --index)
        tilesContent.removeChildComponent(index);

    tiles.reserve(lastDefinitions.size());

    for (std::size_t index = 0; index < lastDefinitions.size(); ++index)
    {
        auto tile = std::make_unique<ControllerTileComponent>(
            [this] (int controllerIndex, const UserDspControllerDefinition& definition)
            {
                return updateControllerDefinition(controllerIndex, definition, false);
            },
            [this] (int controllerIndex, MidiBindingSource preferredSource)
            {
                armMidiLearnForController(controllerIndex, preferredSource);
            },
            [this] (int controllerIndex)
            {
                cancelMidiLearnForController(controllerIndex);
            },
            [this] (int sourceIndex, int destinationIndex) { moveController(sourceIndex, destinationIndex); },
            [this] (int controllerIndex) { deleteController(controllerIndex); },
            [this] (int controllerIndex, float value) { applyControllerValue(controllerIndex, value); });

        tilesContent.addAndMakeVisible(tile.get());
        tiles.push_back(std::move(tile));
    }

    updateTileLayout();
}

void RealtimeControlsComponent::updateTileLayout()
{
    const auto viewportBounds = tilesViewport.getLocalBounds();

    if (viewportBounds.isEmpty())
        return;

    const int contentWidth = juce::jmax(viewportBounds.getWidth() - 6, tileWidth + tileGap * 2);
    const int columns = juce::jmax(1, (contentWidth - tileGap) / (tileWidth + tileGap));
    const int usedWidth = columns * tileWidth + (columns - 1) * tileGap;
    const int leftInset = juce::jmax(6, (contentWidth - usedWidth) / 2);

    for (int index = 0; index < static_cast<int>(tiles.size()); ++index)
    {
        const int column = index % columns;
        const int row = index / columns;
        const int x = leftInset + column * (tileWidth + tileGap);
        const int y = tileGap + row * (tileHeight + tileGap);
        tiles[static_cast<std::size_t>(index)]->setBounds(x, y, tileWidth, tileHeight);
    }

    const int rows = tiles.empty() ? 1 : ((static_cast<int>(tiles.size()) + columns - 1) / columns);
    const int contentHeight = juce::jmax(viewportBounds.getHeight(),
                                         tileGap + rows * tileHeight + juce::jmax(0, rows - 1) * tileGap + tileGap);

    tilesContent.setSize(contentWidth, contentHeight);
}

void RealtimeControlsComponent::showErrorMessage(const juce::String& title, const juce::String& message)
{
    juce::AlertWindow window(title, message, juce::MessageBoxIconType::WarningIcon);
    window.setLookAndFeel(&getLookAndFeel());
    window.addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window.runModalLoop();
}
