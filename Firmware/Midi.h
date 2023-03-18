#pragma once

#include "hardware/uart.h"
#include "pico/stdlib.h"

#define MIDI_BUF_LENGTH_POW 5
#define MIDI_BUF_LENGTH 1 << MIDI_BUF_LENGTH_POW

class Midi
{
    public:
        void Init();
        void StartSequence();
        void StopSequence();
        void TimingClock();
        void NoteOn(uint8_t channel, uint8_t pitch, uint8_t velocity);
        void NoteOff(uint8_t channel, uint8_t pitch);
        int GetNote();
        uint16_t Write(const uint8_t* data, uint16_t length);
        void Flush();
        void(*OnCCChanged)(uint8_t cc, uint8_t newValue) = NULL;
    private:
        uint8_t lastCCValue[128]; // used for filtering out values so we don't send them all the time
        bool initialized = false;
        uint8_t pingPong = 0; // what half of the buffer we are sending
        uint8_t TxBuffer[MIDI_BUF_LENGTH*2];
        uint16_t TxIndex;
        uint16_t DmaChannelTX;
};