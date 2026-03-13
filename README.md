# DSP Education Stand

JUCE standalone application for learning DSP through editable user projects.

The app is now focused on one workflow only:
- choose a source signal
- edit a C++ user DSP project
- define realtime controls in `Controls`
- compile and hot-reload the module

Built-in DSP processors and preset management were removed. The app is now a user-DSP-only playground.

## What Works Now

- project-based user DSP development with `.dspedu` archives
- project tree with multiple source/header files
- safe compile, load, and hot reload of user DSP modules
- runtime controls exposed as `controls.<codeName>`
- source signals:
  - sine
  - white noise
  - impulse
  - WAV file
  - live hardware input
- audio device selection, channel enablement, and routing
- preferred sample rate / block size / channel count reported by the module
- stereo oscilloscope popup
- dark IDE-style UI

## Main Workflow

1. Create a new project or open an existing `.dspedu` archive.
2. Edit files in the code editor.
3. Set the exported processor class in `Processor Class`.
4. Open `Controls` and add knobs, buttons, or toggles.
5. Compile the project.
6. The app stages the project, injects the SDK wrapper automatically, builds a native module, and hot-swaps it safely.

A new project now starts as a minimal compile-ready file:

- `src/main.cpp`
- processor class `MyAudioProcessor`
- no pre-created controls

## User DSP API

Do not manually add:

- `#include "UserDspApi.h"`
- `DSP_EDU_DEFINE_SIMPLE_PLUGIN(...)`

The app injects the SDK include, generated controls header, and entry wrapper during staging.

Current contract:

- `getPreferredAudioConfig(...)`
- `prepare(const DspEduProcessSpec&)`
- `reset()`
- `process(const float* const* inputs, float* const* outputs, int numInputChannels, int numOutputChannels, int numSamples)`

Minimal example:

```cpp
class MyAudioProcessor
{
public:
    const char* getName() const
    {
        return "New User DSP Project";
    }

    bool getPreferredAudioConfig(DspEduPreferredAudioConfig& config) const
    {
        config.preferredInputChannels = 2;
        config.preferredOutputChannels = 2;
        return true;
    }

    void prepare(const DspEduProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
    }

    void process(const float* const* inputs,
                 float* const* outputs,
                 int numInputChannels,
                 int numOutputChannels,
                 int numSamples)
    {
        for (int channel = 0; channel < numOutputChannels; ++channel)
        {
            auto* output = outputs[channel];

            if (output == nullptr)
                continue;

            const auto* input = (inputs != nullptr && channel < numInputChannels) ? inputs[channel] : nullptr;

            for (int sample = 0; sample < numSamples; ++sample)
                output[sample] = input != nullptr ? input[sample] : 0.0f;
        }
    }

private:
    double sampleRate = 44100.0;
};
```

## Realtime Controls

The `Controls` window lets you define custom runtime controls for a project.

Supported types:

- `Knob`
- `Button`
- `Toggle`

Behavior:

- knobs output `0..1`
- buttons are momentary booleans
- toggles are latched booleans

Generated names are available in code as:

```cpp
controls.cutoffKnob
controls.triggerButton
controls.bypassToggle
```

## Example Projects

The repository now includes educational examples in [example_projects](./example_projects):

- `oscillator`
- `filter`
- `envelope`
- `lfo`
- `delay`
- `simple_synth_voice`

The readable source versions live in `example_projects/`.
Ready-to-open archives live in `example_project_archives/`.

Each source example is stored as an extracted `.dspedu` project:

- `project.json` contains controller metadata and project structure
- `files/src/main.cpp` contains the processor source with detailed comments

If you update the extracted examples, rebuild the archives with:

```powershell
cmd /c scripts\package_example_projects.cmd
```

Most examples are designed to process an incoming signal. They work especially well with:

- `Hardware Input`
- `WAV`
- `Impulse`
- `Noise`

## Audio Model

The app tracks three layers of audio configuration:

- preferred values reported by the user DSP module
- requested values after merging project state and manual overrides
- actual values accepted by the current device/backend

This applies to:

- sample rate
- block size
- input channels
- output channels
- routing

If a preferred configuration cannot be applied exactly, the app falls back safely and reports the mismatch in the UI.

## Build

Requirements:

- CMake 3.22+
- local JUCE checkout available through `JUCE_DIR`

Windows:

- Ninja
- Visual Studio Build Tools with C++

macOS:

- Xcode or Command Line Tools

### Windows App Build

```powershell
cmd /c scripts\build_windows.cmd build
```

The helper defaults to `Release`.

Debug build:

```powershell
cmd /c scripts\build_windows.cmd build Debug
```

### macOS App Build

```bash
cmake -S . -B build -DJUCE_DIR=/path/to/JUCE
cmake --build build --target DspEducationStand -j
```

### Optional Tests

```bash
cmake -S . -B build-tests -DDSP_EDU_BUILD_TESTS=ON -DJUCE_DIR=/path/to/JUCE
cmake --build build-tests --target DspEducationStandTests -j
```

Windows test run example:

```powershell
& ".\build-tests\DspEducationStandTests_artefacts\Release\DspEducationStandTests.exe"
```

## Run

macOS:

```bash
open "build/DspEducationStand_artefacts/DSP Education Stand.app"
```

Windows:

```powershell
& ".\build\DspEducationStand_artefacts\Release\DSP Education Stand.exe"
```

The exact artefact path depends on build directory and configuration.

## Repository Layout

```text
src/
  app/         Application shell and main UI
  audio/       Audio engine, routing, sources, WAV source
  ui/          Look-and-feel, oscilloscope, tool windows, controls UI
  userdsp/     Project manager, compiler, module host, helpers
  util/        Small utility helpers
scripts/       Windows build helpers and user DSP module build scripts
example_projects/
  ...          Educational extracted `.dspedu` examples
example_project_archives/
  ...          Ready-to-open `.dspedu` archives
user_dsp_sdk/  Stable SDK header for user DSP modules
tests/         Optional regression tests
```

## Notes

- The practical audio API is still limited to `DSP_EDU_USER_DSP_MAX_AUDIO_CHANNELS = 2`.
- Routing is a practical channel mapping system, not a full mixer matrix.
- The project targets local developer builds, not packaged or notarized distribution.
- User DSP compilation depends on the local platform toolchain:
  - MSVC on Windows
  - Apple clang on macOS
