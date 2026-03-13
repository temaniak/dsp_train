# HANDOFF

This handoff reflects the repository state after the user-DSP-only cleanup completed on 2026-03-13.

## 1. Product Summary

`DSP Education Stand` is a JUCE standalone desktop app for learning DSP by editing C++ user projects.

The app is now centered on one workflow:

- choose a source signal
- edit a user DSP project
- define runtime controls
- compile and hot-reload the module

Built-in DSP processors and preset management were removed from the product and from the active build.

## 2. What Works

- JUCE standalone app shell
- source signals:
  - sine
  - white noise
  - impulse
  - WAV file
  - live hardware input
- project-based user DSP editing with `.dspedu` archives
- multi-file project tree in the editor
- compile, load, and safe hot reload of user DSP modules
- runtime controls:
  - knobs
  - buttons
  - toggles
- preferred audio configuration reported by the user DSP module
- requested vs actual device configuration reporting
- audio device selection, channel selection, and routing
- oscilloscope popup
- optional regression tests

## 3. New Project Baseline

New projects now start as a minimal compile-ready project:

- active file: `src/main.cpp`
- processor class: `MyAudioProcessor`
- no default controls

The default source is a clean pass-through skeleton, not a preloaded effect example.

Relevant files:

- `src/userdsp/UserDspProjectUtils.*`
- `src/userdsp/UserDspProjectManager.*`

## 4. Example Projects

Educational example projects now live in:

- `example_projects/`
- `example_project_archives/`

Included examples:

- `oscillator`
- `filter`
- `envelope`
- `lfo`
- `delay`
- `simple_synth_voice`

Each example is stored as an extracted `.dspedu` project:

- `project.json`
- `files/src/main.cpp`

Ready-to-open `.dspedu` archives are generated into `example_project_archives/` via:

- `scripts/package_example_projects.cmd`
- `scripts/package_example_projects.ps1`

Most examples demonstrate processing of an incoming signal. `simple_synth_voice` is the self-contained synthesis example.

## 5. Build Entry Points

### Main targets

- root build file: `CMakeLists.txt`
- app target: `DspEducationStand`
- optional tests target: `DspEducationStandTests`

### Windows

- app build helper: `scripts/build_windows.cmd`
- configure helper: `scripts/configure_windows.cmd`
- user DSP module build helper: `scripts/build_user_dsp.cmd`

Requirements:

- `JUCE_DIR` must point to a local JUCE checkout
- Ninja
- MSVC Build Tools

Typical app build:

```powershell
cmd /c scripts\build_windows.cmd build
```

Default helper configuration is `Release`.
Explicit override example:

```powershell
cmd /c scripts\build_windows.cmd build Debug
```

Typical tests build:

```powershell
cmake -S . -B build-tests -G Ninja -DJUCE_DIR=%JUCE_DIR% -DDSP_EDU_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-tests --target DspEducationStandTests
```

### macOS

- user DSP module build helper: `scripts/build_user_dsp.sh`

README documents the standard CMake flow for the app and tests.

## 6. Verified State

Re-verified locally on Windows on 2026-03-13:

- fresh app configure/build succeeded
- fresh tests configure/build succeeded
- `DspEducationStandTests.exe` ran successfully
- `scripts/build_user_dsp.cmd` successfully produced a `.dll` exporting `dspedu_get_api`

After the user-DSP-only cleanup in this task:

- a fresh Windows app build also succeeded
- the built-in DSP path no longer exists in the active target

## 7. Runtime Architecture

### Main UI

Primary UI composition:

- `src/app/MainComponent.h`
- `src/app/MainComponent.cpp`

This is the coordination layer for:

- transport
- source controls
- project actions
- compile actions
- device configuration
- popup tool windows

### Audio

Core runtime:

- `src/audio/AudioEngine.h`
- `src/audio/AudioEngine.cpp`
- `src/audio/AudioConfiguration.h`
- `src/audio/SignalSources.*`
- `src/audio/WavFileSource.*`

The engine now always routes audio through the user DSP host.

### User DSP

Main subsystem:

- `src/userdsp/UserDspHost.*`
- `src/userdsp/UserDspCompiler.*`
- `src/userdsp/UserDspProjectManager.*`
- `src/userdsp/UserDspProjectUtils.*`
- `src/userdsp/UserDspControllers.h`
- `user_dsp_sdk/UserDspApi.h`

Responsibilities:

- project layout and serialization
- controller metadata
- staging source files for compilation
- loading the compiled module
- exposing runtime controls to the module
- reading preferred audio config from the module
- maintaining safe swap/fallback behavior

### UI Tools

- `src/ui/ToolWindow.*`
- `src/ui/RealtimeControlsComponent.*`
- `src/ui/OscilloscopeComponent.*`
- `src/ui/OscilloscopeBuffer.*`
- `src/ui/DarkIdeLookAndFeel.*`

## 8. User DSP API Status

The app injects the SDK include, generated controls header, and entry wrapper automatically during staging.

Current API:

- `getPreferredAudioConfig(...)`
- `prepare(const DspEduProcessSpec&)`
- `reset()`
- `process(const float* const* inputs, float* const* outputs, int numInputChannels, int numOutputChannels, int numSamples)`

Runtime controls are exposed as generated fields under `controls.<name>`.

## 9. Important Files To Read First

- `CMakeLists.txt`
- `README.md`
- `TODO.md`
- `src/app/MainComponent.*`
- `src/audio/AudioEngine.*`
- `src/userdsp/UserDspHost.*`
- `src/userdsp/UserDspCompiler.*`
- `src/userdsp/UserDspProjectManager.*`
- `src/userdsp/UserDspProjectUtils.*`
- `src/ui/RealtimeControlsComponent.*`
- `example_projects/README.md`
- `example_project_archives/`
- `user_dsp_sdk/UserDspApi.h`

## 10. Files Removed In This Cleanup

These were removed from the repo because they represented the old built-in DSP path or stale starter flow:

- `src/dsp/BuiltinProcessors.*`
- `src/presets/PresetManager.*`
- `src/userdsp/UserCodeDocumentManager.*`
- `templates/UserDspTemplate.cpp`

## 11. Current Risks / Follow-Ups

- The Windows app build is verified after cleanup, but the educational examples still deserve individual compile smoke tests in a future pass.
- There is still a benign warning in `src/ui/ToolWindow.cpp` about `parentComponent` shadowing a JUCE member.

## 12. Short Continuation Summary

If another task picks this up cold, the right mental model is:

- the app is now user-DSP-only
- new projects should stay minimal and uncluttered
- educational example projects live in `example_projects/`
- future work is around onboarding, examples, archive/import UX, and higher-level control/input systems such as MIDI
