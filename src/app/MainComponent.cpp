#include "app/MainComponent.h"

namespace
{
void configureSectionLabel(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
    label.setJustificationType(juce::Justification::centredLeft);
    label.setColour(juce::Label::textColourId, juce::Colour(0xff1f2933));
}

void configureBodyLabel(juce::Label& label, const juce::String& text)
{
    label.setText(text, juce::dontSendNotification);
    label.setColour(juce::Label::textColourId, juce::Colour(0xff1f2933));
}
}

MainComponent::MainComponent()
    : compiler(audioEngine.getUserDspHost()),
      oscilloscope(audioEngine.getOscilloscopeBuffer()),
      codeEditor(codeManager.getDocument(), &codeManager.getTokeniser())
{
    configureSectionLabel(sourceHeader, "Source");
    configureSectionLabel(transportHeader, "Transport");
    configureSectionLabel(wavHeader, "WAV Source");
    configureSectionLabel(dspHeader, "DSP Mode");
    configureSectionLabel(builtinHeader, "Built-in DSP");
    configureSectionLabel(userHeader, "User DSP DLL");
    configureSectionLabel(editorHeader, "Code Editor");
    configureSectionLabel(logHeader, "Compile Log");

    configureBodyLabel(gainLabel, "Gain");
    configureBodyLabel(frequencyLabel, "Sine Frequency");
    configureBodyLabel(wavPositionLabel, "Position");
    configureBodyLabel(sampleRateLabel, "Sample Rate: --");
    configureBodyLabel(blockSizeLabel, "Block Size: --");
    configureBodyLabel(wavFileLabel, "No WAV loaded");
    configureBodyLabel(compileStatusLabel, "Idle");

    configureSlider(gainSlider, 0.0, 1.0, 0.001, "x");
    configureSlider(frequencySlider, 20.0, 2000.0, 1.0, " Hz");
    configureSlider(wavPositionSlider, 0.0, 1.0, 0.0001);

    gainSlider.setValue(0.2, juce::dontSendNotification);
    frequencySlider.setValue(440.0, juce::dontSendNotification);

    for (auto& label : builtinParameterLabels)
        configureBodyLabel(label, {});

    for (auto& label : userParameterLabels)
        configureBodyLabel(label, {});

    for (auto& slider : builtinParameterSliders)
        configureSlider(slider, 0.0, 1.0, 0.001);

    for (auto& slider : userParameterSliders)
        configureSlider(slider, 0.0, 1.0, 0.001);

    buildLogEditor.setMultiLine(true);
    buildLogEditor.setReadOnly(true);
    buildLogEditor.setScrollbarsShown(true);
    buildLogEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colours::white);
    buildLogEditor.setColour(juce::TextEditor::textColourId, juce::Colour(0xff1f2933));
    buildLogEditor.setColour(juce::TextEditor::highlightColourId, juce::Colour(0xffdbeafe));
    buildLogEditor.setColour(juce::TextEditor::highlightedTextColourId, juce::Colour(0xff111827));
    buildLogEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xffcbd5e1));
    buildLogEditor.setColour(juce::TextEditor::shadowColourId, juce::Colours::transparentBlack);

    codeEditor.setColour(juce::CodeEditorComponent::backgroundColourId, juce::Colours::white);
    codeEditor.setColour(juce::CodeEditorComponent::defaultTextColourId, juce::Colour(0xff111827));
    codeEditor.setColour(juce::CodeEditorComponent::highlightColourId, juce::Colour(0xffdbeafe));
    codeEditor.setColour(juce::CodeEditorComponent::lineNumberBackgroundId, juce::Colour(0xfff3f4f6));
    codeEditor.setColour(juce::CodeEditorComponent::lineNumberTextId, juce::Colour(0xff6b7280));
    codeEditor.setTabSize(4, true);

    populateCombos();
    configureButtonsAndCallbacks();

    for (auto* component : std::array<juce::Component*, 34>
         {
             &sourceHeader, &transportHeader, &wavHeader, &dspHeader, &builtinHeader, &userHeader, &editorHeader, &logHeader,
             &sourceCombo, &startButton, &stopButton, &gainLabel, &gainSlider, &frequencyLabel, &frequencySlider, &sampleRateLabel,
             &blockSizeLabel, &loadWavButton, &wavFileLabel, &wavLoopToggle, &wavPositionLabel, &wavPositionSlider, &processingModeCombo,
             &builtinProcessorCombo, &savePresetButton, &loadPresetButton, &compileButton, &importCodeButton, &exportCodeButton,
             &resetCodeButton, &compileStatusLabel, &buildLogEditor, &oscilloscope, &codeEditor
         })
    {
        addAndMakeVisible(component);
    }

    for (auto& label : builtinParameterLabels)
        addAndMakeVisible(label);

    for (auto& slider : builtinParameterSliders)
        addAndMakeVisible(slider);

    for (auto& label : userParameterLabels)
        addAndMakeVisible(label);

    for (auto& slider : userParameterSliders)
        addAndMakeVisible(slider);

    audioEngine.setSourceGain(0.2f);
    audioEngine.setSineFrequency(440.0f);
    audioEngine.setBuiltinProcessorType(BuiltinProcessorType::bypass);

    if (const auto initResult = audioEngine.initialise(); initResult.failed())
        showErrorMessage("Audio Initialisation Failed", initResult.getErrorMessage());

    refreshBuiltinControls();
    refreshUserControls();
    refreshEngineState();
    refreshCompilerState();
    setSize(1400, 900);
    startTimerHz(10);
}

MainComponent::~MainComponent()
{
    stopTimer();
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xfff8fafc));
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds().reduced(10);
    auto controlsArea = bounds.removeFromLeft(360).reduced(0, 4);
    auto contentArea = bounds.reduced(4, 4);

    auto placeRow = [] (juce::Rectangle<int>& area, int height)
    {
        auto row = area.removeFromTop(height);
        area.removeFromTop(4);
        return row;
    };

    sourceHeader.setBounds(placeRow(controlsArea, 24));
    sourceCombo.setBounds(placeRow(controlsArea, 26));

    transportHeader.setBounds(placeRow(controlsArea, 24));
    auto transportRow = placeRow(controlsArea, 28);
    startButton.setBounds(transportRow.removeFromLeft(transportRow.getWidth() / 2).reduced(0, 0).reduced(0, 0).reduced(0, 0).reduced(0, 0).reduced(2, 0));
    stopButton.setBounds(transportRow.reduced(2, 0));

    gainLabel.setBounds(placeRow(controlsArea, 20));
    gainSlider.setBounds(placeRow(controlsArea, 28));
    frequencyLabel.setBounds(placeRow(controlsArea, 20));
    frequencySlider.setBounds(placeRow(controlsArea, 28));
    sampleRateLabel.setBounds(placeRow(controlsArea, 20));
    blockSizeLabel.setBounds(placeRow(controlsArea, 20));

    wavHeader.setBounds(placeRow(controlsArea, 24));
    loadWavButton.setBounds(placeRow(controlsArea, 28));
    wavFileLabel.setBounds(placeRow(controlsArea, 20));
    wavLoopToggle.setBounds(placeRow(controlsArea, 24));
    wavPositionLabel.setBounds(placeRow(controlsArea, 20));
    wavPositionSlider.setBounds(placeRow(controlsArea, 28));

    dspHeader.setBounds(placeRow(controlsArea, 24));
    processingModeCombo.setBounds(placeRow(controlsArea, 28));

    builtinHeader.setBounds(placeRow(controlsArea, 24));
    builtinProcessorCombo.setBounds(placeRow(controlsArea, 28));

    for (int index = 0; index < static_cast<int>(builtinParameterLabels.size()); ++index)
    {
        builtinParameterLabels[static_cast<std::size_t>(index)].setBounds(placeRow(controlsArea, 20));
        builtinParameterSliders[static_cast<std::size_t>(index)].setBounds(placeRow(controlsArea, 28));
    }

    auto presetRow = placeRow(controlsArea, 28);
    savePresetButton.setBounds(presetRow.removeFromLeft(presetRow.getWidth() / 2).reduced(2, 0));
    loadPresetButton.setBounds(presetRow.reduced(2, 0));

    userHeader.setBounds(placeRow(controlsArea, 24));

    for (int index = 0; index < static_cast<int>(userParameterLabels.size()); ++index)
    {
        userParameterLabels[static_cast<std::size_t>(index)].setBounds(placeRow(controlsArea, 20));
        userParameterSliders[static_cast<std::size_t>(index)].setBounds(placeRow(controlsArea, 28));
    }

    auto oscilloscopeArea = contentArea.removeFromTop(210);
    oscilloscope.setBounds(oscilloscopeArea);

    auto logArea = contentArea.removeFromBottom(220);
    auto editorToolbar = contentArea.removeFromTop(26);
    editorHeader.setBounds(editorToolbar.removeFromLeft(140));
    compileButton.setBounds(editorToolbar.removeFromLeft(120).reduced(2, 0));
    importCodeButton.setBounds(editorToolbar.removeFromLeft(110).reduced(2, 0));
    exportCodeButton.setBounds(editorToolbar.removeFromLeft(110).reduced(2, 0));
    resetCodeButton.setBounds(editorToolbar.removeFromLeft(120).reduced(2, 0));
    contentArea.removeFromTop(4);
    codeEditor.setBounds(contentArea);

    logHeader.setBounds(logArea.removeFromTop(24));
    compileStatusLabel.setBounds(logArea.removeFromTop(24));
    logArea.removeFromTop(4);
    buildLogEditor.setBounds(logArea);
}

void MainComponent::timerCallback()
{
    audioEngine.getUserDspHost().reclaimRetiredRuntimes();
    refreshEngineState();
    refreshBuiltinControls();
    refreshUserControls();
    refreshCompilerState();
}

void MainComponent::configureSlider(juce::Slider& slider, double minValue, double maxValue, double step, const juce::String& suffix)
{
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 90, 22);
    slider.setRange(minValue, maxValue, step);
    slider.setTextValueSuffix(suffix);
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xff111827));
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::white);
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xffcbd5e1));
    slider.setColour(juce::Slider::trackColourId, juce::Colour(0xff93c5fd));
    slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff2563eb));
}

void MainComponent::configureButtonsAndCallbacks()
{
    startButton.onClick = [this] { audioEngine.startTransport(); };
    stopButton.onClick = [this] { audioEngine.stopTransport(); };
    loadWavButton.onClick = [this] { launchLoadWavChooser(); };
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

    for (int index = 0; index < static_cast<int>(userParameterSliders.size()); ++index)
    {
        userParameterSliders[static_cast<std::size_t>(index)].onValueChange = [this, index]
        {
            if (! updatingUi)
                audioEngine.setUserParameter(index, static_cast<float>(userParameterSliders[static_cast<std::size_t>(index)].getValue()));
        };
    }

    savePresetButton.onClick = [this] { launchSavePresetChooser(); };
    loadPresetButton.onClick = [this] { launchLoadPresetChooser(); };

    compileButton.onClick = [this]
    {
        if (! compiler.compileAsync(codeManager.getCodeText()))
            showErrorMessage("Compilation Busy", "A user DSP compilation is already running.");
    };

    importCodeButton.onClick = [this] { launchImportCodeChooser(); };
    exportCodeButton.onClick = [this] { launchExportCodeChooser(); };
    resetCodeButton.onClick = [this] { codeManager.resetToTemplate(); };
}

void MainComponent::populateCombos()
{
    for (const auto source : getAvailableSourceTypes())
        sourceCombo.addItem(sourceTypeToDisplayName(source), static_cast<int>(source) + 1);

    sourceCombo.setSelectedId(static_cast<int>(SourceType::sine) + 1, juce::dontSendNotification);

    processingModeCombo.addItem("Built-in DSP", 1);
    processingModeCombo.addItem("User DSP DLL", 2);
    processingModeCombo.setSelectedId(1, juce::dontSendNotification);

    for (const auto& descriptor : getBuiltinProcessorDescriptors())
        builtinProcessorCombo.addItem(descriptor.name, static_cast<int>(descriptor.type) + 1);

    builtinProcessorCombo.setSelectedId(static_cast<int>(BuiltinProcessorType::bypass) + 1, juce::dontSendNotification);
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

    if (snapshot.deviceError.isNotEmpty())
        compileStatusLabel.setText(snapshot.deviceError, juce::dontSendNotification);

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
    const auto userModeEnabled = processingModeCombo.getSelectedId() == 2;

    updatingUi = true;

    userHeader.setText(userModeEnabled ? "User DSP DLL: " + snapshot.processorName : "User DSP DLL", juce::dontSendNotification);

    for (int index = 0; index < static_cast<int>(userParameterSliders.size()); ++index)
    {
        const auto& parameter = snapshot.parameters[static_cast<std::size_t>(index)];
        auto& label = userParameterLabels[static_cast<std::size_t>(index)];
        auto& slider = userParameterSliders[static_cast<std::size_t>(index)];
        const auto visible = parameter.active;

        label.setVisible(visible);
        slider.setVisible(visible);
        slider.setEnabled(visible && userModeEnabled && snapshot.hasActiveModule);

        if (visible)
        {
            label.setText(parameter.name, juce::dontSendNotification);
            slider.setRange(parameter.minValue, parameter.maxValue, 0.0001);
            slider.setValue(parameter.currentValue, juce::dontSendNotification);
        }
    }

    updatingUi = false;
}

void MainComponent::refreshCompilerState()
{
    const auto snapshot = compiler.getSnapshot();

    compileStatusLabel.setText(snapshot.statusText, juce::dontSendNotification);
    buildLogEditor.setText(snapshot.logText, juce::dontSendNotification);
    compileButton.setEnabled(snapshot.state != UserDspCompiler::State::compiling);
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

void MainComponent::launchImportCodeChooser()
{
    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    activeChooser = std::make_unique<juce::FileChooser>("Import user DSP code", juce::File(), "*.cpp");
    activeChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                               [safeThis] (const juce::FileChooser& chooser)
                               {
                                   if (safeThis == nullptr)
                                       return;

                                   const auto file = chooser.getResult();

                                   if (! file.existsAsFile())
                                       return;

                                   if (const auto result = safeThis->codeManager.loadFromFile(file); result.failed())
                                       safeThis->showErrorMessage("Import Failed", result.getErrorMessage());
                               });
}

void MainComponent::launchExportCodeChooser()
{
    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    activeChooser = std::make_unique<juce::FileChooser>("Export user DSP code", juce::File(), "*.cpp");
    activeChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                               [safeThis] (const juce::FileChooser& chooser)
                               {
                                   if (safeThis == nullptr)
                                       return;

                                   auto file = chooser.getResult();

                                   if (file == juce::File())
                                       return;

                                   if (file.getFileExtension().isEmpty())
                                       file = file.withFileExtension(".cpp");

                                   if (const auto result = safeThis->codeManager.saveToFile(file); result.failed())
                                       safeThis->showErrorMessage("Export Failed", result.getErrorMessage());
                               });
}

void MainComponent::showErrorMessage(const juce::String& title, const juce::String& message)
{
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, title, message);
}
