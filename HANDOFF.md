# HANDOFF

This file is a developer handoff for the next task. It is intentionally focused on current implementation state, real entry points, and likely next steps.

## 1. Project Summary

`DSP Education Stand` is a JUCE standalone desktop application used as a DSP learning playground.

The project has moved beyond the original mono MVP. The current app is a project-based user DSP environment with:

- built-in signal sources
- built-in DSP processors
- user DSP projects stored as `.dspedu` archives
- real dynamic module compilation and hot reload
- custom runtime controls for user DSP
- audio device and channel configuration
- preferred audio configuration reported by the DSP module
- oscilloscope popup window
- dark IDE-like UI

The repository currently targets local developer builds on Windows and macOS.
The Windows path was re-verified locally on 2026-03-13.

## 2. What Is Working

At a high level, the following features are already present in the codebase:

- JUCE standalone application shell
- audio engine with device selection and runtime reconfiguration
- signal sources:
  - sine
  - white noise
  - impulse
  - WAV file
  - live hardware input
- built-in DSP:
  - bypass
  - gain
  - hard clip
  - one-pole lowpass
  - simple delay
- project-based user DSP editing and compilation
- safe hot reload with fallback to previous valid module
- custom realtime controls:
  - knobs
  - buttons
  - toggles
- preferred audio config flow from user DSP to host
- requested vs actual audio configuration reporting
- stereo oscilloscope tool window
- minimal optional tests

## 3. Current Product Direction

The app is no longer just a simple "type code in one editor and compile a DLL" demo.

The current direction is:

- user DSP projects as the primary workflow
- runtime controls exposed to user DSP code
- device/channel/routing visibility in the UI
- cross-platform local developer workflow

There is still legacy code from the earlier simpler iteration in the repository, but the active direction is the project-based user DSP workflow.

## 4. Build Entry Points

### Main CMake target

- Root build file: `CMakeLists.txt`
- Main app target: `DspEducationStand`
- Optional tests target: `DspEducationStandTests`

### Windows

- Configure/build helper: `scripts/build_windows.cmd`
- Configure helper: `scripts/configure_windows.cmd`
- User DSP module build helper: `scripts/build_user_dsp.cmd`

Requirements:

- `JUCE_DIR` must point to a local JUCE checkout
- Ninja
- MSVC Build Tools

Typical build:

```powershell
cmd /c scripts\build_windows.cmd build
```

Verified on 2026-03-13:

- fresh Windows configure/build succeeded with MSVC + Ninja
- fresh optional tests configure/build succeeded
- `DspEducationStandTests.exe` ran successfully
- `scripts/build_user_dsp.cmd` successfully produced a `.dll` exporting `dspedu_get_api`

Current helper behavior:

- `scripts/configure_windows.cmd` configures plain `Ninja` and explicitly passes `-DCMAKE_BUILD_TYPE`
- default helper configuration is `Release`
- optional override is supported as the second argument, for example `cmd /c scripts\build_windows.cmd build Debug`

### macOS

- User DSP module build helper: `scripts/build_user_dsp.sh`

README already documents the expected CMake flow for macOS.

## 5. Runtime Architecture

### Main UI

Primary UI composition lives in:

- `src/app/MainComponent.h`
- `src/app/MainComponent.cpp`

This is the central coordination layer for:

- transport
- source controls
- built-in DSP controls
- project actions
- compile actions
- device configuration
- popup tool windows

### Audio

Core audio runtime:

- `src/audio/AudioEngine.h`
- `src/audio/AudioEngine.cpp`
- `src/audio/AudioConfiguration.h`
- `src/audio/SignalSources.*`
- `src/audio/WavFileSource.*`

The engine handles:

- source generation
- input/output device state
- channel configuration
- routing
- built-in DSP path
- user DSP path

### User DSP

Main subsystem files:

- `src/userdsp/UserDspHost.*`
- `src/userdsp/UserDspCompiler.*`
- `src/userdsp/UserDspProjectManager.*`
- `src/userdsp/UserDspProjectUtils.*`
- `src/userdsp/UserDspControllers.h`
- `user_dsp_sdk/UserDspApi.h`

This subsystem is responsible for:

- project layout and serialization
- staging source files for compilation
- loading the compiled module
- exposing runtime controls to the module
- reading preferred audio config from the module
- maintaining safe swap/fallback behavior

### UI Tools

Related files:

- `src/ui/ToolWindow.*`
- `src/ui/RealtimeControlsComponent.*`
- `src/ui/OscilloscopeComponent.*`
- `src/ui/OscilloscopeBuffer.*`
- `src/ui/DarkIdeLookAndFeel.*`

These provide the dark UI, popup windows, controls window, and oscilloscope window.

## 6. User DSP API Status

Important: the current user DSP workflow is not the old "single text file with manual SDK include and plugin macro" model.

The current implementation stages a project and injects the SDK/include/entry wrapper automatically.

The current API is based around:

- `getPreferredAudioConfig(...)`
- `prepare(const DspEduProcessSpec&)`
- `reset()`
- `process(const float* const* inputs, float* const* outputs, int numInputChannels, int numOutputChannels, int numSamples)`

The controls system exposes generated fields under `controls.<name>`.

Examples from the current direction:

- `controls.driveKnob`
- `controls.fireButton`
- `controls.bypassToggle`

Do not assume the earlier simpler macro-based workflow is still the main path. There are remnants from the earlier iteration in the repository, but the project has already evolved beyond that.

## 7. Important Current Files

Use these first when continuing work:

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
- `user_dsp_sdk/UserDspApi.h`

## 8. Likely Legacy / Stale Files

These files still exist in the tree but are not part of the current main target in `CMakeLists.txt`:

- `src/userdsp/UserCodeDocumentManager.h`
- `src/userdsp/UserCodeDocumentManager.cpp`

They appear to belong to an earlier simpler editor flow. Treat them carefully before using them as a source of truth.

If future cleanup happens, verify whether they should be removed or merged into the project-based workflow.

Also note:

- older existing build directories may still contain stale references to these legacy files
- a clean reconfigure follows the current `CMakeLists.txt` source list

## 9. Documentation Notes

- `README.md` is currently the main user-facing project description.
- In raw Windows terminal output, the Russian section may look garbled because of code page issues. That is a terminal rendering issue, not necessarily a content issue in editors/GitHub.
- The Windows helper scripts now default to a `Release` build with plain Ninja and accept an optional explicit build type as the second argument.

## 10. Open Technical Directions

The current `TODO.md` points at these next directions:

1. Remove or de-emphasize built-in DSP so that user DSP projects become the primary focus.
2. Add MIDI input, MIDI learn, and MIDI-to-control mapping.
3. Rework the left-side menu and reduce UI noise.
4. Improve the new-project onboarding flow.
5. Add more educational example projects.

These are good candidates for the next task.

## 11. Suggested First Checks For The Next Task

Before making changes in the next task:

1. Start from `CMakeLists.txt` to confirm which files are actually compiled.
2. Read `MainComponent`, `AudioEngine`, and `UserDspProjectManager` before changing workflow.
3. Treat `README.md` as product-level intent and `TODO.md` as active direction.
4. Verify whether a file is actively wired into the build before refactoring it.

## 12. Local Repository State

At the time of writing:

- this workspace is a git repository
- the project is oriented around local developer builds
- a clean Windows app build, clean Windows tests build, and Windows user DSP module build path were all re-verified on 2026-03-13
- there is no assumption here of packaged, signed, or notarized distribution

## 13. Short Continuation Summary

If another task picks this up cold, the correct mental model is:

- this is a JUCE-based DSP playground
- the app already supports project-based user DSP compilation
- runtime controls and preferred audio config are already part of the design
- the main continuation work is product refinement, workflow cleanup, and additional input/control systems such as MIDI
