#include "voice_data.h"
#include "m6x118pt7b.h"

ParamLockPool VoiceData::lockPool;

int8_t VoiceData::GetOctave()
{
    return ((int8_t)(octave/51))-2;
}

// incorporates the lock if any
uint8_t VoiceData::GetParamValue(ParamType param, uint8_t lastNotePlayed, uint8_t step, uint8_t pattern)
{
    uint8_t value;
    // instrument special case
    if(GetInstrumentType() == INSTRUMENT_MACRO)
    {
        switch(param)
        {
            case Timbre: return HasLockForStep(step, pattern, Timbre, value)?value:timbre;
            case Color: return HasLockForStep(step, pattern, Color, value)?value:color;
        }
    }
    if(GetInstrumentType() == INSTRUMENT_MACRO)
    {
        switch(param)
        {
            case SampleIn: return HasLockForStep(step, pattern, SampleIn, value)?value:sampleStart[lastNotePlayed];
            case SampleOut: return HasLockForStep(step, pattern, SampleOut, value)?value:sampleLength[lastNotePlayed];
        }
    }
    switch(param)
    {
        case Cutoff: return HasLockForStep(step, pattern, Cutoff, value)?value:cutoff;
        case Resonance: return HasLockForStep(step, pattern, Resonance, value)?value:resonance;
        case Volume: return HasLockForStep(step, pattern, Volume, value)?value:volume;
        case Octave: return HasLockForStep(step, pattern, Octave, value)?value:octave;
        case AttackTime: return HasLockForStep(step, pattern, AttackTime, value)?value:attackTime;
        case DecayTime: return HasLockForStep(step, pattern, DecayTime, value)?value:decayTime;
        case Length: return length[pattern];
        case DelaySend: return HasLockForStep(step, pattern, DelaySend, value)?value:delaySend;
    }
    return 0;
}

// used for setting the value in place
uint8_t& VoiceData::GetParam(uint8_t param, uint8_t lastNotePlayed, uint8_t currentPattern)
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
        case 24: return length[currentPattern];
        case 25: return rate[currentPattern];
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
    "env ", "?", "?", "?",
    "?", "?", "?", "?",
    "?", "?", "FLTO", "TYPE"
};
const char *voice_sampleparams[16] = { 
    "I/O", "FILT", "VlPn", "Ptch",
    "Env", "EnvF", "EnvP", "EnvT",
    "lop", "?", "?", "?",
    "?", "?", "FLTO", "TYPE"
};

const char *rates[7] = { 
    "1/8",
    "1/4",
    "1/2",
    "3/4",
    "1",
    "3/2",
    "2"
};

void VoiceData::GetParamString(uint8_t param, char *str, uint8_t lastNotePlayed, uint8_t currentPattern)
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
    // shared non global params
    bool needsReturn = false;
    switch(param)
    {
        case 4:
            vala = attackTime;
            valb = decayTime;
            needsReturn = true;
            break;
        case 12:
            vala = length[currentPattern]/4+1;
            valb = rate[currentPattern];
            sprintf(str, "len %i", vala);//, valb); need to implement rate
            return;
    }
    if(needsReturn)
    {
        sprintf(str, "%s %i %i", voice_macroparams[param], vala, valb);
        return;
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

bool VoiceData::CheckLockAndSetDisplay(uint8_t step, uint8_t pattern, uint8_t param, uint8_t value, char *paramString)
{
    uint8_t valA = 0;
    if(HasLockForStep(step, pattern, param, valA))
    {
        sprintf(paramString, "%i", valA);
        return true;
    }
    sprintf(paramString, "%i", value);
    return false;
}

void VoiceData::GetParamsAndLocks(uint8_t param, uint8_t step, uint8_t pattern, char *strA, char *strB, uint8_t lastNotePlayed, char *pA, char *pB, bool &lockA, bool &lockB)
{
    uint8_t valA = 0, valB = 0;
    InstrumentType instrumentType = GetInstrumentType();
    
    switch(param)
    {
        case 12:
            sprintf(strA, "Len");
            sprintf(strB, "");
            sprintf(pA, "%i", length[pattern]/4+1);
            sprintf(pB, "");
            // valb = rate[currentPattern];
            // sprintf(str, "len %i", vala);//, valb); need to implement rate
            return;
        case 14:
            sprintf(strA, "Dely");
            sprintf(strB, "");
            lockA = CheckLockAndSetDisplay(step, pattern, 28, delaySend, pA);
            sprintf(pB, "");
            // valb = rate[currentPattern];
            // sprintf(str, "len %i", vala);//, valb); need to implement rate
            return;
    }
    if(instrumentType == INSTRUMENT_GLOBAL)
    {
        switch (param)
        {
            case 0:
                sprintf(strA, "Bpm");
                sprintf(strB, "");
                sprintf(pA, "%i", bpm);
                sprintf(pB, "");
                return;
            case 1:
                sprintf(strA, "Spkr");
                sprintf(strB, "");
                sprintf(pA, amp_enabled?"On":"Off");
                sprintf(pB, "");
                return;
            default:
                sprintf(strA, "");
                sprintf(strB, "");
                sprintf(pA, "");
                sprintf(pB, "");
                return;
        }
    }

    if(instrumentType == INSTRUMENT_MACRO || instrumentType == INSTRUMENT_SAMPLE)
    {
        switch (param)
        {
            case 1:
                sprintf(strA, "Cut");
                sprintf(strB, "Res");
                lockA = CheckLockAndSetDisplay(step, pattern, 2, cutoff, pA);
                lockB = CheckLockAndSetDisplay(step, pattern, 3, resonance, pB);
                return;
            // volume / pan
            case 2:
                sprintf(strA, "Volm");
                sprintf(strB, "");
                lockA = CheckLockAndSetDisplay(step, pattern, 4, volume, pA);
                sprintf(pB, "");
                return;
            case 3:
                sprintf(strA, "Octv");
                sprintf(strB, "");
                sprintf(pA, "%i", GetOctave());
                sprintf(pB, "");
                return;
            case 4:
                sprintf(strA, "Atk");
                sprintf(strB, "Dcy");
                lockA = CheckLockAndSetDisplay(step, pattern, 8, attackTime, pA);
                lockB = CheckLockAndSetDisplay(step, pattern, 9, decayTime, pB);
                return;
        }
    }
    if(GetInstrumentType() == INSTRUMENT_SAMPLE)
    {
        switch (param)
        {
            // 0
            case 0:
                sprintf(strA, "In");
                sprintf(strB, "Len");
                sprintf(pA, "%i", sampleStart[lastNotePlayed]);
                sprintf(pB, "%i", sampleLength[lastNotePlayed]);
                return;
            case 15:
                sprintf(strA, "Type");
                sprintf(strB, "");
                sprintf(pA, "Samp");
                sprintf(pB, "");
                return;
            default:
                return;
        }
        // sprintf(str, "%s", voice_macroparams[param]);
    }

    if(GetInstrumentType() == INSTRUMENT_MACRO)
    {
        switch (param)
        {
            // 0
            case 0:
                sprintf(strA, "Timb");
                sprintf(strB, "Colr");
                lockA = CheckLockAndSetDisplay(step, pattern, 0, timbre, pA);
                lockB = CheckLockAndSetDisplay(step, pattern, 1, color, pB);
                return;
            case 15:
                sprintf(strA, "Type");
                sprintf(strB, "");
                sprintf(pA, "Synt");
                if(HasLockForStep(step, pattern, 31, valB))
                {
                    sprintf(pB, "%s", algo_values[(MacroOscillatorShape)((((uint16_t)valB)*41) >> 8)]);
                    lockB = true;
                }
                else
                    sprintf(pB, "%s", algo_values[GetShape()]);
                return;
            default:
                return;
        }
        // sprintf(str, "%s", voice_macroparams[param]);
    }
}

void VoiceData::DrawParamString(uint8_t param, char *str, uint8_t lastNotePlayed, uint8_t currentPattern, uint8_t paramLock)
{
    ssd1306_t* disp = GetDisplay();
    uint8_t width = 36;
    uint8_t column4 = 128-width;
    bool lockA = false, lockB = false;
    GetParamsAndLocks(param, paramLock, currentPattern, str, str+16, lastNotePlayed, str+32, str+48, lockA, lockB);
    if(lockA)
        ssd1306_draw_square_rounded(disp, column4, 0, width, 15);
    if(lockB)
        ssd1306_draw_square_rounded(disp, column4, 17, width, 15);
    ssd1306_draw_string_gfxfont(disp, column4+3, 12, str+32, !lockA, 1, 1, &m6x118pt7b);
    ssd1306_draw_string_gfxfont(disp, column4+3, 17+12, str+48, !lockB, 1, 1, &m6x118pt7b);
    
    ssd1306_draw_string_gfxfont(disp, column4-33, 12, str, true, 1, 1, &m6x118pt7b);    
    ssd1306_draw_string_gfxfont(disp, column4-33, 17+12, str+16, true, 1, 1, &m6x118pt7b);
}

void VoiceData::SerializeStatic(Serializer &s)
{
    uint8_t *lockPoolReader = (uint8_t*)&lockPool;
    for (size_t i = 0; i < sizeof(ParamLockPool); i++)
    {
        s.AddData(*lockPoolReader);
        lockPoolReader++;
    }
}

void VoiceData::DeserializeStatic(Serializer &s)
{
    uint8_t *lockPoolReader = (uint8_t*)&lockPool;
    for (size_t i = 0; i < sizeof(ParamLockPool); i++)
    {
        *lockPoolReader = s.GetNextValue();
        lockPoolReader++;
    }
}


void TestVoiceData()
{
    VoiceData voiceData;
    voiceData.StoreParamLock(1, 1, 1, 5);
    uint8_t lockValue;
    bool hasLock = voiceData.HasLockForStep(0x80|1, 1, 1, lockValue);
    printf("%i, %i\n", hasLock, lockValue);
    voiceData.StoreParamLock(1, 1, 1, 127);
    hasLock = voiceData.HasLockForStep(0x80|1, 1, 1, lockValue);
    printf("%i, %i\n", hasLock, lockValue);
}