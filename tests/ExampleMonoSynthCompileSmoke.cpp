#include "UserDspApi.h"

#include <cmath>

namespace
{
struct DspEduProjectControls
{
    float tuneKnob = 0.5f;
    float cutoffKnob = 0.5f;
    float resonanceKnob = 0.2f;
    float releaseKnob = 0.5f;
    float envAmountKnob = 0.5f;
    float levelKnob = 0.5f;
};

DspEduProjectControls monoControls;
DspEduMidiState monoMidi;
const auto& controls = monoControls;
const auto& midi = monoMidi;
} // namespace

#include "../example_projects/simple_synth_voice/files/src/SimpleSynthVoiceProcessor.h"

[[maybe_unused]] static int exampleMonoSynthCompileSmoke()
{
    SimpleSynthVoiceProcessor processor;
    DspEduProcessSpec spec;
    spec.sampleRate = 48000.0;
    spec.maximumBlockSize = 32;
    spec.activeInputChannels = 0;
    spec.activeOutputChannels = 2;
    processor.prepare(spec);

    float left[32] {};
    float right[32] {};
    float* outputs[] { left, right };

    monoMidi.gate = 1;
    monoMidi.noteNumber = 60;
    monoMidi.velocity = 0.8f;
    monoMidi.pitchWheel = 0.0f;

    processor.process(nullptr, outputs, 0, 2, 32);
    return std::isfinite(outputs[0][0]) ? 0 : 1;
}
