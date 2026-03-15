#pragma once

#include <JuceHeader.h>

#include "audio/AudioEngine.h"
#include "ui/DarkIdeLookAndFeel.h"
#include "ui/ScrollPassthroughControls.h"
#include "userdsp/UserDspCompiler.h"
#include "userdsp/UserDspProjectManager.h"

class ProjectTreeItem;
class SidePanelResizeBar;
class ToolWindow;

class MainComponent final : public juce::Component,
                            private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool handleCloseRequest();

private:
    void timerCallback() override;

    void configureSlider(juce::Slider& slider, double minValue, double maxValue, double step, const juce::String& suffix = {});
    void configureButtonsAndCallbacks();
    void populateCombos();
    void refreshEngineState();
    void refreshAudioDeviceControls();
    void refreshUserControls();
    void refreshCompilerState();
    void refreshProjectState();
    void refreshProjectNavigator();
    void refreshPanelButtons();
    void applyDarkTheme();
    void applyCodeEditorAppearance();
    void updatePanelAnimation();
    void launchLoadWavChooser();
    void saveProject(bool forceSaveAs, std::function<void(bool)> completion = {});
    void confirmProjectTransition(const juce::String& actionName, std::function<void(bool)> completion);
    void promptForNewProject();
    void promptForOpenProject();
    void compileProject();
    void updateProcessorClassFromEditor();
    void handleProjectNavigatorSelection(const juce::String& relativePath);
    void showProjectContextMenu(const juce::String& relativePath, juce::Point<int> screenPosition);
    void promptCreateFile(const juce::String& parentFolderPath);
    void promptCreateFolder(const juce::String& parentFolderPath);
    void promptRenameItem(const juce::String& relativePath);
    void promptMoveItem(const juce::String& relativePath);
    void promptDeleteItem(const juce::String& relativePath);
    void promptImportCpp(const juce::String& destinationFolderPath);
    void promptExportFile(const juce::String& relativePath);
    void toggleLeftPanel();
    void toggleNavigatorPanel();
    void resizeNavigatorPanel(float deltaX);
    void showOscilloscopeWindow();
    void showControlsWindow();
    void ensureOscilloscopeWindow();
    void ensureControlsWindow();
    void applyCurrentProjectAudioState();
    void updateProjectAudioState(const std::function<void(ProjectAudioState&)>& mutator);
    void setToolWindowRefreshSuspended(bool shouldSuspend);
    bool pauseRefreshTimer();
    void resumeRefreshTimer(bool shouldResume);
    void showTextInputDialogAsync(const juce::String& title,
                                  const juce::String& message,
                                  const juce::String& defaultValue,
                                  const juce::String& editorName,
                                  std::function<void(juce::String)> completion);
    void showChoiceDialogAsync(const juce::String& title,
                               const juce::String& message,
                               const juce::StringArray& options,
                               std::function<void(int)> completion);
    void showSaveDiscardCancelDialogAsync(const juce::String& title,
                                          const juce::String& message,
                                          std::function<void(int)> completion);
    void showOkCancelDialogAsync(const juce::String& title,
                                 const juce::String& message,
                                 const juce::String& okText,
                                 std::function<void(bool)> completion);
    void showErrorMessage(const juce::String& title, const juce::String& message);

    friend class ProjectTreeItem;
    friend class SidePanelResizeBar;

    AudioEngine audioEngine;
    UserDspProjectManager projectManager;
    UserDspCompiler compiler;
    juce::CodeEditorComponent codeEditor;

    juce::Label sourceHeader;
    juce::Label transportHeader;
    juce::Label wavHeader;
    juce::Label dspHeader;
    juce::Label deviceHeader;
    juce::Label userHeader;
    juce::Label toolsHeader;
    juce::Label editorHeader;
    juce::Label navigatorHeader;
    juce::Label logHeader;
    juce::Label projectPathLabel;
    juce::Label codeFontSizeLabel;
    juce::Label codeFontSizeValueLabel;

    ScrollPassthroughComboBox sourceCombo;
    juce::TextButton transportButton { "Start Audio" };
    ScrollPassthroughSlider gainSlider;
    ScrollPassthroughSlider frequencySlider;
    juce::Label gainLabel;
    juce::Label frequencyLabel;
    juce::Label inputDeviceLabel;
    juce::Label outputDeviceLabel;
    juce::Label midiInputLabel;
    juce::Label sampleRateLabel;
    juce::Label blockSizeLabel;
    juce::Label inputRoutingLabel;
    juce::Label outputRoutingLabel;
    juce::Label midiInputStatusLabel;
    juce::Label preferredAudioStatusLabel;
    juce::Label requestedAudioStatusLabel;
    juce::Label actualAudioStatusLabel;
    juce::Label overrideAudioStatusLabel;
    juce::Label warningAudioStatusLabel;
    ScrollPassthroughComboBox inputDeviceCombo;
    ScrollPassthroughComboBox outputDeviceCombo;
    ScrollPassthroughComboBox midiInputCombo;
    ScrollPassthroughComboBox sampleRateCombo;
    ScrollPassthroughComboBox blockSizeCombo;
    std::array<ScrollPassthroughComboBox, DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS> inputRoutingCombos;
    std::array<ScrollPassthroughComboBox, DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS> outputRoutingCombos;

    juce::TextButton loadWavButton { "Load WAV" };
    juce::Label wavFileLabel;
    juce::ToggleButton wavLoopToggle { "Loop" };
    ScrollPassthroughSlider wavPositionSlider;
    juce::Label wavPositionLabel;

    juce::Label userModuleStatusLabel;

    juce::TextButton newProjectButton { "New Project" };
    juce::TextButton openProjectButton { "Open Project" };
    juce::TextButton saveProjectButton { "Save Project" };
    juce::TextButton saveProjectAsButton { "Save Project As" };
    juce::TextButton compileButton { "Compile" };
    juce::TextButton showOscilloscopeButton { "Oscilloscope" };
    juce::TextButton showControlsButton { "Controls" };
    juce::TextButton toggleLeftPanelButton { "<" };
    juce::TextButton toggleNavigatorButton { ">" };
    ScrollPassthroughSlider codeFontSizeSlider;
    juce::Label processorClassLabel;
    juce::TextEditor processorClassEditor;
    juce::Label compileStatusLabel;
    juce::TextEditor buildLogEditor;
    juce::TreeView projectTreeView;

    juce::Component controlsPanel;
    ScrollPassthroughViewport controlsViewport;
    juce::Component controlsContent;
    juce::Component leftPanelToggleStrip;
    juce::Component editorToolbarPanel;
    juce::Component editorOptionsPanel;
    juce::Component workspacePanel;
    juce::Component editorPanel;
    juce::Component navigatorPanel;
    juce::Component navigatorToggleStrip;
    juce::Component logPanel;

    juce::StretchableLayoutManager workspaceLayout;
    juce::StretchableLayoutResizerBar workspaceResizerBar { &workspaceLayout, 1, false };
    std::unique_ptr<SidePanelResizeBar> navigatorResizeBar;

    std::unique_ptr<juce::FileChooser> activeChooser;
    std::unique_ptr<ToolWindow> oscilloscopeWindow;
    std::unique_ptr<ToolWindow> controlsWindow;
    std::unique_ptr<ide::DarkIdeLookAndFeel> ideLookAndFeel;
    int lastSeenUserModuleGeneration = 0;
    juce::String navigatorSelectionPath;
    float codeFontSize = 17.0f;
    float controlsPanelExpandedWidth = 360.0f;
    float controlsPanelCurrentWidth = 360.0f;
    float controlsPanelTargetWidth = 360.0f;
    float navigatorPanelExpandedWidth = 280.0f;
    float navigatorPanelCurrentWidth = 280.0f;
    float navigatorPanelTargetWidth = 280.0f;
    bool controlsPanelCollapsed = false;
    bool navigatorPanelCollapsed = false;
    bool projectNavigatorNeedsRefresh = true;
    bool updatingUi = false;
};
