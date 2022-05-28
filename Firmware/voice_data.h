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

struct ParamLock
{
    uint8_t step;
    uint8_t param;
    uint8_t value;
};


class VoiceData
{
    public: 
        VoiceData()
        {
            for (size_t i = 0; i < 16; i++)
            {
                length[i] = 15*4; // need to up this to fit into 0xff
            }
            for (size_t i = 0; i < 64*16; i++)
            {
                notes[i] = 0;
            }
            
        }
        void GetParamString(uint8_t param, char *str, uint8_t lastNotePlayed, uint8_t currentPattern);
        MacroOscillatorShape GetShape(){
            return (MacroOscillatorShape)((((uint16_t)shape)*41) >> 8);
        }
        int8_t GetOctave();
        uint8_t& GetParam(uint8_t param, uint8_t lastNotePlayed, uint8_t currentPattern);

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
        uint8_t GetLength(uint8_t pattern)
        {
            return length[pattern]/4+1;
        }
        void StoreParamLock(uint8_t param, uint8_t step, uint8_t value)
        {
            // check to see if this step & param already has a lock 
            for(int i=0;i<20;i++)
            {
                if(paramLocks[i].step == step && paramLocks[i].param == param)
                {
                    printf("updating param lock %i %i %i\n", step, param, value);
                    paramLocks[i].value = value;
                    return;
                }
            }
            if(paramLockCount>=20)
                return;
            paramLocks[paramLockCount++] = (ParamLock){ .step = step, .param = param, .value = value };
        }
        void SetNextRequestedStep(uint8_t step)
        {
            nextRequestedStep = step | 0x80;
        }
        void ClearNextRequestedStep()
        {
            nextRequestedStep = 0;
        }
        bool HasLockForStep(uint8_t step, uint8_t param, uint8_t *value)
        {
            if(step>>7==0)
                return false;
            for(int i=0;i<20;i++)
            {
                if(paramLocks[i].step == (step&0x7f) && paramLocks[i].param == param)
                {
                    *value = paramLocks[i].value;
                    return true;
                }
            }
            return false;
        }
        uint8_t nextRequestedStep;
        ffs_file *file;
        bool    global_instrument = false; // this ... should get shared somewhere, lots of nonsense to cart around
        uint8_t instrumentTypeBare = 0;
        uint8_t paramLockCount = 0;
        ParamLock paramLocks[20];

        uint8_t DelaySend();
        uint8_t delaySend;
        uint8_t AttackTime();
        uint8_t attackTime = 0x20;
        uint8_t HoldTime();
        uint8_t holdTime;
        uint8_t DecayTime();
        uint8_t decayTime = 0x20;
        uint8_t envTimbre;
        uint8_t envColor;

        // these are per pattern
        uint8_t rate[16];
        uint8_t length[16];
        
        uint8_t notes[64*16]; // patterns * 64 steps per pattern

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