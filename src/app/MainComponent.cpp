#include "app/MainComponent.h"

#include <cmath>

#include "ui/OscilloscopeComponent.h"
#include "ui/RealtimeControlsComponent.h"
#include "ui/ToolWindow.h"

namespace
{
constexpr int controlsWidth = 360;
constexpr int toolbarHeight = 48;
constexpr int splitterThickness = 8;
constexpr int panelToggleStripWidth = 24;
constexpr float panelAnimationRate = 0.22f;
constexpr float panelAnimationSnapDistance = 1.0f;
constexpr float navigatorMinimumWidth = 210.0f;

float animateTowards(float currentValue, float targetValue)
{
    if (std::abs(targetValue - currentValue) <= panelAnimationSnapDistance)
        return targetValue;

    return currentValue + (targetValue - currentValue) * panelAnimationRate;
}

void configureSectionLabel(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
    label.setJustificationType(juce::Justification::centredLeft);
    label.setColour(juce::Label::textColourId, ide::text);
}

void configureBodyLabel(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setColour(juce::Label::textColourId, ide::textSecondary);
}

void configureStatusLabel(juce::Label& label)
{
    label.setOpaque(true);
    label.setColour(juce::Label::backgroundColourId, ide::active);
    label.setColour(juce::Label::textColourId, ide::textSecondary);
    label.setJustificationType(juce::Justification::centredLeft);
    label.setBorderSize(juce::BorderSize<int>(0, 8, 0, 8));
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

    combo->setColour(juce::ComboBox::backgroundColourId, ide::active);
    combo->setColour(juce::ComboBox::textColourId, ide::text);
    combo->setColour(juce::ComboBox::outlineColourId, ide::border);
    combo->setColour(juce::ComboBox::buttonColourId, ide::active);
    combo->setColour(juce::ComboBox::arrowColourId, ide::textSecondary);
    combo->setColour(juce::ComboBox::focusedOutlineColourId, ide::constant);
}

juce::String parentPathFromRelativePath(const juce::String& relativePath)
{
    const auto normalised = relativePath.trim().replaceCharacter('\\', '/');
    const auto separator = normalised.lastIndexOfChar('/');

    if (separator < 0)
        return {};

    return normalised.substring(0, separator);
}
} // namespace

class ProjectTreeItem final : public juce::TreeViewItem
{
public:
    ProjectTreeItem(MainComponent& ownerToNotify,
                    const UserDspProjectManager& projectManagerToRead,
                    juce::String relativePathToRepresent)
        : owner(ownerToNotify),
          projectManager(projectManagerToRead),
          relativePath(std::move(relativePathToRepresent))
    {
    }

    bool mightContainSubItems() override
    {
        if (const auto* node = projectManager.getNodeForPath(relativePath); node != nullptr)
            return node->type == UserDspProjectNodeType::folder && ! node->children.empty();

        return false;
    }

    juce::String getUniqueName() const override
    {
        return relativePath.isEmpty() ? "__project_root__" : relativePath;
    }

    void paintItem(juce::Graphics& g, int width, int height) override
    {
        const auto* node = projectManager.getNodeForPath(relativePath);
        const auto isFolder = node == nullptr || node->type == UserDspProjectNodeType::folder;
        const auto text = relativePath.isEmpty() ? projectManager.getProjectName() : node->name;

        auto rowBounds = juce::Rectangle<float>(2.0f, 2.0f, static_cast<float>(juce::jmax(width - 4, 1)), static_cast<float>(juce::jmax(height - 4, 1)));

        if (isSelected())
        {
            g.setColour(ide::active);
            g.fillRoundedRectangle(rowBounds, 4.0f);
        }

        const auto accentColour = isFolder ? ide::type : ide::textMuted;
        g.setColour(accentColour);
        g.fillEllipse(10.0f, static_cast<float>(height / 2 - 3), 6.0f, 6.0f);

        g.setColour(ide::text);
        g.setFont(juce::Font(juce::FontOptions(isFolder ? 14.0f : 13.5f, isFolder ? juce::Font::bold : juce::Font::plain)));
        g.drawText(text, 24, 0, juce::jmax(width - 30, 40), height, juce::Justification::centredLeft, true);
    }

    void itemOpennessChanged(bool isNowOpen) override
    {
        if (isNowOpen)
            rebuildSubItems();
        else
            clearSubItems();
    }

    void itemSelectionChanged(bool isNowSelected) override
    {
        if (isNowSelected)
            owner.handleProjectNavigatorSelection(relativePath);
    }

    void itemClicked(const juce::MouseEvent& event) override
    {
        if (event.mods.isPopupMenu())
            owner.showProjectContextMenu(relativePath, event.getMouseDownScreenPosition());
    }

    ProjectTreeItem* findItemByPath(const juce::String& targetPath)
    {
        if (relativePath == targetPath)
            return this;

        if (const auto* node = projectManager.getNodeForPath(relativePath); node != nullptr
            && node->type == UserDspProjectNodeType::folder && getNumSubItems() == 0)
        {
            rebuildSubItems();
        }

        for (int index = 0; index < getNumSubItems(); ++index)
        {
            if (auto* item = dynamic_cast<ProjectTreeItem*>(getSubItem(index)); item != nullptr)
            {
                if (auto* match = item->findItemByPath(targetPath); match != nullptr)
                    return match;
            }
        }

        return nullptr;
    }

private:
    void rebuildSubItems()
    {
        clearSubItems();

        const auto* node = projectManager.getNodeForPath(relativePath);

        if (node == nullptr || node->type != UserDspProjectNodeType::folder)
            return;

        for (const auto& child : node->children)
            addSubItem(new ProjectTreeItem(owner, projectManager, relativePath.isEmpty() ? child->name : relativePath + "/" + child->name));
    }

    MainComponent& owner;
    const UserDspProjectManager& projectManager;
    juce::String relativePath;
};

class SidePanelResizeBar final : public juce::Component
{
public:
    explicit SidePanelResizeBar(MainComponent& ownerToNotify)
        : owner(ownerToNotify)
    {
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(ide::background);

        auto gripBounds = getLocalBounds().toFloat().reduced(1.5f, 8.0f);
        const auto gripColour = isMouseButtonDown() ? ide::constant
                              : isMouseOver()      ? ide::textSecondary
                                                   : ide::border;

        g.setColour(gripColour);
        g.fillRoundedRectangle(gripBounds, 3.0f);
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        dragStartWidth = owner.navigatorPanelExpandedWidth;
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        owner.resizeNavigatorPanel(dragStartWidth - static_cast<float>(event.getDistanceFromDragStartX()));
    }

private:
    MainComponent& owner;
    float dragStartWidth = 0.0f;
};

MainComponent::MainComponent()
    : compiler(audioEngine.getUserDspHost()),
      codeEditor(projectManager.getDocument(), &projectManager.getTokeniser())
{
    navigatorResizeBar = std::make_unique<SidePanelResizeBar>(*this);
    ideLookAndFeel = std::make_unique<ide::DarkIdeLookAndFeel>();
    setLookAndFeel(ideLookAndFeel.get());

    workspaceLayout.setItemLayout(0, 220.0, -0.85, -0.68);
    workspaceLayout.setItemLayout(1, splitterThickness, splitterThickness, splitterThickness);
    workspaceLayout.setItemLayout(2, 150.0, -0.55, -0.32);

    configureSectionLabel(sourceHeader, "Source");
    configureSectionLabel(transportHeader, "Transport");
    configureSectionLabel(wavHeader, "WAV Source");
    configureSectionLabel(dspHeader, "DSP Mode");
    configureSectionLabel(builtinHeader, "Built-in DSP");
    configureSectionLabel(userHeader, "User DSP Module");
    configureSectionLabel(toolsHeader, "Tools");
    configureSectionLabel(editorHeader, "Code Editor");
    configureSectionLabel(navigatorHeader, "Project Navigator");
    configureSectionLabel(logHeader, "Compile Log");

    configureBodyLabel(gainLabel, "Gain");
    configureBodyLabel(frequencyLabel, "Sine Frequency");
    configureBodyLabel(wavPositionLabel, "Position");
    configureBodyLabel(sampleRateLabel, "Sample Rate: --");
    configureBodyLabel(blockSizeLabel, "Block Size: --");
    configureBodyLabel(wavFileLabel, "No WAV loaded");
    configureBodyLabel(processorClassLabel, "Processor Class");
    configureBodyLabel(codeFontSizeLabel, "Code Font");
    configureBodyLabel(codeFontSizeValueLabel, {});
    configureBodyLabel(userModuleStatusLabel, {});
    userModuleStatusLabel.setJustificationType(juce::Justification::topLeft);
    configureStatusLabel(projectPathLabel);
    configureStatusLabel(compileStatusLabel);
    compileStatusLabel.setText("Idle", juce::dontSendNotification);

    configureSlider(gainSlider, 0.0, 1.0, 0.001, "x");
    configureSlider(frequencySlider, 20.0, 2000.0, 1.0, " Hz");
    configureSlider(wavPositionSlider, 0.0, 1.0, 0.0001);

    gainSlider.setValue(0.2, juce::dontSendNotification);
    frequencySlider.setValue(440.0, juce::dontSendNotification);

    codeFontSizeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    codeFontSizeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    codeFontSizeSlider.setRange(12.0, 24.0, 1.0);
    codeFontSizeSlider.setValue(codeFontSize, juce::dontSendNotification);
    codeFontSizeSlider.setColour(juce::Slider::trackColourId, ide::border.brighter(0.15f));
    codeFontSizeSlider.setColour(juce::Slider::thumbColourId, ide::constant);
    codeFontSizeSlider.setColour(juce::Slider::backgroundColourId, ide::active);

    for (auto& label : builtinParameterLabels)
        configureBodyLabel(label, {});

    for (auto& slider : builtinParameterSliders)
        configureSlider(slider, 0.0, 1.0, 0.001);

    codeEditor.setTabSize(4, true);
    codeEditor.setScrollbarThickness(12);

    processorClassEditor.setMultiLine(false);
    processorClassEditor.setReturnKeyStartsNewLine(false);
    processorClassEditor.setInputRestrictions(0, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_:");
    processorClassEditor.setTextToShowWhenEmpty("MyAudioProcessor", ide::textMuted);
    processorClassEditor.setText(projectManager.getProcessorClassName(), juce::dontSendNotification);

    buildLogEditor.setMultiLine(true);
    buildLogEditor.setReadOnly(true);
    buildLogEditor.setScrollbarsShown(true);

    projectTreeView.setRootItemVisible(true);
    projectTreeView.setDefaultOpenness(true);
    projectTreeView.setIndentSize(16);
    projectTreeView.setOpenCloseButtonsVisible(true);
    controlsViewport.setViewedComponent(&controlsContent, false);
    controlsViewport.setScrollBarsShown(true, false);
    controlsViewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::never);

    controlsPanelExpandedWidth = static_cast<float>(controlsWidth);
    controlsPanelCurrentWidth = controlsPanelExpandedWidth;
    controlsPanelTargetWidth = controlsPanelExpandedWidth;
    navigatorPanelExpandedWidth = 280.0f;
    navigatorPanelCurrentWidth = navigatorPanelExpandedWidth;
    navigatorPanelTargetWidth = navigatorPanelExpandedWidth;

    populateCombos();
    applyDarkTheme();
    applyCodeEditorAppearance();
    configureButtonsAndCallbacks();
    refreshPanelButtons();

    for (auto* component : std::initializer_list<juce::Component*>
         {
             &controlsViewport, &workspaceResizerBar, navigatorResizeBar.get(), &leftPanelToggleStrip, &navigatorToggleStrip,
             &sourceHeader, &transportHeader, &wavHeader, &dspHeader, &builtinHeader, &userHeader, &toolsHeader, &editorHeader, &navigatorHeader,
             &logHeader, &projectPathLabel, &codeFontSizeLabel, &codeFontSizeValueLabel, &sourceCombo, &startButton, &stopButton,
             &gainLabel, &gainSlider, &frequencyLabel, &frequencySlider, &sampleRateLabel, &blockSizeLabel, &loadWavButton,
             &wavFileLabel, &wavLoopToggle, &wavPositionLabel, &wavPositionSlider, &processingModeCombo, &builtinProcessorCombo,
             &savePresetButton, &loadPresetButton, &newProjectButton, &openProjectButton, &saveProjectButton, &saveProjectAsButton,
             &compileButton, &showOscilloscopeButton, &showControlsButton, &toggleLeftPanelButton, &toggleNavigatorButton,
             &codeFontSizeSlider, &processorClassLabel, &processorClassEditor, &compileStatusLabel, &buildLogEditor, &codeEditor
         })
    {
        if (component == &sourceHeader || component == &transportHeader || component == &wavHeader || component == &dspHeader
            || component == &builtinHeader || component == &userHeader || component == &toolsHeader || component == &sourceCombo
            || component == &startButton || component == &stopButton || component == &gainLabel || component == &gainSlider
            || component == &frequencyLabel || component == &frequencySlider || component == &sampleRateLabel || component == &blockSizeLabel
            || component == &loadWavButton || component == &wavFileLabel || component == &wavLoopToggle || component == &wavPositionLabel
            || component == &wavPositionSlider || component == &processingModeCombo || component == &builtinProcessorCombo
            || component == &savePresetButton || component == &loadPresetButton || component == &showOscilloscopeButton
            || component == &showControlsButton)
        {
            controlsContent.addAndMakeVisible(component);
        }
        else
        {
            addAndMakeVisible(component);
        }
    }

    addAndMakeVisible(projectTreeView);
    controlsContent.addAndMakeVisible(userModuleStatusLabel);

    for (auto& label : builtinParameterLabels)
        controlsContent.addAndMakeVisible(label);

    for (auto& slider : builtinParameterSliders)
        controlsContent.addAndMakeVisible(slider);

    audioEngine.setSourceGain(0.2f);
    audioEngine.setSineFrequency(440.0f);
    audioEngine.setBuiltinProcessorType(BuiltinProcessorType::bypass);

    if (const auto initResult = audioEngine.initialise(); initResult.failed())
        showErrorMessage("Audio Initialisation Failed", initResult.getErrorMessage());

    refreshBuiltinControls();
    refreshUserControls();
    refreshEngineState();
    refreshProjectState();
    refreshCompilerState();
    setSize(1520, 940);
    startTimerHz(20);
}

MainComponent::~MainComponent()
{
    stopTimer();
    controlsWindow.reset();
    oscilloscopeWindow.reset();
    setLookAndFeel(nullptr);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(ide::background);

    auto drawPanel = [&g] (juce::Rectangle<int> bounds, juce::Colour fillColour)
    {
        if (bounds.isEmpty())
            return;

        auto area = bounds.toFloat();
        g.setColour(fillColour);
        g.fillRoundedRectangle(area, 8.0f);
        g.setColour(ide::border);
        g.drawRoundedRectangle(area, 8.0f, 1.0f);
    };

    drawPanel(controlsPanel.getBounds(), ide::panel);
    drawPanel(leftPanelToggleStrip.getBounds(), ide::active);
    drawPanel(editorToolbarPanel.getBounds(), ide::panel);
    drawPanel(editorPanel.getBounds(), ide::background);
    drawPanel(navigatorPanel.getBounds(), ide::panel);
    drawPanel(navigatorToggleStrip.getBounds(), ide::active);
    drawPanel(logPanel.getBounds(), ide::panel);
    drawPanel(editorOptionsPanel.getBounds(), ide::active);
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds().reduced(12);
    controlsPanel.setBounds(bounds.removeFromLeft(juce::roundToInt(controlsPanelCurrentWidth)).reduced(0, 4));
    leftPanelToggleStrip.setBounds(bounds.removeFromLeft(panelToggleStripWidth).reduced(0, 4));
    bounds.removeFromLeft(10);

    auto contentArea = bounds;
    editorToolbarPanel.setBounds(contentArea.removeFromTop(toolbarHeight));
    contentArea.removeFromTop(10);

    juce::Component* verticalItems[] = { &workspacePanel, &workspaceResizerBar, &logPanel };
    workspaceLayout.layOutComponents(verticalItems,
                                     3,
                                     contentArea.getX(),
                                     contentArea.getY(),
                                     contentArea.getWidth(),
                                     contentArea.getHeight(),
                                     true,
                                     true);

    auto workspaceArea = workspacePanel.getBounds();
    navigatorToggleStrip.setBounds(workspaceArea.removeFromRight(panelToggleStripWidth));

    const auto navigatorWidth = juce::roundToInt(navigatorPanelCurrentWidth);

    if (navigatorWidth > 0)
    {
        navigatorPanel.setBounds(workspaceArea.removeFromRight(navigatorWidth));
        navigatorResizeBar->setBounds(workspaceArea.removeFromRight(splitterThickness));
    }
    else
    {
        navigatorPanel.setBounds({});
        navigatorResizeBar->setBounds({});
    }

    editorPanel.setBounds(workspaceArea);

    controlsViewport.setBounds(controlsPanel.getBounds().reduced(4));

    const auto leftPanelWidth = controlsViewport.getWidth();
    const auto leftPanelHeight = controlsViewport.getHeight();

    if (leftPanelWidth >= 120 && leftPanelHeight >= 120)
    {
        const int contentWidth = juce::jmax(leftPanelWidth - 28, 120);
        const int x = 14;
        int y = 14;

        auto placeRow = [&] (int height)
        {
            auto row = juce::Rectangle<int>(x, y, contentWidth, height);
            y += height + 6;
            return row;
        };

        sourceHeader.setBounds(placeRow(24));
        sourceCombo.setBounds(placeRow(28));

        transportHeader.setBounds(placeRow(24));
        auto transportRow = placeRow(30);
        startButton.setBounds(transportRow.removeFromLeft(transportRow.getWidth() / 2).reduced(2, 0));
        stopButton.setBounds(transportRow.reduced(2, 0));

        toolsHeader.setBounds(placeRow(24));
        auto toolsRow = placeRow(30);
        showOscilloscopeButton.setBounds(toolsRow.removeFromLeft(toolsRow.getWidth() / 2).reduced(2, 0));
        showControlsButton.setBounds(toolsRow.reduced(2, 0));

        gainLabel.setBounds(placeRow(18));
        gainSlider.setBounds(placeRow(28));
        frequencyLabel.setBounds(placeRow(18));
        frequencySlider.setBounds(placeRow(28));
        sampleRateLabel.setBounds(placeRow(18));
        blockSizeLabel.setBounds(placeRow(18));

        wavHeader.setBounds(placeRow(24));
        loadWavButton.setBounds(placeRow(28));
        wavFileLabel.setBounds(placeRow(20));
        wavLoopToggle.setBounds(placeRow(22));
        wavPositionLabel.setBounds(placeRow(18));
        wavPositionSlider.setBounds(placeRow(28));

        dspHeader.setBounds(placeRow(24));
        processingModeCombo.setBounds(placeRow(28));

        builtinHeader.setBounds(placeRow(24));
        builtinProcessorCombo.setBounds(placeRow(28));

        for (int index = 0; index < static_cast<int>(builtinParameterLabels.size()); ++index)
        {
            builtinParameterLabels[static_cast<std::size_t>(index)].setBounds(placeRow(18));
            builtinParameterSliders[static_cast<std::size_t>(index)].setBounds(placeRow(28));
        }

        auto presetRow = placeRow(30);
        savePresetButton.setBounds(presetRow.removeFromLeft(presetRow.getWidth() / 2).reduced(2, 0));
        loadPresetButton.setBounds(presetRow.reduced(2, 0));

        userHeader.setBounds(placeRow(24));
        userModuleStatusLabel.setBounds(placeRow(72));

        controlsContent.setSize(leftPanelWidth, y + 10);
    }
    else
    {
        for (auto* component : std::initializer_list<juce::Component*>
             {
                 &sourceHeader, &sourceCombo, &transportHeader, &startButton, &stopButton, &toolsHeader,
                 &showOscilloscopeButton, &showControlsButton, &gainLabel, &gainSlider, &frequencyLabel, &frequencySlider,
                 &sampleRateLabel, &blockSizeLabel, &wavHeader, &loadWavButton, &wavFileLabel, &wavLoopToggle,
                 &wavPositionLabel, &wavPositionSlider, &dspHeader, &processingModeCombo, &builtinHeader,
                 &builtinProcessorCombo, &savePresetButton, &loadPresetButton, &userHeader, &userModuleStatusLabel
             })
        {
            component->setBounds({});
        }

        for (auto& label : builtinParameterLabels)
            label.setBounds({});

        for (auto& slider : builtinParameterSliders)
            slider.setBounds({});

        controlsContent.setSize(juce::jmax(leftPanelWidth, 1), juce::jmax(leftPanelHeight, 1));
    }

    toggleLeftPanelButton.setBounds(leftPanelToggleStrip.getBounds().reduced(2, 8));
    toggleNavigatorButton.setBounds(navigatorToggleStrip.getBounds().reduced(2, 8));

    auto toolbarArea = editorToolbarPanel.getBounds().reduced(10, 8);
    auto optionsArea = toolbarArea.removeFromRight(320);
    editorOptionsPanel.setBounds(optionsArea);

    const int buttonWidth = 118;
    newProjectButton.setBounds(toolbarArea.removeFromLeft(buttonWidth).reduced(2, 0));
    openProjectButton.setBounds(toolbarArea.removeFromLeft(buttonWidth).reduced(2, 0));
    saveProjectButton.setBounds(toolbarArea.removeFromLeft(buttonWidth).reduced(2, 0));
    saveProjectAsButton.setBounds(toolbarArea.removeFromLeft(136).reduced(2, 0));
    compileButton.setBounds(toolbarArea.removeFromLeft(108).reduced(2, 0));

    auto editorOptionsArea = editorOptionsPanel.getBounds().reduced(10, 8);
    codeFontSizeLabel.setBounds(editorOptionsArea.removeFromLeft(74));
    codeFontSizeValueLabel.setBounds(editorOptionsArea.removeFromRight(52));
    editorOptionsArea.removeFromRight(6);
    codeFontSizeSlider.setBounds(editorOptionsArea);

    auto editorArea = editorPanel.getBounds().reduced(10);
    editorHeader.setBounds(editorArea.removeFromTop(28));
    editorArea.removeFromTop(6);
    codeEditor.setBounds(editorArea);

    auto navigatorArea = navigatorPanel.getBounds().reduced(10);
    navigatorHeader.setBounds(navigatorArea.removeFromTop(28));
    projectPathLabel.setBounds(navigatorArea.removeFromTop(30));
    navigatorArea.removeFromTop(6);
    projectTreeView.setBounds(navigatorArea);

    auto logArea = logPanel.getBounds().reduced(10);
    logHeader.setBounds(logArea.removeFromTop(28));
    auto statusArea = logArea.removeFromTop(32);
    auto processorClassArea = statusArea.removeFromRight(370);
    processorClassLabel.setBounds(processorClassArea.removeFromLeft(108));
    processorClassEditor.setBounds(processorClassArea.reduced(2, 0));
    compileStatusLabel.setBounds(statusArea);
    logArea.removeFromTop(6);
    buildLogEditor.setBounds(logArea);
}

void MainComponent::refreshPanelButtons()
{
    toggleLeftPanelButton.setButtonText(controlsPanelCollapsed ? ">" : "<");
    toggleNavigatorButton.setButtonText(navigatorPanelCollapsed ? "<" : ">");
    navigatorResizeBar->setVisible(navigatorPanelCurrentWidth > 0.5f);
    navigatorHeader.setVisible(navigatorPanelCurrentWidth > 0.5f);
    projectPathLabel.setVisible(navigatorPanelCurrentWidth > 0.5f);
    projectTreeView.setVisible(navigatorPanelCurrentWidth > 0.5f);
}

void MainComponent::updatePanelAnimation()
{
    const auto nextControlsWidth = animateTowards(controlsPanelCurrentWidth, controlsPanelTargetWidth);
    const auto nextNavigatorWidth = animateTowards(navigatorPanelCurrentWidth, navigatorPanelTargetWidth);
    const auto controlsChanged = std::abs(nextControlsWidth - controlsPanelCurrentWidth) > 0.0f;
    const auto navigatorChanged = std::abs(nextNavigatorWidth - navigatorPanelCurrentWidth) > 0.0f;

    controlsPanelCurrentWidth = nextControlsWidth;
    navigatorPanelCurrentWidth = nextNavigatorWidth;

    if (controlsChanged || navigatorChanged)
    {
        refreshPanelButtons();
        resized();
        repaint();
    }
}

void MainComponent::toggleLeftPanel()
{
    if (! controlsPanelCollapsed)
    {
        controlsPanelExpandedWidth = juce::jmax(220.0f, controlsPanelCurrentWidth);
        controlsPanelTargetWidth = 0.0f;
        controlsPanelCollapsed = true;
    }
    else
    {
        controlsPanelTargetWidth = controlsPanelExpandedWidth;
        controlsPanelCollapsed = false;
    }

    refreshPanelButtons();
}

void MainComponent::toggleNavigatorPanel()
{
    if (! navigatorPanelCollapsed)
    {
        navigatorPanelExpandedWidth = juce::jmax(navigatorMinimumWidth, navigatorPanelCurrentWidth);
        navigatorPanelTargetWidth = 0.0f;
        navigatorPanelCollapsed = true;
    }
    else
    {
        navigatorPanelTargetWidth = navigatorPanelExpandedWidth;
        navigatorPanelCollapsed = false;
    }

    refreshPanelButtons();
}

void MainComponent::resizeNavigatorPanel(float newWidth)
{
    if (navigatorPanelCollapsed)
        return;

    navigatorPanelExpandedWidth = juce::jlimit(navigatorMinimumWidth,
                                               juce::jmax(navigatorMinimumWidth, static_cast<float>(workspacePanel.getWidth()) * 0.65f),
                                               newWidth);
    navigatorPanelCurrentWidth = navigatorPanelExpandedWidth;
    navigatorPanelTargetWidth = navigatorPanelExpandedWidth;
    refreshPanelButtons();
    resized();
}

void MainComponent::ensureOscilloscopeWindow()
{
    if (oscilloscopeWindow != nullptr)
        return;

    oscilloscopeWindow = std::make_unique<ToolWindow>("Oscilloscope",
                                                      std::make_unique<OscilloscopeComponent>(audioEngine.getOscilloscopeBuffer()),
                                                      720,
                                                      360,
                                                      420,
                                                      240);
}

void MainComponent::ensureControlsWindow()
{
    if (controlsWindow != nullptr)
        return;

    controlsWindow = std::make_unique<ToolWindow>("Controls",
                                                  std::make_unique<RealtimeControlsComponent>(audioEngine, projectManager),
                                                  860,
                                                  620,
                                                  560,
                                                  420);
}

void MainComponent::showOscilloscopeWindow()
{
    ensureOscilloscopeWindow();
    oscilloscopeWindow->present(this);
}

void MainComponent::showControlsWindow()
{
    ensureControlsWindow();
    controlsWindow->present(this);
}

bool MainComponent::handleCloseRequest()
{
    if (! projectManager.hasUnsavedChanges())
        return true;

    auto safeThis = juce::Component::SafePointer<MainComponent>(this);

    confirmProjectTransition("closing the application",
                             [safeThis] (bool shouldClose)
                             {
                                 if (shouldClose && safeThis != nullptr)
                                     juce::JUCEApplication::getInstance()->quit();
                             });

    return false;
}

void MainComponent::timerCallback()
{
    updatePanelAnimation();
    audioEngine.getUserDspHost().reclaimRetiredRuntimes();
    refreshEngineState();
    refreshBuiltinControls();
    refreshUserControls();
    refreshProjectState();
    refreshCompilerState();
}

void MainComponent::setToolWindowRefreshSuspended(bool shouldSuspend)
{
    if (oscilloscopeWindow != nullptr)
        if (auto* oscilloscope = dynamic_cast<OscilloscopeComponent*>(oscilloscopeWindow->getToolContent()); oscilloscope != nullptr)
            oscilloscope->setRefreshSuspended(shouldSuspend);

    if (controlsWindow != nullptr)
        if (auto* controls = dynamic_cast<RealtimeControlsComponent*>(controlsWindow->getToolContent()); controls != nullptr)
            controls->setRefreshSuspended(shouldSuspend);
}

bool MainComponent::pauseRefreshTimer()
{
    const auto wasRunning = isTimerRunning();

    if (wasRunning)
        stopTimer();

    setToolWindowRefreshSuspended(true);

    return wasRunning;
}

void MainComponent::resumeRefreshTimer(bool shouldResume)
{
    setToolWindowRefreshSuspended(false);

    if (shouldResume)
        startTimerHz(20);
}

void MainComponent::showTextInputDialogAsync(const juce::String& title,
                                             const juce::String& message,
                                             const juce::String& defaultValue,
                                             const juce::String& editorName,
                                             std::function<void(juce::String)> onComplete)
{
    const auto shouldResumeTimer = pauseRefreshTimer();
    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    auto* window = new juce::AlertWindow(title, message, juce::MessageBoxIconType::NoIcon, this);
    configureAlertWindowLookAndFeel(*window, ideLookAndFeel.get());
    window->addTextEditor(editorName, defaultValue);
    configureAlertTextEditor(window->getTextEditor(editorName));
    window->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    auto safeWindow = juce::Component::SafePointer<juce::AlertWindow>(window);

    window->enterModalState(true,
                            juce::ModalCallbackFunction::create(
                                [safeThis, safeWindow, shouldResumeTimer, editorName, completionHandler = std::move(onComplete)] (int result) mutable
                                {
                                    juce::String text;

                                    if (result == 1 && safeWindow != nullptr)
                                        text = safeWindow->getTextEditorContents(editorName).trim();

                                    if (safeWindow != nullptr)
                                        safeWindow->setLookAndFeel(nullptr);

                                    if (safeThis != nullptr)
                                        safeThis->resumeRefreshTimer(shouldResumeTimer);

                                    if (completionHandler)
                                        completionHandler(text);
                                }),
                            true);

    auto safeEditor = juce::Component::SafePointer<juce::TextEditor>(window->getTextEditor(editorName));

    juce::MessageManager::callAsync([safeEditor]
    {
        if (safeEditor != nullptr)
        {
            safeEditor->grabKeyboardFocus();
            safeEditor->selectAll();
        }
    });
}

void MainComponent::showChoiceDialogAsync(const juce::String& title,
                                          const juce::String& message,
                                          const juce::StringArray& options,
                                          std::function<void(int)> onComplete)
{
    const auto shouldResumeTimer = pauseRefreshTimer();
    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    auto* window = new juce::AlertWindow(title, message, juce::MessageBoxIconType::NoIcon, this);
    configureAlertWindowLookAndFeel(*window, ideLookAndFeel.get());
    window->addComboBox("choice", options, {});
    configureAlertComboBox(window->getComboBoxComponent("choice"));

    if (auto* combo = window->getComboBoxComponent("choice"); combo != nullptr && options.size() > 0)
        combo->setSelectedItemIndex(0, juce::dontSendNotification);

    window->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    auto safeWindow = juce::Component::SafePointer<juce::AlertWindow>(window);

    window->enterModalState(true,
                            juce::ModalCallbackFunction::create(
                                [safeThis, safeWindow, shouldResumeTimer, completionHandler = std::move(onComplete)] (int result) mutable
                                {
                                    int selectedIndex = -1;

                                    if (result == 1 && safeWindow != nullptr)
                                        if (auto* combo = safeWindow->getComboBoxComponent("choice"); combo != nullptr)
                                            selectedIndex = combo->getSelectedItemIndex();

                                    if (safeWindow != nullptr)
                                        safeWindow->setLookAndFeel(nullptr);

                                    if (safeThis != nullptr)
                                        safeThis->resumeRefreshTimer(shouldResumeTimer);

                                    if (completionHandler)
                                        completionHandler(selectedIndex);
                                }),
                            true);
}

void MainComponent::showSaveDiscardCancelDialogAsync(const juce::String& title,
                                                     const juce::String& message,
                                                     std::function<void(int)> onComplete)
{
    const auto shouldResumeTimer = pauseRefreshTimer();
    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    auto* window = new juce::AlertWindow(title, message, juce::MessageBoxIconType::QuestionIcon, this);
    configureAlertWindowLookAndFeel(*window, ideLookAndFeel.get());
    window->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Discard", 2);
    window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    auto safeWindow = juce::Component::SafePointer<juce::AlertWindow>(window);

    window->enterModalState(true,
                            juce::ModalCallbackFunction::create(
                                [safeThis, safeWindow, shouldResumeTimer, completionHandler = std::move(onComplete)] (int result) mutable
                                {
                                    if (safeWindow != nullptr)
                                        safeWindow->setLookAndFeel(nullptr);

                                    if (safeThis != nullptr)
                                        safeThis->resumeRefreshTimer(shouldResumeTimer);

                                    if (completionHandler)
                                        completionHandler(result);
                                }),
                            true);
}

void MainComponent::showOkCancelDialogAsync(const juce::String& title,
                                            const juce::String& message,
                                            const juce::String& okText,
                                            std::function<void(bool)> onComplete)
{
    const auto shouldResumeTimer = pauseRefreshTimer();
    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    auto* window = new juce::AlertWindow(title, message, juce::MessageBoxIconType::WarningIcon, this);
    configureAlertWindowLookAndFeel(*window, ideLookAndFeel.get());
    window->addButton(okText, 1, juce::KeyPress(juce::KeyPress::returnKey));
    window->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    auto safeWindow = juce::Component::SafePointer<juce::AlertWindow>(window);

    window->enterModalState(true,
                            juce::ModalCallbackFunction::create(
                                [safeThis, safeWindow, shouldResumeTimer, completionHandler = std::move(onComplete)] (int result) mutable
                                {
                                    if (safeWindow != nullptr)
                                        safeWindow->setLookAndFeel(nullptr);

                                    if (safeThis != nullptr)
                                        safeThis->resumeRefreshTimer(shouldResumeTimer);

                                    if (completionHandler)
                                        completionHandler(result == 1);
                                }),
                            true);
}

void MainComponent::configureSlider(juce::Slider& slider,
                                    double minValue,
                                    double maxValue,
                                    double step,
                                    const juce::String& suffix)
{
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 82, 22);
    slider.setRange(minValue, maxValue, step);
    slider.setTextValueSuffix(suffix);
    slider.setColour(juce::Slider::textBoxTextColourId, ide::text);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, ide::active);
    slider.setColour(juce::Slider::textBoxOutlineColourId, ide::border);
    slider.setColour(juce::Slider::trackColourId, ide::border.brighter(0.2f));
    slider.setColour(juce::Slider::thumbColourId, ide::constant);
    slider.setColour(juce::Slider::backgroundColourId, ide::active);
}

void MainComponent::configureButtonsAndCallbacks()
{
    startButton.onClick = [this] { audioEngine.startTransport(); };
    stopButton.onClick = [this] { audioEngine.stopTransport(); };
    loadWavButton.onClick = [this] { launchLoadWavChooser(); };
    showOscilloscopeButton.onClick = [this] { showOscilloscopeWindow(); };
    showControlsButton.onClick = [this] { showControlsWindow(); };
    toggleLeftPanelButton.onClick = [this] { toggleLeftPanel(); };
    toggleNavigatorButton.onClick = [this] { toggleNavigatorPanel(); };
    wavLoopToggle.onClick = [this] { audioEngine.setWavLooping(wavLoopToggle.getToggleState()); };

    gainSlider.onValueChange = [this]
    {
        if (! updatingUi)
            audioEngine.setSourceGain(static_cast<float>(gainSlider.getValue()));
    };

    frequencySlider.onValueChange = [this]
    {
        if (! updatingUi)
            audioEngine.setSineFrequency(static_cast<float>(frequencySlider.getValue()));
    };

    wavPositionSlider.onValueChange = [this]
    {
        if (! updatingUi)
            audioEngine.setWavPositionNormalized(wavPositionSlider.getValue());
    };

    sourceCombo.onChange = [this]
    {
        if (updatingUi)
            return;

        audioEngine.setSourceType(static_cast<SourceType>(sourceCombo.getSelectedId() - 1));
    };

    processingModeCombo.onChange = [this]
    {
        if (updatingUi)
            return;

        const auto mode = processingModeCombo.getSelectedId() == 2 ? ProcessingMode::userModule : ProcessingMode::builtIn;
        audioEngine.setProcessingMode(mode);
        refreshBuiltinControls();
        refreshUserControls();
    };

    builtinProcessorCombo.onChange = [this]
    {
        if (updatingUi)
            return;

        const auto type = static_cast<BuiltinProcessorType>(builtinProcessorCombo.getSelectedId() - 1);
        audioEngine.setBuiltinProcessorType(type);
        refreshBuiltinControls();
    };

    for (int index = 0; index < static_cast<int>(builtinParameterSliders.size()); ++index)
    {
        builtinParameterSliders[static_cast<std::size_t>(index)].onValueChange = [this, index]
        {
            if (! updatingUi)
                audioEngine.setBuiltinParameter(index, static_cast<float>(builtinParameterSliders[static_cast<std::size_t>(index)].getValue()));
        };
    }

    savePresetButton.onClick = [this] { launchSavePresetChooser(); };
    loadPresetButton.onClick = [this] { launchLoadPresetChooser(); };
    newProjectButton.onClick = [this] { promptForNewProject(); };
    openProjectButton.onClick = [this] { promptForOpenProject(); };
    saveProjectButton.onClick = [this] { saveProject(false); };
    saveProjectAsButton.onClick = [this] { saveProject(true); };
    compileButton.onClick = [this] { compileProject(); };
    processorClassEditor.onTextChange = [this] { updateProcessorClassFromEditor(); };
    codeFontSizeSlider.onValueChange = [this]
    {
        codeFontSize = static_cast<float>(codeFontSizeSlider.getValue());
        applyCodeEditorAppearance();
    };
}

void MainComponent::populateCombos()
{
    auto configureCombo = [] (juce::ComboBox& combo)
    {
        combo.setColour(juce::ComboBox::backgroundColourId, ide::panel);
        combo.setColour(juce::ComboBox::textColourId, ide::text);
        combo.setColour(juce::ComboBox::outlineColourId, ide::border);
        combo.setColour(juce::ComboBox::buttonColourId, ide::panel);
        combo.setColour(juce::ComboBox::arrowColourId, ide::textSecondary);
        combo.setColour(juce::ComboBox::focusedOutlineColourId, ide::constant);
    };

    for (auto source : getAvailableSourceTypes())
        sourceCombo.addItem(sourceTypeToDisplayName(source), static_cast<int>(source) + 1);

    sourceCombo.setSelectedId(static_cast<int>(SourceType::sine) + 1, juce::dontSendNotification);
    configureCombo(sourceCombo);

    processingModeCombo.addItem("Built-in DSP", 1);
    processingModeCombo.addItem("User DSP Module", 2);
    processingModeCombo.setSelectedId(1, juce::dontSendNotification);
    configureCombo(processingModeCombo);

    for (const auto& descriptor : getBuiltinProcessorDescriptors())
        builtinProcessorCombo.addItem(descriptor.name, static_cast<int>(descriptor.type) + 1);

    builtinProcessorCombo.setSelectedId(static_cast<int>(BuiltinProcessorType::bypass) + 1, juce::dontSendNotification);
    configureCombo(builtinProcessorCombo);
}

void MainComponent::refreshEngineState()
{
    const auto snapshot = audioEngine.getSnapshot();
    updatingUi = true;

    sourceCombo.setSelectedId(static_cast<int>(snapshot.sourceType) + 1, juce::dontSendNotification);
    processingModeCombo.setSelectedId(snapshot.processingMode == ProcessingMode::builtIn ? 1 : 2, juce::dontSendNotification);
    builtinProcessorCombo.setSelectedId(static_cast<int>(snapshot.builtinProcessor) + 1, juce::dontSendNotification);
    gainSlider.setValue(snapshot.sourceGain, juce::dontSendNotification);
    frequencySlider.setValue(snapshot.sineFrequency, juce::dontSendNotification);
    wavFileLabel.setText(snapshot.wavLoaded ? snapshot.wavFileName : "No WAV loaded", juce::dontSendNotification);
    wavLoopToggle.setToggleState(snapshot.wavLooping, juce::dontSendNotification);
    sampleRateLabel.setText("Sample Rate: " + juce::String(snapshot.sampleRate, 1) + " Hz", juce::dontSendNotification);
    blockSizeLabel.setText("Block Size: " + juce::String(snapshot.blockSize), juce::dontSendNotification);

    if (! wavPositionSlider.isMouseButtonDown())
        wavPositionSlider.setValue(snapshot.wavPosition, juce::dontSendNotification);

    frequencySlider.setEnabled(snapshot.sourceType == SourceType::sine);
    wavLoopToggle.setEnabled(snapshot.wavLoaded);
    wavPositionSlider.setEnabled(snapshot.wavLoaded);
    stopButton.setEnabled(snapshot.transportRunning);
    startButton.setEnabled(! snapshot.transportRunning);

    updatingUi = false;
}

void MainComponent::refreshBuiltinControls()
{
    const auto descriptor = getBuiltinProcessorDescriptor(audioEngine.getBuiltinProcessorType());
    const auto builtinModeEnabled = processingModeCombo.getSelectedId() == 1;

    builtinProcessorCombo.setEnabled(builtinModeEnabled);
    savePresetButton.setEnabled(builtinModeEnabled);
    loadPresetButton.setEnabled(builtinModeEnabled);

    updatingUi = true;

    for (int index = 0; index < static_cast<int>(builtinParameterSliders.size()); ++index)
    {
        const auto visible = index < descriptor.parameterCount;
        auto& label = builtinParameterLabels[static_cast<std::size_t>(index)];
        auto& slider = builtinParameterSliders[static_cast<std::size_t>(index)];

        label.setVisible(visible);
        slider.setVisible(visible);
        slider.setEnabled(visible && builtinModeEnabled);

        if (visible)
        {
            const auto& spec = descriptor.parameters[static_cast<std::size_t>(index)];
            label.setText(spec.name, juce::dontSendNotification);
            slider.setRange(spec.minValue, spec.maxValue, 0.0001);
            slider.setValue(audioEngine.getBuiltinParameter(index), juce::dontSendNotification);
        }
    }

    updatingUi = false;
}

void MainComponent::refreshUserControls()
{
    const auto snapshot = audioEngine.getUserDspHost().getSnapshot();
    const auto& controllerDefinitions = projectManager.getControllerDefinitions();
    const auto userModeEnabled = processingModeCombo.getSelectedId() == 2;

    const auto layoutMatchesCompiledModule = [&]
    {
        if (! snapshot.hasActiveModule || snapshot.controlCount != static_cast<int>(controllerDefinitions.size()))
            return false;

        for (int index = 0; index < snapshot.controlCount; ++index)
        {
            const auto& compiledControl = snapshot.controls[static_cast<std::size_t>(index)];
            const auto& projectControl = controllerDefinitions[static_cast<std::size_t>(index)];

            if (! compiledControl.active
                || compiledControl.type != projectControl.type
                || compiledControl.label != projectControl.label
                || compiledControl.codeName != projectControl.codeName)
            {
                return false;
            }
        }

        return true;
    }();

    updatingUi = true;

    userHeader.setText(userModeEnabled ? "User DSP Module: " + snapshot.processorName : "User DSP Module",
                       juce::dontSendNotification);

    juce::String statusText;

    if (controllerDefinitions.empty())
    {
        statusText = "No custom controls yet.\nOpen Controls to add knobs, buttons, or toggles.";
    }
    else if (! snapshot.hasActiveModule)
    {
        statusText = "Compile the project to expose " + juce::String(controllerDefinitions.size())
                   + " named control" + (controllerDefinitions.size() == 1 ? "" : "s") + ".";
    }
    else if (! layoutMatchesCompiledModule)
    {
        statusText = "Controller layout changed.\nCompile again to apply the current Controls setup.";
    }
    else
    {
        statusText = "Loaded module with " + juce::String(snapshot.controlCount)
                   + " control" + (snapshot.controlCount == 1 ? "" : "s") + ".\n";
        statusText += "Open Controls for realtime values.";
    }

    userModuleStatusLabel.setText(statusText, juce::dontSendNotification);

    updatingUi = false;
}

void MainComponent::refreshCompilerState()
{
    const auto snapshot = compiler.getSnapshot();
    compileStatusLabel.setText(snapshot.statusText, juce::dontSendNotification);
    buildLogEditor.setText(snapshot.logText, juce::dontSendNotification);
    compileButton.setEnabled(snapshot.state != UserDspCompiler::State::compiling);

    juce::Colour statusColour = ide::textSecondary;

    switch (snapshot.state)
    {
        case UserDspCompiler::State::idle:      statusColour = ide::textSecondary; break;
        case UserDspCompiler::State::compiling: statusColour = ide::warning; break;
        case UserDspCompiler::State::succeeded: statusColour = ide::success; break;
        case UserDspCompiler::State::failed:    statusColour = ide::error; break;
    }

    compileStatusLabel.setColour(juce::Label::textColourId, statusColour);
}

void MainComponent::refreshProjectState()
{
    updatingUi = true;
    processorClassEditor.setText(projectManager.getProcessorClassName(), juce::dontSendNotification);
    updatingUi = false;

    auto projectStatusText = projectManager.getArchiveFile() != juce::File()
                                 ? projectManager.getArchiveFile().getFullPathName()
                                 : (projectManager.getProjectName() + " (unsaved)");

    if (projectManager.hasUnsavedChanges())
        projectStatusText << " *";

    projectPathLabel.setText(projectStatusText, juce::dontSendNotification);

    const auto activeFilePath = projectManager.getActiveFilePath();
    editorHeader.setText(activeFilePath.isNotEmpty() ? ("Code Editor: " + activeFilePath) : "Code Editor",
                         juce::dontSendNotification);

    if (navigatorSelectionPath.isEmpty())
        navigatorSelectionPath = activeFilePath;

    if (projectNavigatorNeedsRefresh)
    {
        refreshProjectNavigator();
        projectNavigatorNeedsRefresh = false;
    }
}

void MainComponent::refreshProjectNavigator()
{
    auto opennessState = projectTreeView.getOpennessState(true);
    projectTreeView.setRootItem(nullptr);
    projectTreeView.setRootItem(new ProjectTreeItem(*this, projectManager, {}));

    if (auto* rootItem = projectTreeView.getRootItem(); rootItem != nullptr)
    {
        rootItem->setOpen(true);

        if (opennessState != nullptr)
            projectTreeView.restoreOpennessState(*opennessState, true);

        const auto selectionPath = navigatorSelectionPath.isNotEmpty() ? navigatorSelectionPath : projectManager.getActiveFilePath();

        if (auto* projectRootItem = dynamic_cast<ProjectTreeItem*>(rootItem); projectRootItem != nullptr)
        {
            if (auto* selectedItem = projectRootItem->findItemByPath(selectionPath); selectedItem != nullptr)
                selectedItem->setSelected(true, true);
        }
    }
}

void MainComponent::applyDarkTheme()
{
    auto configureButton = [] (juce::TextButton& button)
    {
        button.setColour(juce::TextButton::buttonColourId, ide::active);
        button.setColour(juce::TextButton::buttonOnColourId, ide::active.brighter(0.15f));
        button.setColour(juce::TextButton::textColourOffId, ide::text);
        button.setColour(juce::TextButton::textColourOnId, ide::text);
    };

    for (auto* button : std::array<juce::TextButton*, 12>
         { &startButton, &stopButton, &loadWavButton, &savePresetButton, &loadPresetButton, &newProjectButton, &openProjectButton, &saveProjectButton,
           &saveProjectAsButton, &showOscilloscopeButton, &showControlsButton, &toggleLeftPanelButton })
    {
        configureButton(*button);
    }

    configureButton(toggleNavigatorButton);
    compileButton.setColour(juce::TextButton::buttonColourId, ide::success.darker(0.2f));
    compileButton.setColour(juce::TextButton::buttonOnColourId, ide::success);
    compileButton.setColour(juce::TextButton::textColourOffId, ide::text);
    compileButton.setColour(juce::TextButton::textColourOnId, ide::text);

    wavLoopToggle.setColour(juce::ToggleButton::textColourId, ide::text);
    wavLoopToggle.setColour(juce::ToggleButton::tickColourId, ide::constant);
    wavLoopToggle.setColour(juce::ToggleButton::tickDisabledColourId, ide::textMuted);

    buildLogEditor.setColour(juce::TextEditor::backgroundColourId, ide::panel);
    buildLogEditor.setColour(juce::TextEditor::textColourId, ide::text);
    buildLogEditor.setColour(juce::TextEditor::highlightColourId, ide::active);
    buildLogEditor.setColour(juce::TextEditor::highlightedTextColourId, ide::text);
    buildLogEditor.setColour(juce::TextEditor::outlineColourId, ide::border);
    buildLogEditor.setColour(juce::TextEditor::shadowColourId, juce::Colours::transparentBlack);

    processorClassEditor.setColour(juce::TextEditor::backgroundColourId, ide::active);
    processorClassEditor.setColour(juce::TextEditor::textColourId, ide::text);
    processorClassEditor.setColour(juce::TextEditor::highlightColourId, ide::border);
    processorClassEditor.setColour(juce::TextEditor::highlightedTextColourId, ide::text);
    processorClassEditor.setColour(juce::TextEditor::outlineColourId, ide::border);
    processorClassEditor.setColour(juce::TextEditor::focusedOutlineColourId, ide::constant);

    codeEditor.setColour(juce::CodeEditorComponent::backgroundColourId, ide::background);
    codeEditor.setColour(juce::CodeEditorComponent::defaultTextColourId, ide::text);
    codeEditor.setColour(juce::CodeEditorComponent::highlightColourId, ide::active);
    codeEditor.setColour(juce::CodeEditorComponent::lineNumberBackgroundId, ide::panel);
    codeEditor.setColour(juce::CodeEditorComponent::lineNumberTextId, ide::textMuted);

    juce::CodeEditorComponent::ColourScheme codeScheme;
    codeScheme.set("Error", ide::error);
    codeScheme.set("Comment", ide::comment);
    codeScheme.set("Keyword", ide::keyword);
    codeScheme.set("Operator", ide::textSecondary);
    codeScheme.set("Identifier", ide::variable);
    codeScheme.set("Integer", ide::number);
    codeScheme.set("Float", ide::number);
    codeScheme.set("String", ide::string);
    codeScheme.set("Bracket", ide::text);
    codeScheme.set("Punctuation", ide::textSecondary);
    codeScheme.set("Preprocessor Text", ide::constant);
    codeEditor.setColourScheme(codeScheme);

    projectTreeView.setColour(juce::TreeView::backgroundColourId, ide::panel);
    projectTreeView.setColour(juce::TreeView::linesColourId, ide::border);
}

void MainComponent::applyCodeEditorAppearance()
{
    const auto monoTypeface = juce::Font::getDefaultMonospacedFontName();
    const auto editorFont = juce::Font(juce::FontOptions(monoTypeface, codeFontSize, juce::Font::plain));
    const auto logFont = juce::Font(juce::FontOptions(monoTypeface, juce::jmax(codeFontSize - 1.0f, 12.0f), juce::Font::plain));

    codeEditor.setFont(editorFont);
    buildLogEditor.applyFontToAllText(logFont);
    processorClassEditor.applyFontToAllText(juce::Font(juce::FontOptions(monoTypeface, 14.0f, juce::Font::plain)));
    codeFontSizeValueLabel.setText(juce::String(static_cast<int>(codeFontSize)) + " px", juce::dontSendNotification);
}

void MainComponent::launchLoadWavChooser()
{
    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    activeChooser = std::make_unique<juce::FileChooser>("Load WAV file", juce::File(), "*.wav");
    activeChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                               [safeThis] (const juce::FileChooser& chooser)
                               {
                                   if (safeThis == nullptr)
                                       return;

                                   const auto file = chooser.getResult();

                                   if (! file.existsAsFile())
                                       return;

                                   if (const auto result = safeThis->audioEngine.loadWavFile(file); result.failed())
                                       safeThis->showErrorMessage("WAV Load Failed", result.getErrorMessage());
                               });
}

void MainComponent::launchSavePresetChooser()
{
    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    activeChooser = std::make_unique<juce::FileChooser>("Save built-in DSP preset", juce::File(), "*.json");
    activeChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                               [safeThis] (const juce::FileChooser& chooser)
                               {
                                   if (safeThis == nullptr)
                                       return;

                                   auto file = chooser.getResult();

                                   if (file == juce::File())
                                       return;

                                   if (file.getFileExtension().isEmpty())
                                       file = file.withFileExtension(".json");

                                   if (const auto result = safeThis->presetManager.savePreset(file, safeThis->audioEngine.captureBuiltinPreset()); result.failed())
                                       safeThis->showErrorMessage("Preset Save Failed", result.getErrorMessage());
                               });
}

void MainComponent::launchLoadPresetChooser()
{
    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    activeChooser = std::make_unique<juce::FileChooser>("Load built-in DSP preset", juce::File(), "*.json");
    activeChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                               [safeThis] (const juce::FileChooser& chooser)
                               {
                                   if (safeThis == nullptr)
                                       return;

                                   BuiltinPresetData preset;
                                   const auto file = chooser.getResult();

                                   if (! file.existsAsFile())
                                       return;

                                   if (const auto result = safeThis->presetManager.loadPreset(file, preset); result.failed())
                                   {
                                       safeThis->showErrorMessage("Preset Load Failed", result.getErrorMessage());
                                       return;
                                   }

                                   safeThis->audioEngine.applyBuiltinPreset(preset);
                                   safeThis->refreshBuiltinControls();
                               });
}

void MainComponent::saveProject(bool forceSaveAs, std::function<void(bool)> onComplete)
{
    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    auto finishCallback = [completionHandler = std::move(onComplete)] (bool succeeded) mutable
    {
        if (completionHandler)
            completionHandler(succeeded);
    };

    auto saveToTargetCallback = [safeThis, finishHandler = std::move(finishCallback)] (const juce::File& targetFile) mutable
    {
        if (safeThis == nullptr || targetFile == juce::File())
        {
            finishHandler(false);
            return;
        }

        const auto result = safeThis->projectManager.saveProjectAs(targetFile);

        if (result.failed())
        {
            safeThis->showErrorMessage("Project Save Failed", result.getErrorMessage());
            finishHandler(false);
            return;
        }

        safeThis->refreshProjectState();
        finishHandler(true);
    };

    auto targetFile = projectManager.getArchiveFile();

    if (forceSaveAs || targetFile == juce::File())
    {
        activeChooser = std::make_unique<juce::FileChooser>("Save DSP project",
                                                            targetFile != juce::File() ? targetFile
                                                                                       : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                                                                             .getChildFile(projectManager.getProjectName() + ".dspedu"),
                                                            "*.dspedu");
        activeChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                                   [saveTargetHandler = std::move(saveToTargetCallback)] (const juce::FileChooser& chooser) mutable
                                   {
                                       auto selectedFile = chooser.getResult();

                                       if (selectedFile != juce::File() && selectedFile.getFileExtension().isEmpty())
                                           selectedFile = selectedFile.withFileExtension(".dspedu");

                                       saveTargetHandler(selectedFile);
                                   });
        return;
    }

    saveToTargetCallback(targetFile);
}

void MainComponent::confirmProjectTransition(const juce::String& actionName, std::function<void(bool)> onComplete)
{
    if (! projectManager.hasUnsavedChanges())
    {
        if (onComplete)
            onComplete(true);
        return;
    }

    auto safeThis = juce::Component::SafePointer<MainComponent>(this);

    showSaveDiscardCancelDialogAsync("Unsaved DSP Project",
                                     "Save changes to the current DSP project before " + actionName + "?",
                                     [safeThis, completionHandler = std::move(onComplete)] (int result) mutable
                                     {
                                         if (safeThis == nullptr)
                                         {
                                             if (completionHandler)
                                                 completionHandler(false);
                                             return;
                                         }

                                         if (result == 1)
                                         {
                                             safeThis->saveProject(false, std::move(completionHandler));
                                             return;
                                         }

                                         if (completionHandler)
                                             completionHandler(result == 2);
                                     });
}

void MainComponent::promptForNewProject()
{
    auto safeThis = juce::Component::SafePointer<MainComponent>(this);

    confirmProjectTransition("creating a new project",
                             [safeThis] (bool shouldProceed)
                             {
                                 if (! shouldProceed || safeThis == nullptr)
                                     return;

                                 safeThis->projectManager.createNewProject();
                                 safeThis->navigatorSelectionPath = safeThis->projectManager.getActiveFilePath();
                                 safeThis->projectNavigatorNeedsRefresh = true;
                                 safeThis->refreshProjectState();
                             });
}

void MainComponent::promptForOpenProject()
{
    auto safeThis = juce::Component::SafePointer<MainComponent>(this);

    confirmProjectTransition("opening another project",
                             [safeThis] (bool shouldProceed)
                             {
                                 if (! shouldProceed || safeThis == nullptr)
                                     return;

                                 safeThis->activeChooser = std::make_unique<juce::FileChooser>("Open DSP project", juce::File(), "*.dspedu");
                                 safeThis->activeChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                                                      [safeThis] (const juce::FileChooser& chooser)
                                                                      {
                                                                          if (safeThis == nullptr)
                                                                              return;

                                                                          const auto selectedFile = chooser.getResult();

                                                                          if (! selectedFile.existsAsFile())
                                                                              return;

                                                                          if (const auto result = safeThis->projectManager.loadProjectArchive(selectedFile); result.failed())
                                                                          {
                                                                              safeThis->showErrorMessage("Project Open Failed", result.getErrorMessage());
                                                                              return;
                                                                          }

                                                                          safeThis->navigatorSelectionPath = safeThis->projectManager.getActiveFilePath();
                                                                          safeThis->projectNavigatorNeedsRefresh = true;
                                                                          safeThis->refreshProjectState();
                                                                      });
                             });
}

void MainComponent::compileProject()
{
    updateProcessorClassFromEditor();

    if (const auto result = compiler.compileAsync(projectManager.createCompileSnapshot()); result.failed())
    {
        const auto title = result.getErrorMessage().contains("already running") ? "Compilation Busy" : "Compilation Error";
        showErrorMessage(title, result.getErrorMessage());
    }
}

void MainComponent::updateProcessorClassFromEditor()
{
    if (updatingUi)
        return;

    projectManager.setProcessorClassName(processorClassEditor.getText());
    refreshProjectState();
}

void MainComponent::handleProjectNavigatorSelection(const juce::String& relativePath)
{
    navigatorSelectionPath = relativePath;

    const auto* node = projectManager.getNodeForPath(relativePath);

    if (node != nullptr && node->type == UserDspProjectNodeType::file)
    {
        if (const auto result = projectManager.selectFile(relativePath); result.failed())
            showErrorMessage("File Open Failed", result.getErrorMessage());
    }

    projectNavigatorNeedsRefresh = false;
    refreshProjectState();
}

void MainComponent::showProjectContextMenu(const juce::String& relativePath, juce::Point<int> screenPosition)
{
    const auto* node = projectManager.getNodeForPath(relativePath);

    if (node == nullptr)
        return;

    juce::PopupMenu menu;
    menu.setLookAndFeel(ideLookAndFeel.get());

    const auto isRoot = relativePath.isEmpty();
    const auto isFolder = node->type == UserDspProjectNodeType::folder;

    if (isRoot || isFolder)
    {
        menu.addItem(1, "New File");
        menu.addItem(2, "New Folder");
        menu.addItem(3, "Import CPP");
        menu.addSeparator();
    }

    if (! isRoot)
    {
        menu.addItem(4, "Rename");
        menu.addItem(5, "Move");
        menu.addItem(6, "Delete");

        if (! isFolder)
        {
            menu.addSeparator();
            menu.addItem(7, "Export CPP");
        }
    }

    const auto selected = menu.showMenu(juce::PopupMenu::Options()
                                            .withTargetScreenArea(juce::Rectangle<int>(screenPosition.x, screenPosition.y, 1, 1))
                                            .withParentComponent(this));
    menu.setLookAndFeel(nullptr);

    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    auto scheduleAction = [safeThis] (std::function<void(MainComponent&)> callback)
    {
        auto deferredCallback = std::move(callback);

        juce::MessageManager::callAsync([safeThis, task = std::move(deferredCallback)]() mutable
        {
            if (safeThis != nullptr)
                task(*safeThis);
        });
    };

    switch (selected)
    {
        case 1:
            scheduleAction([path = isFolder ? relativePath : juce::String()] (MainComponent& self) { self.promptCreateFile(path); });
            break;

        case 2:
            scheduleAction([path = isFolder ? relativePath : juce::String()] (MainComponent& self) { self.promptCreateFolder(path); });
            break;

        case 3:
            scheduleAction([path = isFolder ? relativePath : juce::String()] (MainComponent& self) { self.promptImportCpp(path); });
            break;

        case 4:
            scheduleAction([path = relativePath] (MainComponent& self) { self.promptRenameItem(path); });
            break;

        case 5:
            scheduleAction([path = relativePath] (MainComponent& self) { self.promptMoveItem(path); });
            break;

        case 6:
            scheduleAction([path = relativePath] (MainComponent& self) { self.promptDeleteItem(path); });
            break;

        case 7:
            scheduleAction([path = relativePath] (MainComponent& self) { self.promptExportFile(path); });
            break;

        default: break;
    }
}

void MainComponent::promptCreateFile(const juce::String& parentFolderPath)
{
    auto safeThis = juce::Component::SafePointer<MainComponent>(this);

    showTextInputDialogAsync("New File",
                             "Enter a file name with .cpp, .h or .hpp extension.",
                             "NewFile.cpp",
                             "fileName",
                             [safeThis, parentFolderPath] (juce::String fileName)
                             {
                                 if (fileName.isEmpty() || safeThis == nullptr)
                                     return;

                                 if (const auto result = safeThis->projectManager.createFile(parentFolderPath, fileName); result.failed())
                                 {
                                     safeThis->showErrorMessage("Create File Failed", result.getErrorMessage());
                                     return;
                                 }

                                 safeThis->navigatorSelectionPath = safeThis->projectManager.getActiveFilePath();
                                 safeThis->projectNavigatorNeedsRefresh = true;
                                 safeThis->refreshProjectState();
                             });
}

void MainComponent::promptCreateFolder(const juce::String& parentFolderPath)
{
    auto safeThis = juce::Component::SafePointer<MainComponent>(this);

    showTextInputDialogAsync("New Folder",
                             "Enter a folder name.",
                             "NewFolder",
                             "folderName",
                             [safeThis, parentFolderPath] (juce::String folderName)
                             {
                                 if (folderName.isEmpty() || safeThis == nullptr)
                                     return;

                                 if (const auto result = safeThis->projectManager.createFolder(parentFolderPath, folderName); result.failed())
                                 {
                                     safeThis->showErrorMessage("Create Folder Failed", result.getErrorMessage());
                                     return;
                                 }

                                 safeThis->navigatorSelectionPath = parentFolderPath.isEmpty() ? juce::String() : parentFolderPath;
                                 safeThis->projectNavigatorNeedsRefresh = true;
                                 safeThis->refreshProjectState();
                             });
}

void MainComponent::promptRenameItem(const juce::String& relativePath)
{
    const auto* node = projectManager.getNodeForPath(relativePath);

    if (node == nullptr)
        return;

    auto safeThis = juce::Component::SafePointer<MainComponent>(this);

    showTextInputDialogAsync("Rename Item",
                             "Enter a new name.",
                             node->name,
                             "itemName",
                             [safeThis, relativePath] (juce::String newName)
                             {
                                 if (newName.isEmpty() || safeThis == nullptr)
                                     return;

                                 if (const auto result = safeThis->projectManager.renameItem(relativePath, newName); result.failed())
                                 {
                                     safeThis->showErrorMessage("Rename Failed", result.getErrorMessage());
                                     return;
                                 }

                                 safeThis->navigatorSelectionPath = relativePath.isNotEmpty() ? parentPathFromRelativePath(relativePath) + "/" + newName : newName;
                                 safeThis->navigatorSelectionPath = safeThis->navigatorSelectionPath.trimCharactersAtStart("/");
                                 safeThis->projectNavigatorNeedsRefresh = true;
                                 safeThis->refreshProjectState();
                             });
}

void MainComponent::promptMoveItem(const juce::String& relativePath)
{
    const auto destinations = projectManager.getAllowedMoveDestinations(relativePath);

    if (destinations.isEmpty())
    {
        showErrorMessage("Move Item", "No valid destination folders are available.");
        return;
    }

    juce::StringArray destinationLabels;

    for (const auto& destination : destinations)
        destinationLabels.add(destination.isEmpty() ? projectManager.getProjectName() : destination);

    auto safeThis = juce::Component::SafePointer<MainComponent>(this);

    showChoiceDialogAsync("Move Item",
                          "Choose the destination folder.",
                          destinationLabels,
                          [safeThis, relativePath, destinations] (int selectedIndex)
                          {
                              if (! juce::isPositiveAndBelow(selectedIndex, destinations.size()) || safeThis == nullptr)
                                  return;

                              const auto destination = destinations[static_cast<int>(selectedIndex)];

                              if (const auto result = safeThis->projectManager.moveItem(relativePath, destination); result.failed())
                              {
                                  safeThis->showErrorMessage("Move Failed", result.getErrorMessage());
                                  return;
                              }

                              safeThis->navigatorSelectionPath = destination;
                              safeThis->projectNavigatorNeedsRefresh = true;
                              safeThis->refreshProjectState();
                          });
}

void MainComponent::promptDeleteItem(const juce::String& relativePath)
{
    auto safeThis = juce::Component::SafePointer<MainComponent>(this);

    showOkCancelDialogAsync("Delete Item",
                            "Delete " + relativePath + "?",
                            "Delete",
                            [safeThis, relativePath] (bool shouldDelete)
                            {
                                if (! shouldDelete || safeThis == nullptr)
                                    return;

                                if (const auto result = safeThis->projectManager.deleteItem(relativePath); result.failed())
                                {
                                    safeThis->showErrorMessage("Delete Failed", result.getErrorMessage());
                                    return;
                                }

                                safeThis->navigatorSelectionPath = safeThis->projectManager.getActiveFilePath();
                                safeThis->projectNavigatorNeedsRefresh = true;
                                safeThis->refreshProjectState();
                            });
}

void MainComponent::promptImportCpp(const juce::String& destinationFolderPath)
{
    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    activeChooser = std::make_unique<juce::FileChooser>("Import user DSP source", juce::File(), "*.cpp");
    activeChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                               [safeThis, destinationFolderPath] (const juce::FileChooser& chooser)
                               {
                                   if (safeThis == nullptr)
                                       return;

                                   const auto selectedFile = chooser.getResult();

                                   if (! selectedFile.existsAsFile())
                                       return;

                                   juce::String importedRelativePath;

                                   if (const auto result = safeThis->projectManager.importFile(selectedFile, destinationFolderPath, &importedRelativePath); result.failed())
                                   {
                                       safeThis->showErrorMessage("Import Failed", result.getErrorMessage());
                                       return;
                                   }

                                   safeThis->navigatorSelectionPath = importedRelativePath;
                                   safeThis->projectNavigatorNeedsRefresh = true;
                                   safeThis->refreshProjectState();
                               });
}

void MainComponent::promptExportFile(const juce::String& relativePath)
{
    const auto fileName = juce::File(relativePath).getFileName();
    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    activeChooser = std::make_unique<juce::FileChooser>("Export user DSP source",
                                                        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(fileName),
                                                        "*.cpp");
    activeChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                               [safeThis, relativePath] (const juce::FileChooser& chooser)
                               {
                                   if (safeThis == nullptr)
                                       return;

                                   auto outputFile = chooser.getResult();

                                   if (outputFile == juce::File())
                                       return;

                                   if (outputFile.getFileExtension().isEmpty())
                                       outputFile = outputFile.withFileExtension(".cpp");

                                   if (const auto result = safeThis->projectManager.exportFile(relativePath, outputFile); result.failed())
                                       safeThis->showErrorMessage("Export Failed", result.getErrorMessage());
                               });
}

void MainComponent::showErrorMessage(const juce::String& title, const juce::String& message)
{
    const auto shouldResumeTimer = pauseRefreshTimer();
    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    auto* window = new juce::AlertWindow(title, message, juce::MessageBoxIconType::WarningIcon, this);
    configureAlertWindowLookAndFeel(*window, ideLookAndFeel.get());
    window->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey));

    auto safeWindow = juce::Component::SafePointer<juce::AlertWindow>(window);

    window->enterModalState(true,
                            juce::ModalCallbackFunction::create(
                                [safeThis, safeWindow, shouldResumeTimer] (int)
                                {
                                    if (safeWindow != nullptr)
                                        safeWindow->setLookAndFeel(nullptr);

                                    if (safeThis != nullptr)
                                        safeThis->resumeRefreshTimer(shouldResumeTimer);
                                }),
                            true);
}
