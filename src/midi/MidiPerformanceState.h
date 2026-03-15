#pragma once

#include "UserDspApi.h"

class MidiPerformanceState
{
public:
    MidiPerformanceState() noexcept;

    void clear() noexcept;
    void noteOn(int channel, int noteNumber, float velocity) noexcept;
    void noteOff(int channel, int noteNumber) noexcept;
    void setPitchWheel(int channel, float pitchWheel) noexcept;

    const DspEduMidiState& getState() const noexcept;

private:
    void refreshVoiceCount() noexcept;
    int findVoiceIndex(int channel, int noteNumber) const noexcept;
    int findAvailableVoiceIndex() const noexcept;

    DspEduMidiState state;
    std::uint32_t nextVoiceOrder = 1;
};
