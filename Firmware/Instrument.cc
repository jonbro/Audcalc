#include "Instrument.h"

void Instrument::Init()
{
    osc.Init();
    currentSegment = ENV_SEGMENT_COMPLETE;
    osc.set_pitch(0);
    osc.set_shape(MACRO_OSC_SHAPE_WAVE_PARAPHONIC);
    //osc.set_shape(MACRO_OSC_SHAPE_KICK);
    env = 0;
    envPhase = 0;
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
        q15_t tenv = 0;
        switch(currentSegment)
        {
            case ENV_SEGMENT_ATTACK:
                tenv = ((uint32_t)envPhase << 15) / attackTime;
                break;
            case ENV_SEGMENT_HOLD:
                tenv = 0x7fff;
            break;
            case ENV_SEGMENT_DECAY:
                phase = envPhase-attackTime-holdTime;
                tenv = ((uint32_t)phase << 15) / decayTime;
                tenv = sub_q15(0x7fff,tenv);
            break;
            case ENV_SEGMENT_COMPLETE:
                tenv = 0;
                break;
        }
        envPhase++;
        env = tenv;//lerp_q15(tenv, env, 0x7aaa);
        buffer[i] = mult_q15(buffer[i], env);
    }
}

void Instrument::SetOscillator(uint8_t oscillator)
{
    osc.set_shape((MacroOscillatorShape)oscillator);
}

void Instrument::Strike()
{
    osc.Strike();
    envPhase = 0;
    currentSegment = ENV_SEGMENT_ATTACK;
}

void Instrument::SetPitch(int16_t pitch)
{
    osc.set_pitch(pitch);
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