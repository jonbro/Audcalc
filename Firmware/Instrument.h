#pragma once

#include "audio/macro_oscillator.h"
#include "q15.h"
#include "audio/svf.h"

using namespace braids;
enum InstrumentParameter {
  INSTRUMENT_PARAM_MACRO_TIMBRE,
  INSTRUMENT_PARAM_MACRO_MODULATION
};
enum EnvelopeSegment {
  ENV_SEGMENT_ATTACK = 0,
  ENV_SEGMENT_HOLD = 1,
  ENV_SEGMENT_DECAY = 2,
  ENV_SEGMENT_COMPLETE = 3,
  ENV_NUM_SEGMENTS,
};

class Instrument
{
    public:
        void Init();
        void SetOscillator(uint8_t oscillator);
        void Render(const uint8_t* sync,int16_t* buffer,size_t size);
        
        void SetParameter(InstrumentParameter param, int8_t value);
        void SetPitch(int16_t pitch);
        void Strike();
        void SetAHD(uint32_t attackTime, uint32_t holdTime, uint32_t decayTime);
    private:
        MacroOscillator osc;
        uint32_t envPhase;
        EnvelopeSegment currentSegment;
        uint32_t attackTime, holdTime, decayTime;
        Svf svf;
};