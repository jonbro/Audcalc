#pragma once

#include "audio/macro_oscillator.h"
#include "q15.h"
#include "audio/svf.h"
#include "Midi.h"
#include "audio/settings.h"
#include "ADSREnvelope.h"
#include "filesystem.h"
#include "voice_data.h"

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
        void Init(Midi *_midi, int16_t *temp_buffer);
        void SetOscillator(uint8_t oscillator);
        void Render(const uint8_t* sync,int16_t* buffer,size_t size);
        void RenderGlobal(const uint8_t* sync,int16_t* buffer,size_t size);
        void SetParameter(uint8_t param, uint8_t value);
        void NoteOn(int16_t key, int16_t midinote, uint8_t step, uint8_t pattern, bool livePlay, VoiceData &voiceData);
        void SetAHD(uint32_t attackTime, uint32_t holdTime, uint32_t decayTime);
        bool IsPlaying();
        void UpdateVoiceData(VoiceData &voiceData);
        void SetType(InstrumentType type)
        {
          instrumentType = type;
          if(type == INSTRUMENT_DRUMS)
            osc.set_shape(MACRO_OSC_SHAPE_KICK);
        }
        void GetParamString(uint8_t param, char *str);
        MacroOscillator osc;
        uint8_t delaySend = 0;
        uint8_t reverbSend = 0;
        VoiceData *globalParams;
        q15_t pWithMods;

    private: 
        // input value should be left shifted 7 eg: ComputePhaseIncrement(60 << 7);
        uint32_t ComputePhaseIncrement(int16_t midi_pitch);
        uint32_t phase_;
        int8_t lastPressedKey = 0;
        uint32_t envPhase;
        int16_t lastSample;
        uint8_t microFade;
        // stores the step & pattern this voice was triggered on for looking up parameter locks
        // must be initialized so we don't send junk data to the parameter lookup
        uint8_t playingStep = 0;
        uint8_t playingPattern = 0;
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
        q15_t param1Base;
        q15_t param2Base;
        q15_t timbre;
        q15_t mainCutoff;
        uint8_t resonance;
        uint32_t sampleOffset; 
        uint32_t lfo_phase = 0;
        uint32_t lfo_phase_increment = 0;
        q15_t lfo_depth = 0;
        q15_t lfo_rate = 0;
        
        uint32_t sampleEnd; 
        ffs_file *file = 0;
        // stored in the displayed param values (since the user doesn't have access to more than this anyways)
        // (maybe I add a fine tune?)
        uint32_t sampleStart[16];
        uint32_t sampleLength[16];
        int8_t playingMidiNotes[16];
        EnvTargets env1Target, env2Target;
        int16_t pitch;
        q15_t env1Depth, env2Depth;
        q15_t distortionAmount;
        uint8_t currentMidiNote;
        uint32_t fullSampleLength;
        ADSREnvelope env, env2;
        uint16_t lastenvval = 0, lastenv2val = 0;
        MacroOscillatorShape shape = MACRO_OSC_SHAPE_CSAW;
        LoopMode loopMode = INSTRUMENT_LOOPMODE_NONE;
        SamplePlaybackSegment sampleSegment = SMP_COMPLETE;
        uint32_t phase_increment = 0;
};