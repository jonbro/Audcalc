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
    if(GetInstrumentType() == INSTRUMENT_MIDI)
    {
        switch(param)
        {
            case Timbre: return HasLockForStep(step, pattern, Timbre, value)?value:timbre;
            case PlayingMidiNotes: return HasLockForStep(step, pattern, PlayingMidiNotes, value)?value:color;
        }
    }
    if(GetInstrumentType() == INSTRUMENT_SAMPLE)
    {
        switch(param)
        {
            case SampleIn: return HasLockForStep(step, pattern, SampleIn, value)?value:sampleStart[lastNotePlayed];
            case SampleOut: return HasLockForStep(step, pattern, SampleOut, value)?value:sampleLength[lastNotePlayed];
            case AttackTime: return HasLockForStep(step, pattern, AttackTime, value)?value:sampleAttackTime;
            case DecayTime: return HasLockForStep(step, pattern, DecayTime, value)?value:sampleDecayTime;
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
        case AttackTime2: return HasLockForStep(step, pattern, AttackTime2, value)?value:attackTime2;
        case DecayTime2: return HasLockForStep(step, pattern, DecayTime2, value)?value:decayTime2;
        case Env1Target: return HasLockForStep(step, pattern, Env1Target, value)?value:env1Target;
        case Env1Depth: return HasLockForStep(step, pattern, Env1Depth, value)?value:env1Depth;
        case Env2Target: return HasLockForStep(step, pattern, Env2Target, value)?value:env2Target;
        case Env2Depth: return HasLockForStep(step, pattern, Env2Depth, value)?value:env2Depth;
        case LFORate: return HasLockForStep(step, pattern, LFORate, value)?value:lfoRate;
        case LFODepth: return HasLockForStep(step, pattern, LFODepth, value)?value:lfoDepth;
        case Length: return length[pattern];
        case DelaySend: return HasLockForStep(step, pattern, DelaySend, value)?value:delaySend;
        case ReverbSend: return HasLockForStep(step, pattern, ReverbSend, value)?value:reverbSend;
    }
    return 0;
}

// used for setting the value in place
uint8_t& VoiceData::GetParam(uint8_t param, uint8_t lastNotePlayed, uint8_t currentPattern)
{
    if(param == 32)
    {
        return globalVoiceData->bpm;
    }
    if(param == 28)
    {
        return effectEditTarget;
    }
    if(param == 29)
    {
        switch((effectEditTarget-1)/127)
        {
            case 0:
                return delaySend;
            default:
                return reverbSend;
        }
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
            case 4: return chromatic;
            case 8: return syncIn;
            case 9: return syncOut;
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
        case 10: return attackTime2;
        case 11: return decayTime2;
        case 12: return lfoRate;
        case 13: return lfoDepth;
        case 16: return env1Target;
        case 17: return env1Depth;
        case 18: return env2Target;
        case 19: return env2Depth;
        case 24: return length[currentPattern];
        case 25: return rate[currentPattern];
    }
    if(GetInstrumentType() == INSTRUMENT_MACRO)
    {
        switch (param)
        {
            case 0: return timbre;
            case 1: return color;
            case 8: return attackTime;
            case 9: return decayTime;
            case 31: return shape;
            default:
                break;
        }
    }    
    if(GetInstrumentType() == INSTRUMENT_MIDI)
    {
        switch (param)
        {
            case 0: return timbre;
            case 1: return color;
            case 31: return midiChannel;
            default:
                break;
        }
    }    
    if(GetInstrumentType() == INSTRUMENT_SAMPLE)
    {
        switch (param)
        {
            case 0: return sampleStart[GetSampler()!=SAMPLE_PLAYER_SLICE?0:lastNotePlayed];
            case 1: return sampleLength[GetSampler()!=SAMPLE_PLAYER_SLICE?0:lastNotePlayed];
            case 8: return sampleAttackTime;
            case 9: return sampleDecayTime;
            case 31: return samplerTypeBare;
            default:
                break;
        }
    }    
    return nothing;
}

const char *rates[7] = { 
    "2x",
    "3/2x",
    "1x",
    "3/4x",
    "1/2x",
    "1/4x",
    "1/8x"
};

const char *syncInStrings[4] = { 
    "none",
    "midi",
    "PO",
    "VL",
};

const char *syncOutStrings[6] = { 
    "none",
    "midi",
    "m+PO",
    "m+24",
    "PO",
    "24pq"
};


const char *envTargets[5] = { 
    "Vol",
    "Timb",
    "Col",
    "Cut",
    "Res",
};

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
        case 14:
            sprintf(strA, "FX");
            sprintf(strB, "Snd");
            switch((effectEditTarget-1)/127)
            {
                case 0:
                    sprintf(pA, "Dely");
                    lockB = CheckLockAndSetDisplay(step, pattern, DelaySend, delaySend, pB);
                    break;
                default:
                    sprintf(pA, "Verb");
                    lockB = CheckLockAndSetDisplay(step, pattern, ReverbSend, reverbSend, pB);
                    break;
            }
            return;
        case 16:
            sprintf(strA, "Bpm");
            sprintf(strB, "");
            sprintf(pA, "%i", (globalVoiceData->bpm+1));
            sprintf(pB, "");
            return;
    }
    if(instrumentType == INSTRUMENT_GLOBAL)
    {
        switch (param)
        {
            case 0:
                sprintf(strA, "Bpm");
                sprintf(strB, "");
                sprintf(pA, "%i", (bpm+1));
                sprintf(pB, "");
                return;
            case 1:
                sprintf(strA, "Spkr");
                sprintf(strB, "");
                sprintf(pA, amp_enabled>0x7f?"On":"Off");
                sprintf(pB, "");
                return;
            case 2:
                sprintf(strA, "chrom");
                sprintf(strB, "");
                sprintf(pA, chromatic>0x7f?"On":"Off");
                sprintf(pB, "");
                return;
            case 4:
                sprintf(strA, "SIn");
                sprintf(strB, "SOut");
                sprintf(pA,syncInStrings[(syncIn*0)>>8]); // todo: make this allow for more than "None"
                sprintf(pB,syncOutStrings[(syncOut*6)>>8]);
                return;
            case 12:
                sprintf(strA, "Chng");
                sprintf(strB, "Swng");
                sprintf(pA, "%i", length[pattern]/4+1);
                sprintf(pB,"");
                return;
            default:
                sprintf(strA, "");
                sprintf(strB, "");
                sprintf(pA, "");
                sprintf(pB, "");
                return;
        }
    }
    
    // all non global instruments
    switch (param)
    {
        case 12:
            sprintf(strA, "Len");
            sprintf(strB, "Rate");
            sprintf(pA, "%i", length[pattern]/4+1);
            sprintf(pB,rates[(rate[pattern]*7)>>8]);
            return;
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
            case 5:
                sprintf(strA, "Atk");
                sprintf(strB, "Dcy");
                lockA = CheckLockAndSetDisplay(step, pattern, 10, attackTime2, pA);
                lockB = CheckLockAndSetDisplay(step, pattern, 11, decayTime2, pB);
                return;
            case 6:
                sprintf(strA, "Rate");
                sprintf(strB, "Dpth");
                lockA = CheckLockAndSetDisplay(step, pattern, 12, lfoRate, pA);
                lockB = CheckLockAndSetDisplay(step, pattern, 13, lfoDepth, pB);
                return;
            case 8:
                sprintf(strA, "Trgt");
                sprintf(strB, "Dpth");
                if(HasLockForStep(step, pattern, 16, valB))
                {
                    sprintf(pA, "%s", envTargets[(((uint16_t)valB)*5) >> 8]);
                    lockA = true;
                }
                else
                    sprintf(pA, "%s", envTargets[(((uint16_t)env1Target)*5)>>8]);
                lockB = CheckLockAndSetDisplay(step, pattern, 17, env1Depth, pB);
                return;
            case 9:
                sprintf(strA, "Trgt");
                sprintf(strB, "Dpth");
                if(HasLockForStep(step, pattern, 18, valB))
                {
                    sprintf(pA, "%s", envTargets[(((uint16_t)valB)*5) >> 8]);
                    lockA = true;
                }
                else
                    sprintf(pA, "%s", envTargets[(((uint16_t)env2Target)*5)>>8]);
                lockB = CheckLockAndSetDisplay(step, pattern, 19, env2Depth, pB);
                return;
            case 12:
                sprintf(strA, "Len");
                sprintf(strB, "Rate");
                sprintf(pA, "%i", length[pattern]/4+1);
                sprintf(pB,rates[(rate[pattern]*7)>>8]);
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
                if(GetSampler() == SAMPLE_PLAYER_SLICE)
                {
                    sprintf(pA, "%i", sampleStart[lastNotePlayed]);
                    sprintf(pB, "%i", sampleLength[lastNotePlayed]);
                }
                else
                {
                    sprintf(pA, "%i", sampleStart[0]);
                    sprintf(pB, "%i", sampleLength[0]);
                }
                return;
            case 4:
                sprintf(strA, "Atk");
                sprintf(strB, "Dcy");
                lockA = CheckLockAndSetDisplay(step, pattern, 8, sampleAttackTime, pA);
                lockB = CheckLockAndSetDisplay(step, pattern, 9, sampleDecayTime, pB);
                return;
            case 15:
                sprintf(strA, "Type");
                sprintf(strB, "");
                sprintf(pA, "Samp");
                switch(GetSampler())
                {
                    case 0:
                        sprintf(pB, "Slice");
                        break;
                    case 1:
                        sprintf(pB, "Pitch");
                        break;
                    default:
                        sprintf(pB, "S-Eql");
                }
                return;
            default:
                return;
        }
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
            case 4:
                sprintf(strA, "Atk");
                sprintf(strB, "Dcy");
                lockA = CheckLockAndSetDisplay(step, pattern, 8, attackTime, pA);
                lockB = CheckLockAndSetDisplay(step, pattern, 9, decayTime, pB);
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
    }
    if(GetInstrumentType() == INSTRUMENT_MIDI)
    {
        switch (param)
        {
            case 0:
                sprintf(strA, "Vel");
                sprintf(strB, "NNte");
                lockA = CheckLockAndSetDisplay(step, pattern, 0, timbre, pA);
                lockB = CheckLockAndSetDisplay(step, pattern, 1, color, pB);
                return;
            // 0
            case 15:
                sprintf(strA, "Type");
                sprintf(strB, "");
                sprintf(pA, "Midi");
                sprintf(pB, "%i", (midiChannel>>4)+1);
                return;
            default:
                return;
        }
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


/* PARAMETER LOCK BEHAVIOR */
void VoiceData::StoreParamLock(uint8_t param, uint8_t step, uint8_t pattern, uint8_t value)
{
    ParamLock *lock = GetLockForStep(0x80|step, pattern, param);
    if(lock != ParamLockPool::NullLock())
    {
        lock->value = value;
        // printf("updated param lock step: %i param: %i value: %i\n", step, param, value);
        return;
    }
    if(lockPool.GetFreeParamLock(&lock))
    {
        if(lock == ParamLockPool::NullLock() || !lockPool.validLock(lock))
        {
            // printf("out of lock space\n failed to add new lock");
            return;
        }
        // printf("new address %x | null lock addy %x\n", lock, ParamLockPool::NullLock());
        lock->param = param;
        lock->step = step;
        lock->value = value;
        lock->next = lockPool.GetLockPosition(locksForPattern[pattern]);
        locksForPattern[pattern] = lock;
        // printf("added param lock step: %i param: %i value: %i at lock position: %i\n", step, param, value, lockPool.GetLockPosition(lock));
        return;
    }
    // printf("failed to add param lock\n");
}
void VoiceData::ClearParameterLocks(uint8_t pattern)
{
    ParamLock* lock = locksForPattern[pattern];
    while(lock != ParamLockPool::NullLock())
    {
        ParamLock* nextLock = lockPool.GetLock(lock->next);
        lockPool.FreeLock(lock);
        lock = nextLock;
    }
    locksForPattern[pattern] = ParamLockPool::NullLock();
}
void VoiceData::RemoveLocksForStep(uint8_t pattern, uint8_t step)
{
    ParamLock* lock = locksForPattern[pattern];
    ParamLock* lastLock = lock;
    while(lock != ParamLockPool::NullLock())
    {
        ParamLock* nextLock = lockPool.GetLock(lock->next);
        if(lock->step == step)
        {
            lastLock->next = lock->next;
            lockPool.FreeLock(lock);
            if(lock == locksForPattern[pattern])
            {
                locksForPattern[pattern] = nextLock;
            }
        }
        else
        {
            lastLock = lock;
        }
        lock = nextLock;
    }
}
void VoiceData::CopyParameterLocks(uint8_t fromPattern, uint8_t toPattern)
{
    ParamLock* lock = locksForPattern[fromPattern];
    while(lock != ParamLockPool::NullLock())
    {
        StoreParamLock(lock->param, lock->step, toPattern, lock->value);
        lock = lockPool.GetLock(lock->next);
    }
}
bool VoiceData::HasLockForStep(uint8_t step, uint8_t pattern, uint8_t param, uint8_t &value)
{
    if(step>>7==0)
        return false;
    ParamLock* lock = GetLockForStep(step, pattern, param);
    if(lock != ParamLockPool::NullLock())
    {
        value = lock->value;
        return true;
    }
    return false;
}
bool VoiceData::HasAnyLockForStep(uint8_t step, uint8_t pattern)
{
    if(step>>7==0)
        return false;
    ParamLock* lock = locksForPattern[pattern];
    while(lock != ParamLockPool::NullLock() && lockPool.validLock(lock))
    {
        if(lock->step == (step&0x7f))
        {
            return true;
        }
        lock = lockPool.GetLock(lock->next);
    }
    return false;
}
ParamLock* VoiceData::GetLockForStep(uint8_t step, uint8_t pattern, uint8_t param)
{
    if(step>>7==0)
        return ParamLockPool::NullLock();
    ParamLock* lock = locksForPattern[pattern];
    while(lock != ParamLockPool::NullLock() && lockPool.validLock(lock))
    {
        if(lock->param == param && lock->step == (step&0x7f))
        {
            return lock;
        }
        lock = lockPool.GetLock(lock->next);
    }
    return ParamLockPool::NullLock();
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