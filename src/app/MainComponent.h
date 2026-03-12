#pragma once

#include <JuceHeader.h>

#include "audio/AudioEngine.h"
#include "presets/PresetManager.h"
#include "ui/OscilloscopeComponent.h"
#include "userdsp/UserCodeDocumentManager.h"
#include "userdsp/UserDspCompiler.h"

class MainComponent final : public juce::Component,
                            private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    void configureSlider(juce::Slider& slider, double minValue, double maxValue, double step, const juce::String& suffix = {});
    void configureButtonsAndCallbacks();
    void populateCombos();
    void refreshEngineState();
    void refreshBuiltinControls();
    void refreshUserControls();
    void refreshCompilerState();
    void launchLoadWavChooser();
    void launchSavePresetChooser();
    void launchLoadPresetChooser();
    void launchImportCodeChooser();
    void launchExportCodeChooser();
    void showErrorMessage(const juce::String& title, const juce::String& message);

    AudioEngine audioEngine;
    PresetManager presetManager;
    UserCodeDocumentManager codeManager;
    UserDspCompiler compiler;
    OscilloscopeComponent oscilloscope;
    juce::CodeEditorComponent codeEditor;

    juce::Label sourceHeader;
    juce::Label transportHeader;
    juce::Label wavHeader;
    juce::Label dspHeader;
    juce::Label builtinHeader;
    juce::Label userHeader;
    juce::Label editorHeader;
    juce::Label logHeader;

    juce::ComboBox sourceCombo;
    juce::TextButton startButton { "Start Audio" };
    juce::TextButton stopButton { "Stop Audio" };
    juce::Slider gainSlider;
    juce::Slider frequencySlider;
    juce::Label gainLabel;
    juce::Label frequencyLabel;
    juce::Label sampleRateLabel;
    juce::Label blockSizeLabel;

    juce::TextButton loadWavButton { "Load WAV" };
    juce::Label wavFileLabel;
    juce::ToggleButton wavLoopToggle { "Loop" };
    juce::Slider wavPositionSlider;
    juce::Label wavPositionLabel;

    juce::ComboBox processingModeCombo;
    juce::ComboBox builtinProcessorCombo;
    juce::TextButton savePresetButton { "Save Preset" };
    juce::TextButton loadPresetButton { "Load Preset" };

    std::array<juce::Label, 4> builtinParameterLabels;
    std::array<juce::Slider, 4> builtinParameterSliders;
    std::array<juce::Label, 4> userParameterLabels;
    std::array<juce::Slider, 4> userParameterSliders;

    juce::TextButton compileButton { "Compile" };
    juce::TextButton importCodeButton { "Import Code" };
    juce::TextButton exportCodeButton { "Export Code" };
    juce::TextButton resetCodeButton { "Reset Template" };
    juce::Label compileStatusLabel;
    juce::TextEditor buildLogEditor;

    std::unique_ptr<juce::FileChooser> activeChooser;
    bool updatingUi = false;
};
