# DSP Education Stand Manual

This manual is about the current workflow of the application, not about C++ in general and not about DSP theory in general.

It explains:

- how to structure code for this host
- how to work with controls
- how MIDI works
- how to use a MIDI keyboard
- how projects are saved

## 1. What This App Is

`DSP Education Stand` is a standalone desktop app for writing and testing user DSP modules.

The app lets you:

- create or open a `.dspedu` project
- edit C++ source files inside the app
- add runtime controls in the `Controls` window
- compile and hot-reload the DSP module
- choose audio and MIDI devices
- test effects and synths immediately

This is not a plugin packaging workflow.
You are writing the DSP processor itself.

## 2. Typical Workflow

The normal workflow is:

1. Create a new project or open an existing `.dspedu` archive.
2. Edit source files in the code editor.
3. Set the correct class name in `Processor Class`.
4. Open `Controls` and add the knobs, buttons, or toggles your code needs.
5. Press `Compile`.
6. Press `Start Audio`.
7. Choose a source signal or play from MIDI.

Useful rule:

- if you change DSP source code, compile again
- if you change control metadata or add/remove controls, compile again

## 3. What Code Shape The Host Expects

The host expects a processor class with methods like these:

```cpp
class MyAudioProcessor
{
public:
    const char* getName() const
    {
        return "My DSP";
    }

    bool getPreferredAudioConfig(DspEduPreferredAudioConfig& config) const
    {
        config.preferredInputChannels = 2;
        config.preferredOutputChannels = 2;
        return true;
    }

    void prepare(const DspEduProcessSpec& spec)
    {
        sampleRate = spec.sampleRate > 1.0 ? spec.sampleRate : 48000.0;
    }

    void reset()
    {
    }

    void process(const float* const* inputs,
                 float* const* outputs,
                 int numInputChannels,
                 int numOutputChannels,
                 int numSamples)
    {
    }

private:
    double sampleRate = 48000.0;
};
```

Important methods:

- `getPreferredAudioConfig(...)`
- `prepare(...)`
- `reset()`
- `process(...)`

## 4. Host-Specific Code Rules

These rules are specific to this app.

### Do not write host boilerplate manually

Do not manually add:

- `#include "UserDspApi.h"`
- `DSP_EDU_DEFINE_SIMPLE_PLUGIN(...)`

The app injects the SDK include, generated controls header, MIDI wrapper, and entry wrapper automatically during compile staging.

### `Processor Class` must match your code

The text field `Processor Class` must contain the exact class name that should be exported.

Example:

- source contains `class MyAudioProcessor`
- `Processor Class` must be `MyAudioProcessor`

If the class is inside a namespace, use the qualified name.

### Preferred config is not the same as actual runtime config

This is important:

- `config.preferredSampleRate = 48000.0;` is only a request
- the real values arrive in `prepare(const DspEduProcessSpec& spec)`

Always write DSP code against:

- `spec.sampleRate`
- `spec.maximumBlockSize`
- actual input/output channel counts passed to `process(...)`

### Realtime-safe coding matters

Inside `process(...)` avoid:

- file I/O
- locks or mutexes
- heap allocation
- expensive resizing of containers
- blocking calls

Prefer:

- member variables for DSP state
- preallocation in `prepare(...)`
- reset in `reset()`
- simple, predictable code

## 5. Recommended Audio Configurations

Use these patterns depending on the type of project.

### Effect / processor for incoming sound

Typical setup:

```cpp
config.preferredInputChannels = 1; // or 2
config.preferredOutputChannels = 2;
```

Good source choices in the app:

- `Hardware Input`
- `WAV`
- `White Noise`
- `Impulse`

### Generated synth without audio input

Typical setup:

```cpp
config.preferredInputChannels = 0;
config.preferredOutputChannels = 2;
```

In this case the DSP generates sound internally and ignores `inputs`.

## 6. Source Signals In The App

The app can feed the DSP from:

- `Sine`
- `White Noise`
- `Impulse`
- `WAV`
- `Hardware Input`

Use them intentionally:

- `Sine` for gain, clipping, modulation tests
- `White Noise` for filters
- `Impulse` for delays and reverbs
- `WAV` for general testing
- `Hardware Input` for live effects

For synths, you usually do not need any source signal at all.

## 7. Working With Controls

Open `Controls` to define runtime controls for the project.

Supported control types:

- `Knob`
- `Button`
- `Toggle`

### What control values look like in code

Generated controls appear in code as:

```cpp
controls.cutoffKnob
controls.triggerButton
controls.bypassToggle
```

Behavior:

- `Knob` gives a float in `0..1`
- `Button` behaves like a momentary boolean
- `Toggle` behaves like a latched boolean

### Current editing model

In the current UI, controller editing happens inline inside each control tile.

In edit mode you can change:

- display name
- DSP code name
- type
- MIDI assignment
- order
- deletion

Metadata changes save automatically inside the current in-memory project state.

Important:

- metadata edits are stored in the current project immediately
- but they are written to the `.dspedu` file only when you use `Save` or `Save As`
- but the running DSP module only becomes fully linked to the current control layout after `Compile`

### Preview vs linked runtime

There are two states:

- `Linked to DSP`
- `Preview only` or `Compile to relink`

Meaning:

- if the current control layout matches the compiled DSP module, controls are live
- if the layout changed after the last compile, the UI can still preview values, but you should compile again to relink the DSP

### Practical naming advice

Use clear code names such as:

- `cutoffKnob`
- `resonanceKnob`
- `triggerButton`
- `bypassToggle`
- `attackKnob`

Do not create your own local variable named `controls`, or you will shadow the generated host object.

## 8. MIDI Overview

There are two different MIDI workflows in this app.

This distinction is very important.

### A. MIDI used to control UI controls

This is `MIDI -> control value`.

Example:

- CC 21 moves `controls.cutoffKnob`
- Note Gate triggers `controls.triggerButton`
- Pitch Wheel moves a macro knob

This is configured inside the `Controls` window.

### B. MIDI used directly in DSP code

This is `MIDI keyboard -> note state -> synth code`.

Example:

- `midi.noteNumber` sets oscillator pitch
- `midi.gate` starts and releases an envelope
- `midi.velocity` sets note strength
- `midi.voices[]` drives a polyphonic synth

This does not require mapping MIDI to a UI control first.

## 9. MIDI Device Selection

The app has a global `MIDI Input` selector in the main window.

This is important:

- the selected MIDI input device is app-wide
- it is not stored per project
- the DSP code does not open MIDI devices itself

Normal workflow:

1. Choose `MIDI Input` in the main window.
2. Either map MIDI to controls, or read direct note state from `midi` in DSP code.

## 10. MIDI Bindings For Controls

Inside `Controls`, each control can have a MIDI assignment.

Available binding sources:

- `CC`
- `Note Gate`
- `Note Velocity`
- `Note Number`
- `Pitch Wheel`

You can assign MIDI in two ways:

- manually through the MIDI menu
- with `Learn`

You can remove it with `Clear`.

### Manual assignment

Inside a control tile in edit mode:

1. Click the `MIDI` field.
2. Choose the source type.
3. Choose the channel.
4. Choose the CC number or note number if needed.

### Learn

Inside a control tile in edit mode:

1. Press `Learn`.
2. Send a MIDI message from your controller.
3. The binding is captured and shown in the tile.

### Binding value rules

Current normalization rules:

- `CC` -> `0..127` becomes `0..1`
- `Note Gate` -> `1` on note-on, `0` on note-off
- `Note Velocity` -> note-on velocity becomes `0..1`, note-off resets to `0`
- `Note Number` -> note number becomes `note / 127.0`, note-off resets to `0`
- `Pitch Wheel` binding -> `0..1`

Important nuance:

- `Pitch Wheel` as a control binding uses `0..1`
- direct `midi.pitchWheel` in DSP code uses `-1..1`

## 11. Writing Code For A MIDI Keyboard

For actual synth programming, read the generated `midi` object directly.

### Mono keyboard state

Useful fields:

```cpp
midi.gate
midi.noteNumber
midi.velocity
midi.pitchWheel
```

Typical mono synth logic:

- if `midi.gate` becomes active, start the note
- use `midi.noteNumber` for oscillator pitch
- use `midi.velocity` for level or timbre
- use `midi.pitchWheel` for pitch bend
- if `midi.gate` goes low, release the envelope

Important current behavior:

- the mono convenience state uses `last note wins`
- if the currently focused mono note is released, gate becomes `0`
- it does not automatically fall back to an older still-held note

So for advanced keyboard behavior, use `midi.voices[]`.

### Example of mono usage

```cpp
const bool gateIsHigh = midi.gate != 0 && midi.noteNumber >= 0;

if (gateIsHigh)
{
    const float note = static_cast<float>(midi.noteNumber);
    const float bendSemitones = midi.pitchWheel * 2.0f;
    const float playedNote = note + bendSemitones;
}
```

## 12. Polyphony And `midi.voices[]`

For polyphonic synths, use:

```cpp
midi.voiceCount
midi.voices[index]
midi.channelPitchWheel[channelIndex]
```

Each voice slot contains:

- `active`
- `channel`
- `noteNumber`
- `velocity`
- `order`

### How to use it

A good pattern is:

1. Keep one synth voice object per host MIDI voice slot.
2. On every block, inspect `midi.voices[]`.
3. If a slot becomes active with a new `order`, start a synth voice.
4. If a slot becomes inactive, release that synth voice.
5. Let the release tail continue inside your synth voice even after the MIDI slot went inactive.

This is exactly the right approach for ADSR-based poly synths in this app.

### Why `order` matters

The host can reuse voice slots.

That means:

- slot 3 can be one note now
- later slot 3 can represent a different note

`order` lets you detect that the slot now represents a new note event.

## 13. MIDI Pitch Wheel In Direct DSP Code

Direct keyboard pitch wheel is available as:

- `midi.pitchWheel` in `-1..1`
- `midi.channelPitchWheel[channel - 1]` in `-1..1`

Typical use:

- mono synth: use `midi.pitchWheel`
- poly synth: use `midi.channelPitchWheel[...]` per voice channel

Example:

```cpp
const float bendRangeInSemitones = 2.0f;
const float bentNote = static_cast<float>(midi.noteNumber) + midi.pitchWheel * bendRangeInSemitones;
```

## 14. How Projects Are Saved

Projects are saved as `.dspedu` archives.

A `.dspedu` file is effectively a zip archive containing:

- `project.json`
- `files/...`

### What the project archive stores

It stores:

- project name
- processor class name
- active file path
- project tree
- source and header file contents
- control metadata
- MIDI bindings of controls
- project audio state and routing metadata

### What is not stored in the project

The selected global MIDI input device is not stored in the `.dspedu` project.

That MIDI device selection is stored separately as an app-wide setting.

## 15. Save, Save As, Open, New

### Save

- saves back to the current `.dspedu` file
- if the project does not yet have a file path, the app will require `Save As`

### Save As

- lets you choose the target `.dspedu` file
- if you omit the extension, the app adds `.dspedu`

### Open Project

- opens a `.dspedu` archive
- restores files, controls, project metadata, and project audio state

### New Project

- creates a fresh minimal project
- starts from a compile-ready default template

If there are unsaved changes, the app will ask whether to save, discard, or cancel.

## 16. Recommended Working Habits

For smooth work in this app:

- keep helper DSP classes in separate `.h` files when the project grows
- keep `main.cpp` small if the project becomes more complex
- use `Processor Class` carefully
- compile after control layout changes
- choose the test source intentionally
- use control bindings for macros and UI interaction
- use direct `midi` state for keyboard synth logic

## 17. Common Mistakes

### Host integration mistakes

- manually including `UserDspApi.h`
- manually exporting the plugin entry
- forgetting to update `Processor Class`
- changing control layout and forgetting to compile again

### Audio mistakes

- assuming stereo everywhere
- ignoring null input/output pointers
- using preferred sample rate instead of actual runtime sample rate
- doing too much work inside `process(...)`

### MIDI mistakes

- confusing control MIDI bindings with direct keyboard MIDI state
- expecting a control-bound pitch wheel to be `-1..1`
- expecting mono `midi.gate` to restore the previous held note automatically
- trying to use a MIDI keyboard without choosing a `MIDI Input` device first

## 18. Good Starting Templates

Use these mental templates:

- pass-through effect: `1-2 input`, `2 output`
- filter effect: input-driven, with knobs for cutoff/resonance
- mono synth: `0 input`, `2 output`, read `midi.gate` and `midi.noteNumber`
- poly synth: `0 input`, `2 output`, read `midi.voices[]`

If you follow those patterns, the app will feel predictable and easy to work with.
