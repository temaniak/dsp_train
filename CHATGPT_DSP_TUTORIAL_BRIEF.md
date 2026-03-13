# DSP Tutorial Brief For ChatGPT

Use this document as context before writing a DSP tutorial for me.

## Goal

I want a practical DSP tutorial that teaches me how to write real algorithms inside my current coding environment.

The tutorial should be:

- beginner-friendly, but technically correct
- focused on intuition first, then implementation
- built around small working projects
- oriented toward hearing results quickly
- aware of the exact host/API I am using

Do not write a generic DSP textbook.
Write a tutorial for this specific environment.

## What Environment We Are Using

We are writing C++ user DSP code inside a desktop app called `DSP Education Stand`.

This app is:

- a JUCE standalone desktop app
- a user-DSP-only playground
- based on project files (`.dspedu`)
- able to compile and hot-reload DSP modules
- able to expose runtime controls to DSP code

The workflow is:

1. Create or open a DSP project.
2. Edit C++ source files inside the app.
3. Add controls in the `Controls` window.
4. Compile.
5. The app builds a module and hot-reloads it.

This is not a plugin SDK tutorial.
This is not about VST/AU packaging.
It is about algorithm development inside this host.

## Core DSP Contract

The app injects the SDK include and entry wrapper automatically.

I should NOT manually write:

- `#include "UserDspApi.h"`
- `DSP_EDU_DEFINE_SIMPLE_PLUGIN(...)`

The code is expected to follow this style:

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
        sampleRate = spec.sampleRate;
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
    double sampleRate = 44100.0;
};
```

Important functions:

- `getPreferredAudioConfig(...)`
- `prepare(...)`
- `reset()`
- `process(...)`

## Audio Model

The host can provide:

- sine
- white noise
- impulse
- WAV file
- live hardware input

This matters for tutorial design:

- some examples should generate sound on their own
- some examples should process incoming sound
- the tutorial should explicitly say which source to use in the app for each lesson

Examples:

- oscillator lesson: can work without input
- filter lesson: should suggest noise, WAV, or hardware input
- delay lesson: should suggest impulse, WAV, or hardware input
- envelope lesson: should explain clearly whether it shapes generated sound or external input

## Runtime Controls

The app has a `Controls` window where runtime controls are created.

Supported control types:

- `Knob`
- `Button`
- `Toggle`

These controls are exposed in code as generated fields:

```cpp
controls.cutoffKnob
controls.triggerButton
controls.bypassToggle
```

Important:

- knobs output values in `0..1`
- buttons are momentary booleans
- toggles are latched booleans
- the code name must match exactly
- after changing the controller layout, the project must be compiled again

Important pitfall:

Do NOT declare my own local `controls` struct inside the DSP class.
If I do that, I shadow the host-generated controls and the UI knobs stop affecting the algorithm.

## Practical Constraints

The tutorial should teach DSP with awareness of realtime audio constraints.

Assume:

- code inside `process(...)` runs on the audio thread
- avoid file I/O in the audio callback
- avoid locks/mutexes in the audio callback
- avoid unnecessary allocations in `process(...)`
- prefer allocating buffers in `prepare(...)`
- prefer stable simple code over over-engineered abstractions

The tutorial should explain these constraints in simple language.

## Channel Model

This environment is practically mono/stereo focused.

Useful assumptions:

- usually `0`, `1`, or `2` input channels
- usually `1` or `2` output channels
- most tutorial code can be mono-in/stereo-out or stereo-in/stereo-out

When reading inputs, the tutorial should use safe patterns like:

```cpp
float getInputSample(const float* const* inputs, int numInputChannels, int channel, int sample) const
{
    if (inputs == nullptr || numInputChannels <= 0)
        return 0.0f;

    if (channel < numInputChannels && inputs[channel] != nullptr)
        return inputs[channel][sample];

    return inputs[0] != nullptr ? inputs[0][sample] : 0.0f;
}
```

## What The Tutorial Should Cover

I want the tutorial to be organized as a sequence of practical lessons.

Preferred order:

1. What digital audio is: samples, sample rate, amplitude
2. The structure of a DSP processor in this host
3. Pass-through and gain
4. Oscillators
5. One-pole filters and basic tone shaping
6. Envelopes
7. LFO modulation
8. Delay
9. Reverb basics
10. Building a simple synth voice
11. Combining modules into larger algorithms
12. Common DSP mistakes and debugging habits

## Teaching Style I Want

Please teach as if I am learning by building and listening.

For each lesson:

- explain the concept simply
- show a complete working example for this environment
- explain what each part of the code does
- explain what controls to create in the `Controls` window
- explain what source signal to choose in the app
- explain what I should hear
- suggest 2 or 3 small experiments to modify the code

The tutorial should favor:

- small complete examples
- audible intuition
- progressive complexity
- repeated reuse of earlier ideas

Avoid:

- long abstract math-first exposition
- giant code dumps without explanation
- framework-agnostic examples that ignore this host

## Coding Preferences

Code should be:

- clear and readable
- split into small helper functions when useful
- not overly clever
- based on `float` for audio samples unless there is a reason otherwise
- cautious about `double -> float` conversions
- explicit about parameter mapping from `0..1` knob ranges into meaningful units

Good tutorial habits:

- use names like `cutoffKnob`, `decayKnob`, `mixKnob`, `triggerButton`
- explain exponential mapping for frequency/time controls
- explain dry/wet mixing carefully
- explain clipping/headroom when relevant
- explain state variables clearly

## Existing Educational Direction

This environment already contains example projects in the repository for:

- oscillator
- filter
- envelope
- lfo
- delay
- simple_synth_voice

You may use that lesson structure as inspiration, but build the tutorial cleanly from fundamentals.

## What I Need From You

Please create a structured DSP tutorial for this environment.

The result should include:

- a course outline
- lesson-by-lesson progression
- concrete code examples written for this exact DSP host style
- clear instructions for controls and expected audible results
- small exercises after each lesson

If useful, you may also include:

- a glossary
- troubleshooting notes
- "listen for this" sections
- short summaries at the end of each lesson

Most importantly:

Make the tutorial help me become capable of writing my own DSP algorithms in this exact sandbox, not just understanding theory in the abstract.
