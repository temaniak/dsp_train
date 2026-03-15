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
            The poly synth uses the same basic saw oscillator as the mono one.

            Reusing the same simple building block helps learners recognise
            which parts of a synth stay the same when moving from mono to poly:

            - the oscillator itself does not care how many voices exist
            - the voice management happens one level above
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
