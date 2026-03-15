#include "PolySynthAdsrProcessor.h"

/*
    Exactly like the mono example, this translation unit exists mainly to
    anchor the project and to show that the real implementation can live in
    separate, focused class files.

    The processor itself is declared in PolySynthAdsrProcessor.h.
    Each supporting concept gets its own file:

    - MidiNoteUtils.h         : note / pitch helper math
    - SimpleSawOscillator.h   : waveform generation
    - AdsrEnvelope.h          : note articulation over time
    - StateVariableLowPass.h  : resonant low-pass filtering
    - PolySynthVoice.h        : one complete playable voice

    This project is intentionally structured like a tiny synth codebase
    rather than a one-file demo.
*/
