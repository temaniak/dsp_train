#pragma once

class SimpleSawOscillator
{
public:
    void reset()
    {
        phase = 0.0f;
    }

    float process(float frequencyHz, float sampleRate)
    {
        /*
            This oscillator is deliberately simple:

            - phase moves from 0 to 1
            - the saw wave is just a remapped version of that phase
            - we wrap phase back into the 0..1 interval every sample

            The class does not know anything about MIDI, envelopes, filters,
            or stereo. It only knows how to produce a basic waveform.
        */
        const float sample = 2.0f * phase - 1.0f;

        phase += frequencyHz / sampleRate;

        while (phase >= 1.0f)
            phase -= 1.0f;

        return sample;
    }

private:
    float phase = 0.0f;
};
