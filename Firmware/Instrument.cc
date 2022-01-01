#include "Instrument.h"

void Instrument::Init(Midi *_midi)
{
    osc.Init();
    currentSegment = ENV_SEGMENT_COMPLETE;
    osc.set_pitch(54<<7);
    envPhase = 0;
    svf.Init();
    midi = _midi;
}

// Values are defined in number of samples
void Instrument::SetAHD(uint32_t attackTime, uint32_t holdTime, uint32_t decayTime)
{
    this->attackTime = attackTime;
    this->holdTime = holdTime;
    this->decayTime = decayTime;
}

#define SAMPLES_PER_BUFFER 128
void Instrument::Render(const uint8_t* sync, int16_t* buffer, size_t size)
{
    if(instrumentType == INSTRUMENT_MIDI)
    {
        for(int i=0;i<SAMPLES_PER_BUFFER;i++)
        {
            noteOffSchedule--;
            if(noteOffSchedule == 0)
            {
                //midi->NoteOff();
            }
        }
        return;
    }
    osc.Render(sync, buffer, size);
    for(int i=0;i<SAMPLES_PER_BUFFER;i++)
    {
        uint32_t mult = 0;
        uint32_t phase = 0;
        if(currentSegment == ENV_SEGMENT_ATTACK && envPhase >= attackTime)
        {
            currentSegment = ENV_SEGMENT_HOLD;
        }
        if(currentSegment == ENV_SEGMENT_HOLD && envPhase >= attackTime+holdTime)
        {
            currentSegment = ENV_SEGMENT_DECAY;
        }
        if(currentSegment == ENV_SEGMENT_DECAY && envPhase >= attackTime+holdTime+decayTime)
        {
            currentSegment = ENV_SEGMENT_COMPLETE;
        }
        q15_t env = 0;
        switch(currentSegment)
        {
            case ENV_SEGMENT_ATTACK:
                env = ((uint32_t)envPhase << 15) / attackTime;
                break;
            case ENV_SEGMENT_HOLD:
                env = 0x7fff;
            break;
            case ENV_SEGMENT_DECAY:
                phase = envPhase-attackTime-holdTime;
                env = ((uint32_t)phase << 15) / decayTime;
                env = sub_q15(0x7fff,env);
            break;
            case ENV_SEGMENT_COMPLETE:
                env = 0;
                break;
        }
        envPhase++;
        if(i==0)
        {
            svf.set_resonance(0);
            svf.set_frequency(env>>1);
        }
        if(enable_filter)
        {
            buffer[i] = svf.Process(buffer[i]);
        }
        if(enable_env)
        {
            buffer[i] = mult_q15(buffer[i], env);
        }
    }
}

void Instrument::SetOscillator(uint8_t oscillator)
{
    osc.set_shape((MacroOscillatorShape)oscillator);
}

const int keyToMidi[16] = {
    81, 83, 84, 86,
    74, 76, 77, 79,
    67, 69, 71, 72,
    60, 62, 64, 65
};

void Instrument::NoteOn(int16_t key)
{
    int note = keyToMidi[key];
    if(instrumentType == INSTRUMENT_MACRO)
    {
        enable_env = true;
        osc.set_pitch(note<<7);
        osc.Strike();
        envPhase = 0;
        currentSegment = ENV_SEGMENT_ATTACK;
    }
    else if(instrumentType == INSTRUMENT_DRUMS)
    {
        osc.Strike();
        currentSegment = ENV_SEGMENT_ATTACK;
        envPhase = 0;
        enable_env = false;
        enable_filter = false;
        switch(key)
        {
            case 0:
                SetOscillator(MACRO_OSC_SHAPE_KICK);
                osc.set_pitch(35<<7);
                break;
            case 1:
                SetOscillator(MACRO_OSC_SHAPE_KICK);
                osc.set_pitch(48<<7);
                break;
            case 2:
                SetOscillator(MACRO_OSC_SHAPE_SNARE);
                osc.set_pitch(55<<7);
                break;
            case 3:
                SetOscillator(MACRO_OSC_SHAPE_SNARE);
                osc.set_pitch(75<<7);
                break;
            case 4:
                SetOscillator(MACRO_OSC_SHAPE_CYMBAL);
                enable_env = true;
                SetAHD(10, 10, 4000);
                osc.set_pitch(65<<7);
                break;
            case 5:
                SetOscillator(MACRO_OSC_SHAPE_CYMBAL);
                enable_env = true;
                SetAHD(10, 2000, 8000);
                osc.set_pitch(63<<7);
                break;
        }
    }
    else
    {
        if(lastNoteOnPitch >= 0)
            midi->NoteOff(lastNoteOnPitch);
        midi->NoteOn(note);
    } 
    lastNoteOnPitch = note;
}


void Instrument::SetParameter(InstrumentParameter param, int8_t val)
{
    int16_t bVal = val;
    switch (param)
    {
        case INSTRUMENT_PARAM_MACRO_TIMBRE:
            osc.set_parameter_1(bVal << 7);
            break;
        case INSTRUMENT_PARAM_MACRO_MODULATION:
            osc.set_parameter_2(bVal << 7);
            break;
        default:
        break;
    }
}