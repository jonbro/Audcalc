#ifndef VOICE_DATA_H_
#define VOICE_DATA_H_

#include <stdio.h>
#include <string.h>
#include "audio/macro_oscillator.h"
#include "audio/quantizer_scales.h"
#include "filesystem.h"
using namespace braids;

typedef enum {
  INSTRUMENT_MACRO,
  INSTRUMENT_SAMPLE,
  INSTRUMENT_MIDI,
  INSTRUMENT_DRUMS,
  INSTRUMENT_GLOBAL = 7 // this is normally inaccessible, only the main system can set it.
} InstrumentType;

class VoiceData
{
    public: 
        void GetParamString(uint8_t param, char *str, uint8_t lastNotePlayed);
        MacroOscillatorShape GetShape(){
            return (MacroOscillatorShape)((((uint16_t)shape)*41) >> 8);
        }
        int8_t GetOctave();
        uint8_t& GetParam(uint8_t param, uint8_t lastNotePlayed);

        InstrumentType GetInstrumentType() {
            return global_instrument?INSTRUMENT_GLOBAL:(InstrumentType)((((uint16_t)instrumentTypeBare)*4) >> 8);
        }
        void SetInstrumentType(InstrumentType instrumentType) {
            if(instrumentType==INSTRUMENT_GLOBAL)
                global_instrument = true;
            else
                instrumentTypeBare = (instrumentType * (0xff / 4));
        }
        void SetFile(ffs_file *_file)
        {
          file = _file;
        }
        ffs_file *file;
        bool    global_instrument = false; // this ... should get shared somewhere, lots of nonsense to cart around
        uint8_t instrumentTypeBare = 0;
        uint8_t notes[16*16]; // top bit is if the note is triggered
        uint8_t delaySend;
        uint8_t attackTime = 0x20;
        uint8_t holdTime;
        uint8_t decayTime = 0x20;
        uint8_t envTimbre;
        uint8_t envColor;
        
        uint8_t sampleStart[16];
        uint8_t sampleLength[16];

        union
        {
            uint8_t bpm;
            uint8_t octave = 0x7f;
        };
        union
        {
            uint8_t amp_enabled;
            uint8_t color = 0x7f;
        };
        union
        {
            uint8_t shape = 0;
            uint8_t scale;
        };
        
        uint8_t timbre = 0x7f;
        uint8_t cutoff = 0xff;
        uint8_t resonance = 0;
        uint8_t sampleOffset; 
        uint8_t sampleEnd; 
        uint8_t volume = 0x7f;
        uint8_t nothing; // used for returning a reference when we don't want it to do anything
};

#endif // VOICEDATA_H_