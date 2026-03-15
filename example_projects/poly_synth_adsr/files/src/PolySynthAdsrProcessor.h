#pragma once

#include <array>
#include <cmath>

#include "MidiNoteUtils.h"
#include "PolySynthVoice.h"

class PolySynthAdsrProcessor
{
public:
    const char* getName() const
    {
        return "Poly Synth ADSR";
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

        for (auto& voice : voices)
            voice.prepare(sampleRate);

        reset();
    }

    void reset()
    {
        for (auto& voice : voices)
            voice.reset();

        previousVoiceOrders.fill(0);
        previousVoiceActive.fill(false);
    }

    void process(const float* const*,
                 float* const* outputs,
                 int,
                 int numOutputChannels,
                 int numSamples)
    {
        /*
            The host already keeps track of polyphonic MIDI notes for us.

            We use midi.voices[] as the "truth" about which notes are held.
            Each slot in midi.voices[] is mirrored by one PolySynthVoice.

            This keeps the example easy to reason about:

            host MIDI voice slot 0 -> synth voice 0
            host MIDI voice slot 1 -> synth voice 1
            ...

            If a host voice slot becomes active with a new order number, we
            start a new synth note in the matching voice.
            If that slot turns inactive, we release the matching ADSR.
        */
        const auto settings = buildRenderSettings();
        syncVoicesWithMidi(settings);

        int activeVoiceCount = 0;

        for (const auto& voice : voices)
            activeVoiceCount += voice.isActive() ? 1 : 0;

        const float normalisation = activeVoiceCount > 0
                                  ? 1.0f / std::sqrt(static_cast<float>(activeVoiceCount))
                                  : 0.0f;
        const float outputLevel = controls.levelKnob * 0.22f;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            float mixedSample = 0.0f;

            for (auto& voice : voices)
                mixedSample += voice.renderSample(settings);

            const float outputSample = mixedSample * normalisation * outputLevel;

            for (int channel = 0; channel < numOutputChannels; ++channel)
            {
                if (outputs[channel] != nullptr)
                    outputs[channel][sample] = outputSample;
            }
        }
    }

private:
    PolySynthVoice::RenderSettings buildRenderSettings() const
    {
        PolySynthVoice::RenderSettings settings;
        settings.attackSeconds = polysynth::mapExponential(controls.attackKnob, 0.001f, 2.0f);
        settings.decaySeconds = polysynth::mapExponential(controls.decayKnob, 0.01f, 2.5f);
        settings.sustainLevel = controls.sustainKnob;
        settings.releaseSeconds = polysynth::mapExponential(controls.releaseKnob, 0.02f, 4.0f);
        settings.filterCutoffHz = polysynth::mapExponential(controls.cutoffKnob, 60.0f, 6000.0f);
        settings.resonance = controls.resonanceKnob;
        settings.envelopeToCutoffHz = controls.envAmountKnob * 7000.0f;
        return settings;
    }

    void syncVoicesWithMidi(const PolySynthVoice::RenderSettings& settings)
    {
        for (int voiceIndex = 0; voiceIndex < DSP_EDU_USER_DSP_MAX_MIDI_VOICES; ++voiceIndex)
        {
            const auto& midiVoice = midi.voices[voiceIndex];
            const auto voiceSlot = static_cast<std::size_t>(voiceIndex);
            auto& synthVoice = voices[voiceSlot];
            const bool midiVoiceIsActive = midiVoice.active != 0;
            const bool wasPreviouslyActive = previousVoiceActive[voiceSlot];

            if (midiVoiceIsActive)
            {
                const int pitchWheelIndex = std::clamp(midiVoice.channel - 1, 0, DSP_EDU_USER_DSP_MAX_MIDI_CHANNELS - 1);
                const float pitchWheel = midi.channelPitchWheel[pitchWheelIndex];

                if (! wasPreviouslyActive || midiVoice.order != previousVoiceOrders[voiceSlot])
                {
                    synthVoice.startNote(midiVoice.noteNumber,
                                         midiVoice.velocity,
                                         midiVoice.channel,
                                         pitchWheel,
                                         settings);
                }
                else
                {
                    synthVoice.updateEnvelopeSettings(settings);
                    synthVoice.setPitchWheel(pitchWheel);
                }
            }
            else if (wasPreviouslyActive)
            {
                synthVoice.noteOff();
            }
            else if (synthVoice.isActive())
            {
                /*
                    A voice can still be active here while its MIDI slot is no
                    longer held, because the ADSR release stage may still be
                    fading out. We still update the envelope times so the
                    Release knob reacts immediately.
                */
                synthVoice.updateEnvelopeSettings(settings);
            }

            previousVoiceActive[voiceSlot] = midiVoiceIsActive;
            previousVoiceOrders[voiceSlot] = midiVoice.order;
        }
    }

    double sampleRate = 44100.0;
    std::array<PolySynthVoice, DSP_EDU_USER_DSP_MAX_MIDI_VOICES> voices;
    std::array<std::uint32_t, DSP_EDU_USER_DSP_MAX_MIDI_VOICES> previousVoiceOrders {};
    std::array<bool, DSP_EDU_USER_DSP_MAX_MIDI_VOICES> previousVoiceActive {};
};
