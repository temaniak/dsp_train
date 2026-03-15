#include "SimpleSynthVoiceProcessor.h"

/*
    This file intentionally contains only one include.

    The educational goal of this example is to show that a user DSP project
    can be split into small, readable classes instead of placing the whole
    synth into one large file.

    The actual processor class lives in SimpleSynthVoiceProcessor.h.
    The helper classes it uses are declared in their own headers:

    - MidiNoteUtils.h
    - SimpleSawOscillator.h
    - SimpleGateEnvelope.h
    - SimpleLadderFilter.h

    Keeping main.cpp this small makes the "entry point" obvious while still
    letting the learner inspect each building block one by one.
*/
