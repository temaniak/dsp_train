#pragma once

#include <JuceHeader.h>

#include <memory>
#include <vector>

#include "audio/AudioEngine.h"
#include "ui/ScrollPassthroughControls.h"
#include "userdsp/UserDspProjectManager.h"

class ControllerTileComponent;

class RealtimeControlsComponent final : public juce::Component,
                                        private juce::Timer
{
public:
    RealtimeControlsComponent(AudioEngine& audioEngineToControl,
                              UserDspProjectManager& projectManagerToEdit);
    ~RealtimeControlsComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void setRefreshSuspended(bool shouldSuspend);

private:
    enum class Mode
    {
        edit,
        play
    };

    void timerCallback() override;

    void setMode(Mode nextMode);
    void addController(UserDspControllerType type);
    void editController(int index);
    void deleteController(int index);
    void moveController(int sourceIndex, int destinationIndex);
    void applyControllerValue(int index, float value);
    void syncToProjectAndRuntime();
    void rebuildTiles();
    void updateTileLayout();
    void showErrorMessage(const juce::String& title, const juce::String& message);

    AudioEngine& audioEngine;
    UserDspProjectManager& projectManager;

    juce::Label titleLabel;
    juce::Label statusLabel;
    juce::Label hintLabel;
    juce::Label modeLabel;
    juce::Label midiModeHintLabel;
    juce::TextButton editModeButton { "Edit" };
    juce::TextButton playModeButton { "Play" };
    juce::TextButton addKnobButton { "Add Knob" };
    juce::TextButton addButtonButton { "Add Button" };
    juce::TextButton addToggleButton { "Add Toggle" };
    ScrollPassthroughViewport tilesViewport;
    juce::Component tilesContent;
    std::vector<std::unique_ptr<ControllerTileComponent>> tiles;
    std::vector<UserDspControllerDefinition> lastDefinitions;
    std::vector<float> previewValues;
    bool previewValuesNeedPush = false;
    bool layoutMatchesRuntime = false;
    Mode mode = Mode::play;
};
