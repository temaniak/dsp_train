# DSP Education Stand

JUCE standalone application for learning DSP through editable user projects.

The app is now focused on one workflow only:
- choose a source signal
- edit a C++ user DSP project
- define realtime controls in `Controls`
- use MIDI mappings or a MIDI keyboard when needed
- compile and hot-reload the module

Built-in DSP processors and preset management were removed. The app is now a user-DSP-only playground.

## What Works Now

- project-based user DSP development with `.dspedu` archives
- project tree with multiple source/header files
- safe compile, load, and hot reload of user DSP modules
- inline-edited runtime controls UI
- runtime controls exposed as `controls.<codeName>`
- source signals:
  - sine
  - white noise
  - impulse
  - WAV file
  - live hardware input
- audio device selection and routing
- global MIDI input selection
- MIDI bindings for controls:
  - CC
  - Note Gate
  - Note Velocity
  - Note Number
  - Pitch Wheel
- direct MIDI keyboard state for DSP code:
  - `midi.gate`
  - `midi.noteNumber`
  - `midi.velocity`
  - `midi.pitchWheel`
  - `midi.voices[]`
  - `midi.channelPitchWheel[]`
- preferred sample rate / block size / channel count reported by the module
- stereo oscilloscope popup
- dark IDE-style UI
- ready-to-open example project archives
- English and Russian manuals

## Main Workflow

1. Create a new project or open an existing `.dspedu` archive.
2. Edit files in the code editor.
3. Set the exported processor class in `Processor Class`.
4. Open `Controls` and add knobs, buttons, or toggles.
5. Compile the project.
6. Choose audio and MIDI devices if needed.
7. Start audio and test the result.

During compile, the app stages the project, injects the SDK wrapper automatically, builds a native module, and hot-swaps it safely.

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
- controller metadata is edited inline in the control tiles
- metadata edits update the in-memory project immediately
- compile again after changing controller layout or metadata that must relink to DSP

Generated names are available in code as:

```cpp
controls.cutoffKnob
controls.triggerButton
controls.bypassToggle
```

If the current control layout no longer matches the compiled module, the UI can still show preview values, but you should compile again before expecting the running DSP module to use that exact layout.

## MIDI

There are two different MIDI workflows in the app.

### MIDI -> UI controls

Each user control can be bound to MIDI in the `Controls` window.

Supported binding sources:

- `CC`
- `Note Gate`
- `Note Velocity`
- `Note Number`
- `Pitch Wheel`

Bindings can be assigned manually or with `Learn`.

### MIDI keyboard -> DSP code

For synths and direct note handling, DSP code can read the generated `midi` object directly.

Useful fields include:

- `midi.gate`
- `midi.noteNumber`
- `midi.velocity`
- `midi.pitchWheel`
- `midi.voiceCount`
- `midi.voices[]`
- `midi.channelPitchWheel[]`

Typical usage:

- mono synths use `midi.gate`, `midi.noteNumber`, and `midi.pitchWheel`
- poly synths use `midi.voices[]` and `midi.channelPitchWheel[]`

Important nuance:

- direct `midi.pitchWheel` in DSP code uses `-1..1`
- `Pitch Wheel` mapped to a control is normalized to `0..1`

## Example Projects

The repository now includes educational examples in [example_projects](./example_projects):

- `oscillator`
- `filter`
- `envelope`
- `lfo`
- `delay`
- `simple_synth_voice`
- `poly_synth_adsr`

The readable source versions live in `example_projects/`.
Ready-to-open archives live in `example_project_archives/`.

Each source example is stored as an extracted `.dspedu` project:

- `project.json` contains controller metadata and project structure
- `files/src/main.cpp` contains the processor source with detailed comments

The two synth examples are intentionally more structured than the single-file effect examples:

- `simple_synth_voice`: monophonic MIDI keyboard synth split into helper headers
- `poly_synth_adsr`: polyphonic saw synth with ADSR, filter, and per-voice classes

If you update the extracted examples, rebuild the archives with:

```powershell
cmd /c scripts\package_example_projects.cmd
```

Most examples are designed to process an incoming signal. They work especially well with:

- `Hardware Input`
- `WAV`
- `Impulse`
- `Noise`

## Manuals

User-facing manuals are available in the repository root:

- [manual_en.md](./manual_en.md)
- [manual_ru.md](./manual_ru.md)

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

### macOS Release Build

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DJUCE_DIR=/path/to/JUCE
cmake --build build-release --target DspEducationStand -j
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

macOS release build:

```bash
open "build-release/DspEducationStand_artefacts/Release/DSP Education Stand.app"
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
- The selected MIDI input device is app-wide and is not stored inside `.dspedu` projects.
- User DSP compilation depends on the local platform toolchain:
  - MSVC on Windows
  - Apple clang on macOS
- Linux is not currently configured as a documented build target in this repository.
