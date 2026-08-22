#ifndef VOICE_DATA_H_
#define VOICE_DATA_H_

#include <stdio.h>
#include <string.h>
#include "audio/macro_oscillator.h"
#include "audio/quantizer_scales.h"
#include "filesystem.h"
#include "ParamLockPool.h"
#include "Serializer.h"
#include "VoiceDataInternal.pb.h"
#include <pb_encode.h>
#include <pb_decode.h>

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
enum LfoShape {
    Lfo_Shape_Sine,
    Lfo_Shape_Triangle,
    Lfo_Shape_Square,
    Lfo_Shape_RampUp,
    Lfo_Shape_RampDown,
    Lfo_Shape_Random,
    Lfo_Shape_SampleGlide,
    Lfo_Shape_Count
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

// Midi gate length, in twenty-fourths of a sequencer step
const uint16_t midiHoldTwentyFourths[16] = {
    3, 4, 6, 8, 12, 18, 24, 36, 48, 72, 96, 144, 192, 288, 384, 576
};
const char *const midiHoldLabels[16] = {
    "1/8", "1/6", "1/4", "1/3", "1/2", "3/4", "1", "1.5",
    "2", "3", "4", "6", "8", "12", "16", "24"
};

enum ParamType {
    Timbre = 10,
    SampleIn = 10,
    Color = 11,
    SampleOut = 11,
    MidiHold = 11,
    Cutoff = 12,
    ChordVoices = 12,
    Resonance = 13,
    ChordShape = 13,
    Volume = 14,
    Pan = 15,
    Portamento = 16,
    FineTune = 17,
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
    Lfo1Shape = 35,
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

// midi cc parameter pads
#define MIDI_CC_PAGE_COUNT 9
#define MIDI_CC_SLOT_COUNT (MIDI_CC_PAGE_COUNT*2)
// confirm that cc slots matches the voice data slots
static_assert(sizeof(VoiceDataInternal::midiCC) == MIDI_CC_SLOT_COUNT,
              "midiCC proto array must have one entry per cc slot");
// pressing an already selected cc page a second time jumps editing the cc routing8
#define PARAM_PAGE_STRIDE 25
extern const uint8_t MidiCCPadParams[MIDI_CC_PAGE_COUNT];

// midi cc targets can also target program change
enum MidiCCSlotTarget {
    MIDI_CC_SLOT_OFF = 0,      // slot transmits nothing at all
    MIDI_CC_SLOT_PROGRAM = 1,  // slot sends program change instead of a cc
    MIDI_CC_SLOT_CC_BASE = 2,  // positions 2..129 are cc 0..127
};
#define MIDI_CC_SLOT_POSITIONS (MIDI_CC_SLOT_CC_BASE + 128)
// Compact "CC" glyph, two small Cs stacked in the cell a single character occupies.
// "CC107" spelled out is 35px against the 33px the label column has; with this it is
// 28px, the same as every other four character label.
#define CC_LIGATURE ""

#define MIDI_CHORD_SHAPE_COUNT 15
// Extra voices are capped at 5 (six notes sounding). A retrigger restrikes the
// whole chord as note off + note on per voice, so each extra voice costs 6 bytes
// of a 3125 byte/sec din link every retrigger - six notes is what fits.
#define MIDI_CHORD_MAX_EXTRA_VOICES 5
#define MIDI_CHORD_MAX_NOTES (MIDI_CHORD_MAX_EXTRA_VOICES + 1)

class Midi;

class VoiceData
{
    public: 
        VoiceData()
        {
            internalData = VoiceDataInternal_init_default;
            InitDefaults();
        }
        void SetDefaultParams();
        void DoublePatternLength(uint8_t pattern)
        {
            uint8_t priorLength = internalData.patterns[pattern].length/4+1; // I store the pattern lengths all weird - sue me.
            uint8_t targetLength = priorLength*2;
            if(targetLength>64)
                return;
            noteCountForPattern[pattern] = noteCountForPattern[pattern]*2;
            internalData.patterns[pattern].length = (targetLength-1)*4;
            for (size_t i = 0; i < priorLength; i++)
            {
                internalData.patterns[pattern].notes[i+priorLength] = internalData.patterns[pattern].notes[i]; // 1x 
                internalData.patterns[pattern].keys[i+priorLength] = internalData.patterns[pattern].keys[i]; // 1x 
            }
        }

        void InitDefaults();
        void Serialize(pb_ostream_t *s);
        void Deserialize(pb_istream_t *s);
        void CopyPattern(uint8_t from, uint8_t to)
        {
            internalData.patterns[to].rate = internalData.patterns[from].rate; // 1x 
            internalData.patterns[to].length = internalData.patterns[from].length; // need to up this to fit into 0xff
            for (size_t i = 0; i < 64; i++)
            {
                internalData.patterns[to].notes[i] = internalData.patterns[from].notes[i]; // 1x 
                internalData.patterns[to].keys[i] = internalData.patterns[from].keys[i]; // 1x 
            }
            noteCountForPattern[to] = noteCountForPattern[from];
        }
        void SetNoteForPattern(uint8_t pattern, uint8_t note, uint8_t value)
        {
            bool lastNoteActive = (internalData.patterns[pattern].notes[note] >> 7) == 1;
            bool currentNoteActive = (value >> 7) == 1;
            internalData.patterns[pattern].notes[note] = value;
            if(!lastNoteActive && currentNoteActive)
            {
                noteCountForPattern[pattern]++;
            }
            else if(lastNoteActive && !currentNoteActive)
            {
                noteCountForPattern[pattern]--;
            }
        }
        bool HasNotesForPattern(uint8_t pattern)
        {
            return noteCountForPattern[pattern] > 0;
        }
        uint8_t* GetNotesForPattern(uint8_t pattern)
        {
            return internalData.patterns[pattern].notes;
        }
        uint8_t* GetKeysForPattern(uint8_t pattern)
        {
            return internalData.patterns[pattern].keys;
        }
        uint8_t GetRateForPattern(uint8_t pattern)
        {
            return internalData.patterns[pattern].rate;
        }
        uint8_t GetLengthForPattern(uint8_t pattern)
        {
            return internalData.patterns[pattern].length;
        }
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
        bool MidiParamsAndLocks(uint8_t param, uint8_t step, uint8_t pattern, char *strA, char *strB, char *pA, char *pB, bool &lockA, bool &lockB, bool showForStep);
        void DrawParamString(uint8_t param, char *str, uint8_t lastNotePlayed, uint8_t currentPattern, uint8_t paramLock, bool showForStep);
        bool CheckLockAndSetDisplay(bool showForStep, uint8_t step, uint8_t pattern, uint8_t param, uint8_t value, char *paramString);
        uint8_t GetParamValue(ParamType param, uint8_t lastNotePlayed, uint8_t step, uint8_t currentPattern);

        static constexpr uint8_t PARAM_CACHE_SIZE = 46; // ReverbSend=45 is highest param enum value
        void FillResolvedParamCache(uint8_t step, uint8_t pattern, uint8_t lastNotePlayed, uint8_t* cache);

        uint8_t GetMidiChannel(){
            return internalData.extraTypeUnion.midiChannel >> 4;
        }

        /* MIDI CHORDS */
        // number of extra notes stacked on top of the root, 0-5
        static uint8_t ChordVoiceCount(uint8_t rawValue)
        {
            return (((uint16_t)rawValue)*(MIDI_CHORD_MAX_EXTRA_VOICES+1))>>8;
        }
        static uint8_t ChordShapeIndex(uint8_t rawValue)
        {
            return (((uint16_t)rawValue)*MIDI_CHORD_SHAPE_COUNT)>>8;
        }
        // fills notes[] with the root followed by the extra chord voices and
        // returns how many were written (always at least 1)
        static uint8_t BuildChord(int16_t root, uint8_t rawVoices, uint8_t rawShape, int16_t *notes);

        /* MIDI CC OUTPUT */
        // which cc page a param pad drives, or -1 if the pad isn't a cc page
        static int8_t MidiCCPageForPad(uint8_t pad);
        // slot behind a GetParam index on a cc number editing page, or -1
        static int8_t MidiCCSlotForParamIndex(uint8_t paramIndex);
        // what this slot points at: MIDI_CC_SLOT_OFF, MIDI_CC_SLOT_PROGRAM, or
        // MIDI_CC_SLOT_CC_BASE + the cc number
        uint8_t GetMidiCCSlotTarget(uint8_t slot)
        {
            return (((uint16_t)internalData.midiCC[slot]) * MIDI_CC_SLOT_POSITIONS) >> 8;
        }
        // writes "Off", "Prog" or the cc number into out, optionally "CC" prefixed
        void FormatMidiCCSlot(uint8_t slot, char *out, bool ccPrefix);
        // sends every mapped slot, reading from a cache already resolved by
        // FillResolvedParamCache. Midi drops the ones already on the wire.
        void SendMidiCCs(Midi *midi, const uint8_t *resolvedParams);
        // pushes a single slot immediately, used while a knob is being turned
        void SendMidiCC(Midi *midi, uint8_t slot, uint8_t value);

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
        void SetFile(ffs_file *_file)
        {
          file = _file;
        }
        ffs_file* GetFile()
        {
          return file;
        }
        uint8_t GetLength(uint8_t pattern)
        {
            return internalData.patterns[pattern].length/4+1;
        }
        VoiceDataInternal* GetVoiceData()
        {
            return &internalData;
        }
        void StoreParamLock(uint8_t param, uint8_t step, uint8_t pattern, uint8_t value);
        void ClearParameterLocks(uint8_t pattern);
        void RemoveLocksForStep(uint8_t pattern, uint8_t step);
        void CopyParameterLocks(uint8_t fromPattern, uint8_t toPattern);
        bool HasLockForStep(uint8_t step, uint8_t pattern, uint8_t param, uint8_t &value);
        bool HasAnyLockForStep(uint8_t step, uint8_t pattern);
        uint16_t CountLocksForPattern(uint8_t pattern);
        bool LockableParam(uint8_t param);
        
        void SetNextRequestedStep(uint8_t step)
        {
            nextRequestedStep = step | 0x80;
        }
        void ClearNextRequestedStep()
        {
            nextRequestedStep = 0;
        }
        
        static void SerializeStatic(pb_ostream_t *s);
        static void DeserializeStatic(pb_istream_t *s, VoiceData *patterns);
        void CopyFrom(VoiceData &copy)
        {
            internalData.instrumentType = copy.internalData.instrumentType;
            // this copies the subtype (sampler type, synth shape or midichannel)
            internalData.extraTypeUnion.samplerType = copy.internalData.extraTypeUnion.samplerType;

            internalData.delaySend = copy.internalData.delaySend;
            internalData.reverbSend = copy.internalData.reverbSend;

            internalData.portamento = copy.internalData.portamento;
            internalData.fineTune = copy.internalData.fineTune;

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
            internalData.lfoShape = copy.internalData.lfoShape;

            internalData.color = copy.internalData.color;
            internalData.timbre = copy.internalData.timbre;
            internalData.cutoff = copy.internalData.cutoff;
            internalData.resonance = copy.internalData.resonance;
            internalData.volume = copy.internalData.volume;
        }


        uint8_t nextRequestedStep;

        // these are per pattern
        uint8_t nothing; // used for returning a reference when we don't want it to do anything
        
        static ParamLockPool lockPool;
        uint16_t locksForPatternStep[16][64];
        uint8_t noteCountForPattern[16] = {0}; 
    private:
        VoiceDataInternal internalData;
        bool GetLockForStep(ParamLock **lockOut, uint8_t step, uint8_t pattern, uint8_t param);
        ffs_file *file;
};

void TestVoiceData();

#endif // VOICEDATA_H_