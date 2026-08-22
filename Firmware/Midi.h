#pragma once

#include "hardware/uart.h"
#include "pico/stdlib.h"

// midiTX is double buffered, 256 bytes per each half of the buffer
#define MIDI_BUF_LENGTH_POW 8
#define MIDI_BUF_LENGTH (1 << MIDI_BUF_LENGTH_POW)

void midi_task();

typedef struct {
    int8_t dataByteCounter = -1;
    char lastCommand;
    char dataBuffer[4]; // only handle incoming messages with a maximum size of 4 bytes
} InputProcessor;

class Midi
{
    public:
        void Init();
        void StartSequence();
        void StopSequence();
        void TimingClock();
        void NoteOn(uint8_t channel, uint8_t pitch, uint8_t velocity);
       
        // returns false if the send fails
        bool NoteOff(uint8_t channel, uint8_t pitch);

        // returns false if the send fails
        // Repeats of a value the channel already holds are skipped, not sent.
        // returns false if the send fails
        bool ControlChange(uint8_t channel, uint8_t cc, uint8_t value);
        
        // program change, skips repeated changes to the same thing
        // returns false if the send fails (not currently used)
        bool ProgramChange(uint8_t channel, uint8_t program);
        
        // clear the last sent ccs, so they are all resent on the next change
        void ResetSentCCState();
        
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
        void ProcessMessage(char msg, uint8_t processor);
    private:
        // per input (trs / usb) so the same cc value on both isn't filtered as a repeat
        uint8_t lastCCValue[2][128];
        // last value transmitted per channel + cc number.
        // cached per (midi) channel so that multiple voices targeting the same midi channel get sent
        uint8_t lastSentCCValue[16][128];
        // program change carries no controller number, so it's one slot per channel
        uint8_t lastSentProgram[16];
        bool initialized = false;
        uint8_t pingPong = 0; // what half of the buffer we are sending
        uint8_t TxBuffer[MIDI_BUF_LENGTH*2];
        uint16_t TxIndex;
        uint16_t DmaChannelTX;
        // processors are trs, usb
        InputProcessor inputProcessors[2];
};

extern Midi *midi; // uart callbacks go here
