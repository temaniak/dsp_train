#pragma once

#include <JuceHeader.h>

#include "userdsp/UserDspProjectManager.h"
#include "userdsp/UserDspHost.h"

class UserDspCompiler
{
public:
    enum class State
    {
        idle,
        compiling,
        succeeded,
        failed
    };

    struct Snapshot
    {
        State state = State::idle;
        juce::String statusText = "Idle";
        juce::String logText;
        juce::String lastSourcePath;
        juce::String lastModulePath;
    };

    explicit UserDspCompiler(UserDspHost& hostToUpdate);
    ~UserDspCompiler();

    juce::Result compileAsync(const UserDspProjectSnapshot& projectSnapshot);
    Snapshot getSnapshot() const;

private:
    void runCompilation(UserDspProjectSnapshot projectSnapshot);
    void setSnapshot(const Snapshot& newSnapshot);
    void updateState(State state, const juce::String& statusText, const juce::String& logText);
    juce::String createVersionTag();

    UserDspHost& host;
    mutable juce::CriticalSection snapshotLock;
    Snapshot snapshot;
    std::atomic<bool> compileInProgress { false };
    std::thread worker;
    std::atomic<int> compileCounter { 0 };
};
