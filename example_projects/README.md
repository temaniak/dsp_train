# User DSP Example Projects

These folders are stored as extracted `.dspedu` projects.

Each example contains:
- `project.json`: controller metadata and project structure
- `files/src/main.cpp`: the processor source with detailed comments

Most examples are designed to teach processing of an incoming signal.
Use `Hardware Input`, `WAV`, `Impulse`, or `Noise` as the source when you want to hear the effect clearly.

Projects included here:
- `oscillator`: a basic oscillator mixed with the incoming signal
- `filter`: a simple resonant low-pass filter for incoming audio
- `envelope`: a one-shot decay envelope that gates the incoming signal
- `lfo`: an LFO that sweeps a low-pass filter over the incoming signal
- `delay`: a stereo delay that makes the role of the incoming source obvious
- `simple_synth_voice`: a monophonic MIDI keyboard synth split into small header-based classes
- `poly_synth_adsr`: a polyphonic saw synth with ADSR envelope, resonant low-pass filter, and per-voice classes

The synth examples are intentionally structured differently from the effect examples:
- `main.cpp` is tiny and only includes the processor header
- the processor pulls in separate `.h` files for oscillator, envelope, filter, and helper math
- comments are intentionally more verbose so the project can be read like teaching material

To turn one of these folders into a `.dspedu` archive later, zip the contents of the example folder itself so that the archive root contains `project.json` and `files/`.
