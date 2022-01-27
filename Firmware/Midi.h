#pragma once

#include "hardware/uart.h"
#include "pico/stdlib.h"

class Midi
{
    public:
        void Init();
        void NoteOn(int16_t pitch);
        void NoteOff(int16_t pitch);
};