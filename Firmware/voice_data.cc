#include "voice_data.h"

int8_t VoiceData::GetOctave()
{
    return ((int8_t)(octave/51))-2;
}

uint8_t& VoiceData::GetParam(uint8_t param, uint8_t lastNotePlayed)
{
    if(param == 28)
    {
        return delaySend;
    }
    if(GetInstrumentType() != INSTRUMENT_GLOBAL && param == 30)
    {
        return instrumentTypeBare;
    }
    
    if (GetInstrumentType() == INSTRUMENT_GLOBAL)
    {
        switch (param)
        {
            // 0
            case 0: return bpm;
            case 2: return amp_enabled;
            default:
                break;
        }
    }

    // shared instrument params
    switch(param)
    {
        case 2: return cutoff;
        case 3: return resonance;
        case 4: return volume;
        case 5: break;
        case 6: return octave;
        case 8: return attackTime;
        case 9: return decayTime;
    }
    if(GetInstrumentType() == INSTRUMENT_MACRO)
    {
        switch (param)
        {
            case 0: return timbre;
            case 1: return color;
            case 31: return shape;
            default:
                break;
        }
    }    
    if(GetInstrumentType() == INSTRUMENT_SAMPLE)
    {
        switch (param)
        {
            case 0: return sampleStart[lastNotePlayed];
            case 1: return sampleLength[lastNotePlayed];
            default:
                break;
        }
    }    
    return nothing;
}

const char *voice_macroparams[16] = { 
    "timb", "filt", "vlpn", "ptch",
    "env", "?", "?", "?",
    "?", "?", "?", "?",
    "?", "?", "FLTO", "TYPE"
};
const char *voice_sampleparams[16] = { 
    "I/O", "FILT", "VlPn", "Ptch",
    "Env", "EnvF", "EnvP", "EnvT",
    "lop", "?", "?", "?",
    "?", "?", "FLTO", "TYPE"
};

void VoiceData::GetParamString(uint8_t param, char *str, uint8_t lastNotePlayed)
{
    int16_t vala = 0;
    int16_t valb = 0;

    if(param == 14)
    {
        sprintf(str, "FX %i", delaySend);
        return;
    }

    if(GetInstrumentType() == INSTRUMENT_GLOBAL)
    {
        switch (param)
        {
            case 0:
                sprintf(str, "bpm %i", bpm);
                return;
                break;
            case 1:
                sprintf(str, amp_enabled>0x7f?"speaker on":"speaker off");
                return;
                break;
        }
    }
    if(GetInstrumentType() == INSTRUMENT_MIDI)
    {
        switch (param)
        {
            case 15:
                sprintf(str, "TYPE MIDI");
                return;
                break;
        }
    }
    if(GetInstrumentType() == INSTRUMENT_DRUMS)
    {
        switch (param)
        {
            case 15:
                sprintf(str, "TYPE DRUMS");
                return;
                break;
        }
    }
    if(GetInstrumentType() == INSTRUMENT_SAMPLE)
    {
        switch (param)
        {
        // sample in point
        case 0:
            vala = sampleStart[lastNotePlayed];
            valb = sampleLength[lastNotePlayed];
            break;

        // volume / pan
        case 1:
            vala = cutoff;
            valb = resonance;
            break;
        // volume / pan
        case 2:
            vala = volume;
            valb = 0x7f;
            break;
        case 3:
            vala = GetOctave();
            valb = 0x7f;
            break;
        case 4:
            vala = attackTime;
            valb = decayTime;
            break;
        case 8:
            // sprintf(str, "LOOP_MODE %i", loopMode);
            return;
            break;
        // sample out point
        case 15:
            sprintf(str, "TYPE SAMP");
            return;
            break;

        default:
            break;
        }
        sprintf(str, "%s %i %i", voice_sampleparams[param], vala, valb);
    }
    if(GetInstrumentType() == INSTRUMENT_MACRO)
    {
        switch (param)
        {
            // 0
            case 0:
                vala = timbre;
                valb = color;
                break;
            case 1:
                vala = cutoff;
                valb = resonance;
                break;
            // volume / pan
            case 2:
                vala = volume;
                valb = 0x7f;
                break;
            case 3:
                vala = GetOctave();
                valb = 0x7f;
                break;
            case 4:
                vala = attackTime;
                valb = decayTime;
                break;
            case 15:
                sprintf(str, "TYPE SYNTH %s", algo_values[GetShape()]);
                return;
                break;
            default:
                break;
        }
        sprintf(str, "%s %i %i", voice_macroparams[param], vala, valb);
    }
}
