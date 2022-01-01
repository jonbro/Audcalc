#pragma once

#include "audio/macro_oscillator.h"
#include "q15.h"
#include "audio/svf.h"
#include "Midi.h"

using namespace braids;
enum InstrumentParameter {
  INSTRUMENT_PARAM_MACRO_TIMBRE,
  INSTRUMENT_PARAM_MACRO_MODULATION
};
enum InstrumentType {
  INSTRUMENT_MACRO,
  INSTRUMENT_MIDI,
  INSTRUMENT_DRUMS
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
        void Init(Midi *_midi);
        void SetOscillator(uint8_t oscillator);
        void Render(const uint8_t* sync,int16_t* buffer,size_t size);
        void SetParameter(InstrumentParameter param, int8_t value);
        void NoteOn(int16_t pitch);
        void SetAHD(uint32_t attackTime, uint32_t holdTime, uint32_t decayTime);
        void SetType(InstrumentType type)
        {
          instrumentType = type;
        }
        MacroOscillator osc;
    private:
        uint32_t envPhase;
        EnvelopeSegment currentSegment;
        uint32_t attackTime, holdTime, decayTime;
        Svf svf;
        InstrumentType instrumentType = INSTRUMENT_MACRO;
        Midi *midi;
        uint32_t noteOffSchedule;
        int16_t lastNoteOnPitch = 0;
        bool enable_env = true;
        bool enable_filter = true;
};