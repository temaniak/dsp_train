#include "ui/RealtimeControlsComponent.h"

#include "ui/DarkIdeLookAndFeel.h"
#include "ui/ScrollPassthroughControls.h"
#include "userdsp/UserDspProjectUtils.h"

namespace
{
constexpr int tileWidth = 172;
constexpr int tileHeight = 206;
constexpr int tileGap = 10;

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

void configureAlertWindowLookAndFeel(juce::AlertWindow& window, juce::LookAndFeel* lookAndFeel)
{
    if (lookAndFeel != nullptr)
        window.setLookAndFeel(lookAndFeel);
}

void configureAlertTextEditor(juce::TextEditor* editor)
{
    if (editor == nullptr)
        return;

    editor->setColour(juce::TextEditor::backgroundColourId, ide::active);
    editor->setColour(juce::TextEditor::textColourId, ide::text);
    editor->setColour(juce::TextEditor::highlightColourId, ide::border);
    editor->setColour(juce::TextEditor::highlightedTextColourId, ide::text);
    editor->setColour(juce::TextEditor::outlineColourId, ide::border);
    editor->setColour(juce::TextEditor::focusedOutlineColourId, ide::constant);
}

void configureAlertComboBox(juce::ComboBox* combo)
{
    if (combo == nullptr)
        return;

    combo->setScrollWheelEnabled(false);
    combo->setColour(juce::ComboBox::backgroundColourId, ide::active);
    combo->setColour(juce::ComboBox::textColourId, ide::text);
    combo->setColour(juce::ComboBox::outlineColourId, ide::border);
    combo->setColour(juce::ComboBox::buttonColourId, ide::active);
    combo->setColour(juce::ComboBox::arrowColourId, ide::textSecondary);
    combo->setColour(juce::ComboBox::focusedOutlineColourId, ide::constant);
}

int controllerTypeToComboId(UserDspControllerType type)
{
    switch (type)
    {
        case UserDspControllerType::knob:   return 1;
        case UserDspControllerType::button: return 2;
        case UserDspControllerType::toggle: return 3;
    }

    return 1;
}

UserDspControllerType comboIdToControllerType(int comboId)
{
    switch (comboId)
    {
        case 2: return UserDspControllerType::button;
        case 3: return UserDspControllerType::toggle;
        default: return UserDspControllerType::knob;
    }
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

bool showControllerEditorDialog(UserDspControllerDefinition& definition, juce::LookAndFeel* lookAndFeel)
{
    juce::AlertWindow window("Edit Controller",
                             "Set the UI label, code name, controller type, and future MIDI mapping hint.",
                             juce::MessageBoxIconType::NoIcon);
    configureAlertWindowLookAndFeel(window, lookAndFeel);
    window.addTextEditor("label", definition.label, "Label");
    window.addTextEditor("codeName", definition.codeName, "Code Name");
    window.addTextEditor("midiBindingHint", definition.midiBindingHint, "MIDI Mapping Hint");
    configureAlertTextEditor(window.getTextEditor("label"));
    configureAlertTextEditor(window.getTextEditor("codeName"));
    configureAlertTextEditor(window.getTextEditor("midiBindingHint"));

    window.addComboBox("type", { "Knob", "Button", "Toggle" }, "Type");

    if (auto* combo = window.getComboBoxComponent("type"); combo != nullptr)
    {
        configureAlertComboBox(combo);
        combo->setSelectedId(controllerTypeToComboId(definition.type), juce::dontSendNotification);
    }

    window.addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window.addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    if (window.runModalLoop() != 1)
        return false;

    definition.label = window.getTextEditor("label")->getText().trim();
    definition.codeName = userdsp::sanitiseControllerCodeName(window.getTextEditor("codeName")->getText());
    definition.midiBindingHint = window.getTextEditor("midiBindingHint")->getText().trim();

    if (definition.codeName.isEmpty())
        definition.codeName = userdsp::sanitiseControllerCodeName(definition.label);

    if (auto* combo = window.getComboBoxComponent("type"); combo != nullptr)
        definition.type = comboIdToControllerType(combo->getSelectedId());

    return true;
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
    configureAlertWindowLookAndFeel(window, lookAndFeel);
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

class ControllerTileComponent final : public juce::Component
{
public:
    ControllerTileComponent(std::function<void(int)> onEdit,
                            std::function<void(int, int)> onMove,
                            std::function<void(int)> onDelete,
                            std::function<void(int, float)> onValueChanged)
        : editRequested(std::move(onEdit)),
          moveRequested(std::move(onMove)),
          deleteRequested(std::move(onDelete)),
          valueChanged(std::move(onValueChanged))
    {
        configureBodyLabel(labelLabel, {}, juce::Justification::centredLeft, 15.0f, true);
        labelLabel.setColour(juce::Label::textColourId, ide::text);
        configureBodyLabel(codeNameLabel, {}, juce::Justification::centredLeft, 12.5f, false);
        configureBodyLabel(typeLabel, {}, juce::Justification::centredLeft, 12.0f, false);
        configureBodyLabel(midiLabel, {}, juce::Justification::centredLeft, 12.0f, false);
        configureBodyLabel(runtimeLabel, {}, juce::Justification::centredLeft, 12.0f, false);

        configureTextButton(editButton);
        configureTextButton(moveLeftButton);
        configureTextButton(moveRightButton);
        configureTextButton(deleteButton);
        configureTextButton(momentaryButton);
        configureTextButton(toggleButton);

        editButton.setButtonText("...");
        moveLeftButton.setButtonText("<");
        moveRightButton.setButtonText(">");
        deleteButton.setButtonText("X");

        knobSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        knobSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 82, 24);
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

        editButton.onClick = [this]
        {
            if (editRequested != nullptr)
                editRequested(controllerIndex);
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
             { &labelLabel, &codeNameLabel, &typeLabel, &midiLabel, &runtimeLabel,
               &editButton, &moveLeftButton, &moveRightButton, &deleteButton, &knobSlider, &momentaryButton, &toggleButton })
        {
            addAndMakeVisible(component);
        }
    }

    void updateFromState(int index,
                         int totalCount,
                         const UserDspControllerDefinition& controllerDefinition,
                         ControlsDisplayMode nextMode,
                         bool isRuntimeLinked,
                         float displayValue,
                         bool compileRequired)
    {
        controllerIndex = index;
        definition = controllerDefinition;
        displayMode = nextMode;
        runtimeLinked = isRuntimeLinked;
        layoutStale = compileRequired;

        const auto isEditMode = displayMode == ControlsDisplayMode::edit;

        labelLabel.setText(definition.label, juce::dontSendNotification);
        codeNameLabel.setText("controls." + definition.codeName, juce::dontSendNotification);
        typeLabel.setText("Type: " + userDspControllerTypeToDisplayName(definition.type), juce::dontSendNotification);
        midiLabel.setText(definition.midiBindingHint.isNotEmpty() ? ("MIDI: " + definition.midiBindingHint)
                                                                  : "MIDI: not assigned yet",
                          juce::dontSendNotification);
        moveLeftButton.setEnabled(index > 0);
        moveRightButton.setEnabled(index + 1 < totalCount);
        editButton.setVisible(isEditMode);
        moveLeftButton.setVisible(isEditMode);
        moveRightButton.setVisible(isEditMode);
        deleteButton.setVisible(isEditMode);
        codeNameLabel.setVisible(isEditMode);
        typeLabel.setVisible(isEditMode);
        midiLabel.setVisible(isEditMode);

        runtimeLabel.setText(runtimeLinked ? "Linked to DSP" : (layoutStale ? "Compile to relink" : "Preview only"),
                             juce::dontSendNotification);
        runtimeLabel.setColour(juce::Label::textColourId,
                               runtimeLinked ? ide::success : (layoutStale ? ide::warning : ide::textMuted));

        suppressValueCallbacks = true;

        knobSlider.setVisible(definition.type == UserDspControllerType::knob);
        momentaryButton.setVisible(definition.type == UserDspControllerType::button);
        toggleButton.setVisible(definition.type == UserDspControllerType::toggle);

        knobSlider.setEnabled(! isEditMode);
        momentaryButton.setEnabled(! isEditMode);
        toggleButton.setEnabled(! isEditMode);

        if (definition.type == UserDspControllerType::knob && ! knobSlider.isValueEditorActive())
            knobSlider.setValue(displayValue, juce::dontSendNotification);

        if (definition.type == UserDspControllerType::button)
            momentaryButton.setMomentaryState(displayValue >= 0.5f, false);

        if (definition.type == UserDspControllerType::toggle)
            toggleButton.setLatchedState(displayValue >= 0.5f);

        suppressValueCallbacks = false;
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
        auto headerRow = area.removeFromTop(24);
        labelLabel.setBounds(headerRow.removeFromLeft(headerRow.getWidth() - 96));

        auto actions = headerRow.removeFromRight(96);
        editButton.setBounds(actions.removeFromLeft(24).reduced(1, 0));
        moveLeftButton.setBounds(actions.removeFromLeft(22).reduced(1, 0));
        moveRightButton.setBounds(actions.removeFromLeft(22).reduced(1, 0));
        deleteButton.setBounds(actions.reduced(1, 0));

        area.removeFromTop(2);
        codeNameLabel.setBounds(area.removeFromTop(18));
        area.removeFromTop(2);
        typeLabel.setBounds(area.removeFromTop(18));
        area.removeFromTop(2);
        midiLabel.setBounds(area.removeFromTop(18));
        area.removeFromTop(4);

        auto widgetArea = area.removeFromTop(110);
        knobSlider.setBounds(widgetArea);
        momentaryButton.setBounds(widgetArea.withSizeKeepingCentre(widgetArea.getWidth(), 40).translated(0, 8));
        toggleButton.setBounds(widgetArea.withSizeKeepingCentre(widgetArea.getWidth(), 40).translated(0, 8));

        area.removeFromTop(2);
        runtimeLabel.setBounds(area.removeFromTop(18));
    }

private:
    std::function<void(int)> editRequested;
    std::function<void(int, int)> moveRequested;
    std::function<void(int)> deleteRequested;
    std::function<void(int, float)> valueChanged;

    int controllerIndex = -1;
    ControlsDisplayMode displayMode = ControlsDisplayMode::play;
    bool runtimeLinked = false;
    bool layoutStale = false;
    bool suppressValueCallbacks = false;
    UserDspControllerDefinition definition;

    juce::Label labelLabel;
    juce::Label codeNameLabel;
    juce::Label typeLabel;
    juce::Label midiLabel;
    juce::Label runtimeLabel;
    ActionTextButton editButton;
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
                       "Edit mode stores labels, code names, and future MIDI mapping hints. "
                       "Play mode is for using controllers only.",
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
    syncToProjectAndRuntime();
}

void RealtimeControlsComponent::setMode(Mode nextMode)
{
    mode = nextMode;
    const auto isEditMode = mode == Mode::edit;

    editModeButton.setToggleState(isEditMode, juce::dontSendNotification);
    playModeButton.setToggleState(! isEditMode, juce::dontSendNotification);
    addKnobButton.setVisible(isEditMode);
    addButtonButton.setVisible(isEditMode);
    addToggleButton.setVisible(isEditMode);
    midiModeHintLabel.setVisible(isEditMode);

    hintLabel.setText(isEditMode
                          ? "Edit mode changes controller metadata and layout. Use the ... button on a tile to edit details."
                          : "Play mode is for performance. Controller widgets only send values to DSP and do not open editors.",
                      juce::dontSendNotification);

    syncToProjectAndRuntime();
    resized();
    repaint();
}

void RealtimeControlsComponent::addController(UserDspControllerType type)
{
    if (const auto result = projectManager.addController(type); result.failed())
    {
        showErrorMessage("Add Controller Failed", result.getErrorMessage());
        return;
    }

    const auto newIndex = static_cast<int>(projectManager.getControllerDefinitions().size()) - 1;
    editController(newIndex);
    syncToProjectAndRuntime();
}

void RealtimeControlsComponent::editController(int index)
{
    const auto& definitions = projectManager.getControllerDefinitions();

    if (! juce::isPositiveAndBelow(index, static_cast<int>(definitions.size())))
        return;

    auto updatedDefinition = definitions[static_cast<std::size_t>(index)];

    if (! showControllerEditorDialog(updatedDefinition, &getLookAndFeel()))
        return;

    if (const auto result = projectManager.updateController(index, updatedDefinition); result.failed())
    {
        showErrorMessage("Edit Controller Failed", result.getErrorMessage());
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

    if (const auto result = projectManager.removeController(index); result.failed())
    {
        showErrorMessage("Delete Controller Failed", result.getErrorMessage());
        return;
    }

    syncToProjectAndRuntime();
}

void RealtimeControlsComponent::moveController(int sourceIndex, int destinationIndex)
{
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

void RealtimeControlsComponent::syncToProjectAndRuntime()
{
    const auto& definitionsRef = projectManager.getControllerDefinitions();
    std::vector<UserDspControllerDefinition> definitions(definitionsRef.begin(), definitionsRef.end());
    const auto snapshot = audioEngine.getUserDspHost().getSnapshot();

    if (definitions != lastDefinitions)
    {
        previewValues = remapPreviewValues(lastDefinitions, previewValues, definitions);
        lastDefinitions = definitions;
        rebuildTiles();
        previewValuesNeedPush = ! previewValues.empty();
    }

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
                         ? "Edit mode is active. Runtime stays linked, but interaction is intentionally disabled while you change metadata."
                         : "Loaded module: " + snapshot.processorName + ". Runtime controls are live.";
    }

    statusLabel.setText(statusText, juce::dontSendNotification);

    for (int index = 0; index < static_cast<int>(tiles.size()); ++index)
    {
        auto displayValue = index < static_cast<int>(previewValues.size())
                                ? previewValues[static_cast<std::size_t>(index)]
                                : 0.0f;

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
                                                                ! layoutMatchesRuntime && snapshot.hasActiveModule);
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
            [this] (int controllerIndex) { editController(controllerIndex); },
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
    configureAlertWindowLookAndFeel(window, &getLookAndFeel());
    window.addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window.runModalLoop();
}
