#include "UserDspApi.h"

#include <cmath>

namespace
{
struct DspEduProjectControls
{
    float attackKnob = 0.2f;
    float decayKnob = 0.3f;
    float sustainKnob = 0.7f;
    float releaseKnob = 0.4f;
    float cutoffKnob = 0.6f;
    float resonanceKnob = 0.2f;
    float envAmountKnob = 0.5f;
    float levelKnob = 0.6f;
};

DspEduProjectControls polyControls;
DspEduMidiState polyMidi;
const auto& controls = polyControls;
const auto& midi = polyMidi;
} // namespace

#include "../example_projects/poly_synth_adsr/files/src/PolySynthAdsrProcessor.h"

[[maybe_unused]] static int examplePolySynthCompileSmoke()
{
    PolySynthAdsrProcessor processor;
    DspEduProcessSpec spec;
    spec.sampleRate = 48000.0;
    spec.maximumBlockSize = 32;
    spec.activeInputChannels = 0;
    spec.activeOutputChannels = 2;
    processor.prepare(spec);

    float left[32] {};
    float right[32] {};
    float* outputs[] { left, right };

    polyMidi.voices[0].active = 1;
    polyMidi.voices[0].channel = 1;
    polyMidi.voices[0].noteNumber = 60;
    polyMidi.voices[0].velocity = 0.9f;
    polyMidi.voices[0].order = 1;
    polyMidi.channelPitchWheel[0] = 0.0f;

    processor.process(nullptr, outputs, 0, 2, 32);
    return std::isfinite(outputs[0][0]) ? 0 : 1;
}
