#ifndef VOICE_DATA_H_
#define VOICE_DATA_H_

#include <stdio.h>
#include <string.h>
#include "audio/macro_oscillator.h"
#include "audio/quantizer_scales.h"
#include "filesystem.h"
#include "ParamLockPool.h"
#include "Serializer.h"

extern "C" {
  #include "ssd1306.h"
}

using namespace braids;

enum InstrumentType {
  INSTRUMENT_MACRO,
  INSTRUMENT_SAMPLE,
  INSTRUMENT_MIDI,
  INSTRUMENT_DRUMS,
  INSTRUMENT_GLOBAL = 7 // this is normally inaccessible, only the main system can set it.
};

enum ParamType {
    Timbre = 0,
    SampleIn = 0,
    Color = 1,
    SampleOut = 1,
    Cutoff = 2,
    Resonance = 3,
    Volume = 4,
    Octave = 6,
    AttackTime = 8,
    DecayTime = 9,
    Length = 24,
    DelaySend = 28
};

enum SamplerPlayerType
{
  SAMPLE_PLAYER_SLICE,
  SAMPLE_PLAYER_PITCH
};

class VoiceData
{
    public: 
        VoiceData()
        {
            for (size_t i = 0; i < 16; i++)
            {
                length[i] = 15*4; // need to up this to fit into 0xff
                locksForPattern[i] = ParamLockPool::NullLock();
            }
            for (size_t i = 0; i < 64*16; i++)
            {
                notes[i] = 0;
            }
        }
        void GetParamString(uint8_t param, char *str, uint8_t lastNotePlayed, uint8_t currentPattern);
        void GetParamsAndLocks(uint8_t param, uint8_t step, uint8_t pattern, char *strA, char *strB, uint8_t lastNotePlayed, char *pA, char *pB, bool &lockA, bool &lockB);
        void DrawParamString(uint8_t param, char *str, uint8_t lastNotePlayed, uint8_t currentPattern, uint8_t paramLock);
        bool CheckLockAndSetDisplay(uint8_t step, uint8_t pattern, uint8_t param, uint8_t value, char *paramString);
        uint8_t GetParamValue(ParamType param, uint8_t lastNotePlayed, uint8_t step, uint8_t currentPattern);

        MacroOscillatorShape GetShape(){
            return (MacroOscillatorShape)((((uint16_t)shape)*41) >> 8);
        }
        SamplerPlayerType GetSampler(){
            return (SamplerPlayerType)((samplerTypeBare*2)>>8);
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
        void StoreParamLock(uint8_t param, uint8_t step, uint8_t pattern, uint8_t value)
        {
            ParamLock *lock = GetLockForStep(0x80|step, pattern, param);
            if(lock != ParamLockPool::NullLock())
            {
                lock->value = value;
                printf("updated param lock step: %i param: %i value: %i\n", step, param, value);
                return;
            }
            if(lockPool.GetParamLock(&lock))
            {
                printf("new address %x\n", lock);
                lock->param = param;
                lock->step = step;
                lock->value = value;
                lock->next = lockPool.GetLockPosition(locksForPattern[pattern]);
                locksForPattern[pattern] = lock;
                printf("added param lock step: %i param: %i value: %i\n", step, param, value);
                return;
            }
            printf("failed to add param lock\n");
        }
        void CopyParameterLocks(uint8_t fromPattern, uint8_t toPattern)
        {
            ParamLock* lock = locksForPattern[fromPattern];
            while(lock != ParamLockPool::NullLock())
            {
                StoreParamLock(lock->param, lock->step, toPattern, lock->value);
                lock = lockPool.GetLock(lock->next);
            }
        }
        void SetNextRequestedStep(uint8_t step)
        {
            nextRequestedStep = step | 0x80;
        }
        void ClearNextRequestedStep()
        {
            nextRequestedStep = 0;
        }
        bool HasLockForStep(uint8_t step, uint8_t pattern, uint8_t param, uint8_t &value)
        {
            if(step>>7==0)
                return false;
            ParamLock* lock = GetLockForStep(step, pattern, param);
            if(lock != ParamLockPool::NullLock())
            {
                value = lock->value;
                return true;
            }
            return false;
        }

        ParamLock* GetLockForStep(uint8_t step, uint8_t pattern, uint8_t param)
        {
            if(step>>7==0)
                return ParamLockPool::NullLock();
            ParamLock* lock = locksForPattern[pattern];
            while(lock != ParamLockPool::NullLock())
            {
                if(lock->param == param && lock->step == (step&0x7f))
                {
                    return lock;
                }
                lock = lockPool.GetLock(lock->next);
            }
            return ParamLockPool::NullLock();
        }

        bool LockableParam(uint8_t param);
        
        static void SerializeStatic(Serializer &s);
        static void DeserializeStatic(Serializer &s);


        ParamLock* locksForPattern[16];

        uint8_t nextRequestedStep;
        ffs_file *file;
        bool    global_instrument = false; // this ... should get shared somewhere, lots of nonsense to cart around
        uint8_t instrumentTypeBare = 0;
        uint8_t samplerTypeBare = 0;
        uint8_t paramLockCount = 0;

        uint8_t delaySend;
        uint8_t attackTime = 0x20;
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
        union
        {
            uint8_t timbre = 0x7f;
            uint8_t chromatic;
        };
        
        uint8_t cutoff = 0xff;
        uint8_t resonance = 0;
        uint8_t volume = 0x7f;
        uint8_t nothing; // used for returning a reference when we don't want it to do anything
    private:
        static ParamLockPool lockPool;
};

void TestVoiceData();

#endif // VOICEDATA_H_