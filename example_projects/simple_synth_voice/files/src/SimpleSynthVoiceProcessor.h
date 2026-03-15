#pragma once

#include "MidiNoteUtils.h"
#include "SimpleGateEnvelope.h"
#include "SimpleLadderFilter.h"
#include "SimpleSawOscillator.h"

class SimpleSynthVoiceProcessor
{
public:
    const char* getName() const
    {
        return "Simple MIDI Mono Synth";
    }

    bool getPreferredAudioConfig(DspEduPreferredAudioConfig& config) const
    {
        config.preferredSampleRate = 48000.0;
        config.preferredBlockSize = 256;
        config.preferredInputChannels = 0;
        config.preferredOutputChannels = 2;
        return true;
    }

    void prepare(const DspEduProcessSpec& spec)
    {
        sampleRate = spec.sampleRate > 1.0 ? spec.sampleRate : 44100.0;
        oscillator.reset();
        filter.prepare(sampleRate);
        envelope.prepare(sampleRate);
        envelope.setAttackTime(0.005f);
        reset();
    }

    void reset()
    {
        oscillator.reset();
        filter.reset();
        envelope.reset();
        previousGate = false;
        lastPlayedMidiNote = 60.0f;
        heldVelocity = 0.0f;
    }

    void process(const float* const*,
                 float* const* outputs,
                 int,
                 int numOutputChannels,
                 int numSamples)
    {
        /*
            Controller layout for this example:

            - Tune:           coarse transposition around the played MIDI note
            - Filter Cutoff:  static low-pass cutoff
            - Resonance:      feedback amount in the ladder filter
            - Release:        how long the note rings after key release
            - Env To Cutoff:  how strongly the envelope opens the filter
            - Level:          final output level

            The key educational message is:

            MIDI keyboard -> note / velocity / pitch wheel
                        -> oscillator frequency
                        -> envelope
                        -> filter
                        -> stereo output
        */
        const float tuneSemitones = monomidi::mapSignedSemitoneOffset(controls.tuneKnob, 24.0f);
        const float pitchBendSemitones = monomidi::pitchWheelToSemitones(midi.pitchWheel, 2.0f);
        const float baseCutoff = monomidi::mapExponential(controls.cutoffKnob, 50.0f, 5000.0f);
        const float resonance = controls.resonanceKnob * 3.8f;
        const float releaseSeconds = monomidi::mapExponential(controls.releaseKnob, 0.03f, 2.5f);
        const float envelopeToCutoffHz = controls.envAmountKnob * 7000.0f;
        const float outputLevel = controls.levelKnob * 0.25f;

        envelope.setReleaseTime(releaseSeconds);
        updateKeyboardState(tuneSemitones, pitchBendSemitones);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float envelopeValue = envelope.process();
            const float oscillatorSample = oscillator.process(monomidi::midiNoteToFrequency(lastPlayedMidiNote),
                                                             static_cast<float>(sampleRate));
            const float modulatedCutoff = baseCutoff + envelopeValue * envelopeToCutoffHz;
            const float filteredSample = filter.process(oscillatorSample, modulatedCutoff, resonance);
            const float voiceSample = filteredSample * envelopeValue * heldVelocity * outputLevel;

            for (int channel = 0; channel < numOutputChannels; ++channel)
            {
                if (outputs[channel] != nullptr)
                    outputs[channel][sample] = voiceSample;
            }
        }
    }

private:
    void updateKeyboardState(float tuneSemitones, float pitchBendSemitones)
    {
        /*
            The application exposes a convenience mono state:

            midi.gate        -> whether the currently focused mono note is on
            midi.noteNumber  -> note number of that mono note
            midi.velocity    -> velocity of that mono note
            midi.pitchWheel  -> bend value in -1..1

            Because the host uses "last note wins", this mono example simply
            follows that state directly.
        */
        const bool gateIsHigh = midi.gate != 0 && midi.noteNumber >= 0;
        const float currentMidiNote = static_cast<float>(midi.noteNumber) + tuneSemitones + pitchBendSemitones;

        if (gateIsHigh && (! previousGate || std::abs(currentMidiNote - lastPlayedMidiNote) > 0.0001f))
        {
            lastPlayedMidiNote = currentMidiNote;
            heldVelocity = std::max(0.05f, midi.velocity);
            oscillator.reset();
            filter.reset();
            envelope.noteOn();
        }
        else if (! gateIsHigh && previousGate)
        {
            envelope.noteOff();
        }
        else if (gateIsHigh)
        {
            /*
                Pitch wheel can move while the key is already held.
                We keep updating the note continuously so bends sound smooth.
            */
            lastPlayedMidiNote = currentMidiNote;
        }

        previousGate = gateIsHigh;
    }

    double sampleRate = 44100.0;
    SimpleSawOscillator oscillator;
    SimpleLadderFilter filter;
    SimpleGateEnvelope envelope;
    bool previousGate = false;
    float lastPlayedMidiNote = 60.0f;
    float heldVelocity = 0.0f;
};
