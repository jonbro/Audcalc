#pragma once

#include "audio/macro_oscillator.h"
#include "q15.h"
#include "audio/svf.h"
#include "Midi.h"
#include "audio/settings.h"
#include "ADSREnvelope.h"
#include "filesystem.h"

using namespace braids;
enum InstrumentParameter {
  INSTRUMENT_PARAM_MACRO_TIMBRE,
  INSTRUMENT_PARAM_MACRO_MODULATION
};
enum InstrumentType {
  INSTRUMENT_MACRO,
  INSTRUMENT_MIDI,
  INSTRUMENT_DRUMS,
  INSTRUMENT_SAMPLE,
};
enum EnvelopeSegment {
  ENV_SEGMENT_ATTACK = 0,
  ENV_SEGMENT_HOLD = 1,
  ENV_SEGMENT_DECAY = 2,
  ENV_SEGMENT_COMPLETE = 3,
  ENV_NUM_SEGMENTS,
};

enum LoopMode {
  INSTRUMENT_LOOPMODE_NONE,
  INSTRUMENT_LOOPMODE_FWD,
};
enum SamplePlaybackSegment {
  SMP_PLAYING = 0,
  SMP_COMPLETE = 1,
  SMP_NUM_SEGMENTS,
};

class Instrument
{
    public:
        void Init(Midi *_midi);
        void SetOscillator(uint8_t oscillator);
        void Render(const uint8_t* sync,int16_t* buffer,size_t size);
        void RenderGlobal(const uint8_t* sync,int16_t* buffer,size_t size);
        void SetParameter(uint8_t param, uint8_t value);
        void NoteOn(int16_t pitch, bool livePlay);
        void SetAHD(uint32_t attackTime, uint32_t holdTime, uint32_t decayTime);
        void SetType(InstrumentType type)
        {
          instrumentType = type;
          if(type == INSTRUMENT_DRUMS)
            osc.set_shape(MACRO_OSC_SHAPE_KICK);
        }
        void SetFile(ffs_file *_file)
        {
          file = _file;
        }
        void GetParamString(uint8_t param, char *str);
        MacroOscillator osc;
    private: 
        // input value should be left shifted 7 eg: ComputePhaseIncrement(60 << 7);
        uint32_t ComputePhaseIncrement(int16_t midi_pitch);
        uint32_t phase_;
        int8_t lastPressedKey = 0;
        uint32_t envPhase;
        EnvelopeSegment currentSegment;
        uint8_t attackTime, holdTime, decayTime;
        int8_t envTimbre = 0, envColor = 0;
        int8_t octave = 0;
        Svf svf;
        InstrumentType instrumentType = INSTRUMENT_MACRO;
        Midi *midi;
        uint32_t noteOffSchedule;
        int16_t lastNoteOnPitch = 0;
        bool enable_env = true;
        bool enable_filter = true;
        int8_t playingSlice = -1;
        int16_t *sample;
        q15_t volume = 0x7fff;
        q15_t color;
        q15_t timbre;
        uint8_t cutoff;
        uint8_t resonance;
        uint32_t sampleOffset; 
        ffs_file *file;
        // stored in the displayed param values (since the user doesn't have access to more than this anyways)
        // (maybe I add a fine tune?)
        uint32_t sampleStart[16];
        uint32_t sampleLength[16];
        uint32_t fullSampleLength;
        ADSREnvelope env;
        MacroOscillatorShape shape = MACRO_OSC_SHAPE_CSAW;
        LoopMode loopMode = INSTRUMENT_LOOPMODE_NONE;
        SamplePlaybackSegment sampleSegment = SMP_COMPLETE;
        uint32_t phase_increment = 0;
};