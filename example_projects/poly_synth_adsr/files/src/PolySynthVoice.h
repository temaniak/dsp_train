#pragma once

#include "AdsrEnvelope.h"
#include "MidiNoteUtils.h"
#include "SimpleSawOscillator.h"
#include "StateVariableLowPass.h"

class PolySynthVoice
{
public:
    struct RenderSettings
    {
        float attackSeconds = 0.01f;
        float decaySeconds = 0.2f;
        float sustainLevel = 0.7f;
        float releaseSeconds = 0.3f;
        float filterCutoffHz = 1200.0f;
        float resonance = 0.1f;
        float envelopeToCutoffHz = 2000.0f;
    };

    void prepare(double newSampleRate)
    {
        sampleRate = newSampleRate > 1.0 ? static_cast<float>(newSampleRate) : 44100.0f;
        oscillator.reset();
        envelope.prepare(sampleRate);
        filter.prepare(sampleRate);
        reset();
    }

    void reset()
    {
        oscillator.reset();
        envelope.reset();
        filter.reset();
        active = false;
        noteNumber = 60;
        velocity = 0.0f;
        pitchWheel = 0.0f;
        midiChannel = 1;
    }

    void startNote(int newNoteNumber, float newVelocity, int newChannel, float newPitchWheel, const RenderSettings& settings)
    {
        /*
            One PolySynthVoice represents one sounding voice inside the synth.

            The processor above us decides *which* voice slot should play a
            given MIDI note. This class only deals with the sound of that one
            voice once it has been assigned a note.
        */
        noteNumber = newNoteNumber;
        velocity = std::clamp(newVelocity, 0.0f, 1.0f);
        midiChannel = newChannel;
        pitchWheel = newPitchWheel;
        oscillator.reset();
        filter.reset();
        envelope.setParameters(settings.attackSeconds,
                               settings.decaySeconds,
                               settings.sustainLevel,
                               settings.releaseSeconds);
        envelope.noteOn();
        active = true;
    }

    void setPitchWheel(float newPitchWheel)
    {
        pitchWheel = newPitchWheel;
    }

    void updateEnvelopeSettings(const RenderSettings& settings)
    {
        envelope.setParameters(settings.attackSeconds,
                               settings.decaySeconds,
                               settings.sustainLevel,
                               settings.releaseSeconds);
    }

    void noteOff()
    {
        envelope.noteOff();
    }

    float renderSample(const RenderSettings& settings)
    {
        if (! active)
            return 0.0f;

        const float envelopeValue = envelope.process();

        if (! envelope.isActive())
        {
            active = false;
            return 0.0f;
        }

        const float bentMidiNote = static_cast<float>(noteNumber)
                                 + polysynth::pitchWheelToSemitones(pitchWheel, 2.0f);
        const float frequencyHz = polysynth::midiNoteToFrequency(bentMidiNote);
        const float oscillatorSample = oscillator.process(frequencyHz, sampleRate);
        const float modulatedCutoff = settings.filterCutoffHz + envelopeValue * settings.envelopeToCutoffHz;
        const float filteredSample = filter.process(oscillatorSample, modulatedCutoff, settings.resonance);
        return filteredSample * envelopeValue * velocity;
    }

    bool isActive() const
    {
        return active;
    }

    int getMidiChannel() const
    {
        return midiChannel;
    }

private:
    float sampleRate = 44100.0f;
    SimpleSawOscillator oscillator;
    AdsrEnvelope envelope;
    StateVariableLowPass filter;
    bool active = false;
    int noteNumber = 60;
    int midiChannel = 1;
    float velocity = 0.0f;
    float pitchWheel = 0.0f;
};
