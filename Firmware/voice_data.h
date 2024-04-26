#ifndef VOICE_DATA_H_
#define VOICE_DATA_H_

#include <stdio.h>
#include <string.h>
#include "audio/macro_oscillator.h"
#include "audio/quantizer_scales.h"
#include "VoiceDataInternal.pb.h"
#include <pb_encode.h>
#include <pb_decode.h>
#include "lua.hpp"

extern "C" {
  #include "ssd1306.h"
}

using namespace braids;

extern const uint8_t ConditionalEvery[];

enum InstrumentType {
  INSTRUMENT_MACRO,
  INSTRUMENT_SAMPLE,
  INSTRUMENT_MIDI,
  INSTRUMENT_DRUMS,
  INSTRUMENT_GLOBAL = 7 // this is normally inaccessible, only the main system can set it.
};

enum ConditionModeEnum {
  CONDITION_MODE_NONE,
  CONDITION_MODE_RAND,
  CONDITION_MODE_LENGTH,
};

enum EnvTargets {
    Target_Volume,
    Target_Timbre,
    Target_Color,
    Target_Cutoff,
    Target_Resonance,
    Target_Pitch,
    Target_Pan,
    Target_Count
};
enum LfoTargets {
    Lfo_Target_Volume,
    Lfo_Target_Timbre,
    Lfo_Target_Color,
    Lfo_Target_Cutoff,
    Lfo_Target_Resonance,
    Lfo_Target_Pitch,
    Lfo_Target_Pan,
    Lfo_Target_Env1Attack,
    Lfo_Target_Env1Decay,
    Lfo_Target_Env2Attack,
    Lfo_Target_Env2Decay,
    Lfo_Target_Env12Attack,
    Lfo_Target_Env12Decay,
    Lfo_Target_Count
};
enum ParamType {
    Timbre = 10,
    SampleIn = 10,
    Color = 11,
    SampleOut = 11,
    MidiHold = 11,
    Cutoff = 12,
    Resonance = 13,
    Volume = 14,
    Pan = 15,
    AttackTime = 20,
    DecayTime = 21,
    AttackTime2 = 22,
    DecayTime2 = 23,
    LFORate = 24,
    LFODepth = 25,
    RetriggerSpeed = 26,
    RetriggerLength = 27,
    Env1Target = 30,
    Env1Depth = 31,
    Env2Target = 32,
    Env2Depth = 33,
    Lfo1Target = 34,
    RetriggerFade = 36,
    Length = 40,
    ConditionMode = 42,
    ConditionData = 43,
    DelaySend = 44, 
    ReverbSend = 45
};

enum SamplerPlayerType
{
  SAMPLE_PLAYER_SLICE,
  SAMPLE_PLAYER_PITCH,
  SAMPLE_PLAYER_SEQL // slice, with even cuts
};

class VoiceData
{
    public: 
        VoiceData()
        {
            internalData = VoiceDataInternal_init_default;
            InitDefaults();
        }
        void InitDefaults();
        uint8_t GetSampleStart(uint8_t key)
        {
            return internalData.sampleStart[key];
        }
        uint8_t GetSampleLength(uint8_t key)
        {
            return internalData.sampleLength[key];
        }
        void GetParamString(uint8_t param, char *str, uint8_t lastNotePlayed, uint8_t currentPattern);
        void GetParamsAndLocks(uint8_t param, uint8_t step, uint8_t pattern, char *strA, char *strB, uint8_t lastNotePlayed, char *pA, char *pB, bool &lockA, bool &lockB, bool showForStep);
        void DrawParamString(uint8_t param, char *str, uint8_t lastNotePlayed, uint8_t currentPattern, uint8_t paramLock, bool showForStep);
        bool CheckLockAndSetDisplay(bool showForStep, uint8_t step, uint8_t pattern, uint8_t param, uint8_t value, char *paramString);
        uint8_t GetParamValue(ParamType param, uint8_t lastNotePlayed, uint8_t step, uint8_t currentPattern);

        uint8_t GetMidiChannel(){
            return internalData.extraTypeUnion.midiChannel >> 4;
        }

        MacroOscillatorShape GetShape(){
            return (MacroOscillatorShape)((((uint16_t)internalData.extraTypeUnion.synthShape)*41) >> 8);
        }
        
        ConditionModeEnum GetConditionMode(){
            return GetConditionMode(internalData.conditionMode);
        }
        ConditionModeEnum GetConditionMode(uint8_t conditionModeOverride){
            return (ConditionModeEnum)((((uint16_t)conditionModeOverride)*3) >> 8);
        }
        SamplerPlayerType GetSampler(){
            return (SamplerPlayerType)((internalData.extraTypeUnion.samplerType*3)>>8);
        }
        int8_t GetOctave();
        uint8_t& GetParam(uint8_t param, uint8_t lastNotePlayed, uint8_t currentPattern);

        InstrumentType GetInstrumentType() {
            return (InstrumentType)((((uint16_t)internalData.instrumentType)*3) >> 8);
        }
        void SetInstrumentType(InstrumentType instrumentType) {
            internalData.instrumentType = (internalData.instrumentType * (0xff / 4));
        }
        VoiceDataInternal* GetVoiceData()
        {
            return &internalData;
        }

        
        void SetNextRequestedStep(uint8_t step)
        {
            nextRequestedStep = step | 0x80;
        }
        void ClearNextRequestedStep()
        {
            nextRequestedStep = 0;
        }
        
        void CopyFrom(VoiceData &copy)
        {
            internalData.instrumentType = copy.internalData.instrumentType;
            // this copies the subtype (sampler type, synth shape or midichannel)
            internalData.extraTypeUnion.samplerType = copy.internalData.extraTypeUnion.samplerType;

            internalData.delaySend = copy.internalData.delaySend;
            internalData.reverbSend = copy.internalData.reverbSend;
            
            internalData.sampleAttack = copy.internalData.sampleAttack;
            internalData.sampleDecay = copy.internalData.sampleDecay;

            internalData.env1.attack = copy.internalData.env1.attack;
            internalData.env1.decay = copy.internalData.env1.decay;
            internalData.env1.target = copy.internalData.env1.target;
            internalData.env1.depth = copy.internalData.env1.depth;
            
            internalData.env2.attack = copy.internalData.env2.attack;
            internalData.env2.decay = copy.internalData.env2.decay;
            internalData.env2.target = copy.internalData.env2.target;
            internalData.env2.depth = copy.internalData.env2.depth;
            
            internalData.lfoRate = copy.internalData.lfoRate;
            internalData.lfoDepth = copy.internalData.lfoDepth;
            internalData.lfoTarget = copy.internalData.lfoTarget;
            internalData.lfoDelay = copy.internalData.lfoDelay;

            internalData.color = copy.internalData.color;
            internalData.timbre = copy.internalData.timbre;
            internalData.cutoff = copy.internalData.cutoff;
            internalData.resonance = copy.internalData.resonance;
            internalData.volume = copy.internalData.volume;
        }


        uint8_t nextRequestedStep;

        // these are per pattern
        uint8_t nothing; // used for returning a reference when we don't want it to do anything
        
    private:
        VoiceDataInternal internalData;
};

void TestVoiceData();

#endif // VOICEDATA_H_