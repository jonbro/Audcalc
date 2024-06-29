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
        uint16_t Write(const uint8_t* data, uint16_t length);
        void Flush();
        void (*OnSync)() = NULL; // 0xf8
        void (*OnStart)() = NULL; // 0xfa
        void (*OnContinue)() = NULL; // 0xfb
        void (*OnStop)() = NULL; // 0xfc
        void (*OnPosition)(uint16_t postion) = NULL; // 0xf2, 2 data bytes
        void (*OnNoteOn)(uint8_t channel, uint8_t note, uint8_t velocity) = NULL; // 0x9x, 2 data bytes
        void (*OnNoteOff)(uint8_t channel, uint8_t note, uint8_t velocity) = NULL; // 0x8x, 2 data bytes
        void(*OnCCChanged)(uint8_t cc, uint8_t newValue) = NULL; // 0xBx, 2 data bytes
        void ProcessMessage(char msg);
    private:
        uint8_t lastCCValue[128]; // used for filtering out values so we don't send them all the time
        bool initialized = false;
        uint8_t pingPong = 0; // what half of the buffer we are sending
        uint8_t TxBuffer[MIDI_BUF_LENGTH*2];
        uint16_t TxIndex;
        uint16_t DmaChannelTX;
        int8_t dataByteCounter = -1;
        char lastCommand;
        char dataBuffer[4]; // only handle incoming messages with a maximum size of 4 bytes
};

extern Midi *midi; // uart callbacks go here
