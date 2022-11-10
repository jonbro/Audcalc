#pragma once

#include "hardware/uart.h"
#include "pico/stdlib.h"

class Midi
{
    public:
        void Init();
        void StartSequence();
        void StopSequence();
        void TimingClock();
        void NoteOn(int16_t pitch, uint8_t velocity);
        void NoteOff(int16_t pitch);
        int GetNote();
};