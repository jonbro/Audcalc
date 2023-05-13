#include "voice_data.h"
#include "m6x118pt7b.h"

ParamLockPool VoiceData::lockPool;

void VoiceData::InitDefaults()
{
    for (size_t i = 0; i < 16; i++)
    {
        locksForPattern[i] = ParamLockPool::InvalidLockPosition();
        internalData.patterns[i].rate = 2*37; // 1x 
        internalData.patterns[i].length = 15*4; // need to up this to fit into 0xff
    }
    internalData.env1.attack = 0x10;
    internalData.env1.decay = 0x20;
    internalData.env1.depth = 0x7f;

    internalData.env2.attack = 0x10;
    internalData.env2.decay = 0x20;
    internalData.env2.depth = 0x7f;

    internalData.sampleAttack = 0x0;
    internalData.sampleDecay = 0xff;
    
    internalData.color = 0x7f;
    internalData.timbre = 0x7f;

    internalData.cutoff = 0xff;
    internalData.resonance = 0;
    internalData.volume = 0x7f;
    internalData.pan = 0x7f;
    
    internalData.retriggerSpeed = 0;
    internalData.retriggerLength = 0;
    internalData.retriggerFade = 0x7f;
    internalData.octave = 0x7f;
}

bool VoiceDataInternal_encode_locks(pb_ostream_t *ostream, const pb_field_t *field, void * const *arg)
{
    uint16_t* locksForPattern = *(uint16_t**)arg;

    // encode all pattern lock pointers
    for (int i = 0; i < 16; i++)
    {
        // skip locks that are pointing at the null lock
        if(locksForPattern[i] == ParamLockPool::InvalidLockPosition())
            continue;
        if (!pb_encode_tag_for_field(ostream, field))
        {
            const char * error = PB_GET_ERROR(ostream);
            return false;
        }
        VoiceDataInternal_LockPointer lock = VoiceDataInternal_LockPointer_init_zero;
        lock.pattern = i;
        lock.pointer = locksForPattern[i];
        if (!pb_encode_submessage(ostream, VoiceDataInternal_LockPointer_fields, &lock))
        {
            const char * error = PB_GET_ERROR(ostream);
            printf("VoiceDataInternal_encode_locks error: %s", error);
            return false;
        }
    }
    return true;
}
void VoiceData::Serialize(pb_ostream_t *s)
{
    s->bytes_written = 0;
    internalData.has_env1 = true;
    internalData.has_env2 = true;
    internalData.which_extraTypeUnion = VoiceDataInternal_synthShape_tag;
    internalData.locksForPattern.funcs.encode = &VoiceDataInternal_encode_locks;
    internalData.locksForPattern.arg = locksForPattern;
    pb_encode_ex(s, VoiceDataInternal_fields, &internalData, PB_ENCODE_DELIMITED);
}
bool VoiceDataInternal_decode_locks(pb_istream_t *stream, const pb_field_iter_t *field, void **arg)
{
    uint16_t* locksForPattern = *(uint16_t**)arg;
    VoiceDataInternal_LockPointer lock = VoiceDataInternal_LockPointer_init_zero;
    if (!pb_decode(stream, VoiceDataInternal_LockPointer_fields, &lock))
        return false;
    locksForPattern[lock.pattern] = lock.pointer;
    return true;
}


void VoiceData::Deserialize(pb_istream_t *s)
{
    internalData.locksForPattern.funcs.decode = &VoiceDataInternal_decode_locks;
    internalData.locksForPattern.arg = locksForPattern;
    if(!pb_decode_ex(s, VoiceDataInternal_fields, &internalData, PB_DECODE_DELIMITED))
    {
        const char * error = PB_GET_ERROR(s);
        printf("VoiceData deserialize error: %s\n", error);
    }
}
void VoiceData::SerializeStatic(pb_ostream_t *s)
{
    lockPool.Serialize(s);
}

void VoiceData::DeserializeStatic(pb_istream_t *s)
{
    lockPool.Deserialize(s);
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
            case Timbre: return HasLockForStep(step, pattern, Timbre, value)?value:internalData.timbre;
            case Color: return HasLockForStep(step, pattern, Color, value)?value:internalData.color;
        }
    }
    if(GetInstrumentType() == INSTRUMENT_MIDI)
    {
        switch(param)
        {
            case Timbre: return HasLockForStep(step, pattern, Timbre, value)?value:internalData.timbre;
            case PlayingMidiNotes: return HasLockForStep(step, pattern, PlayingMidiNotes, value)?value:internalData.color;
        }
    }
    if(GetInstrumentType() == INSTRUMENT_SAMPLE)
    {
        switch(param)
        {
            case SampleIn: return HasLockForStep(step, pattern, SampleIn, value)?value:internalData.sampleStart[lastNotePlayed];
            case SampleOut: return HasLockForStep(step, pattern, SampleOut, value)?value:internalData.sampleLength[lastNotePlayed];
            case AttackTime: return HasLockForStep(step, pattern, AttackTime, value)?value:internalData.sampleAttack;
            case DecayTime: return HasLockForStep(step, pattern, DecayTime, value)?value:internalData.sampleDecay;
        }
    }
    switch(param)
    {
        case Cutoff: return HasLockForStep(step, pattern, Cutoff, value)?value:internalData.cutoff;
        case Resonance: return HasLockForStep(step, pattern, Resonance, value)?value:internalData.resonance;
        case Volume: return HasLockForStep(step, pattern, Volume, value)?value:internalData.volume;
        case Pan: return HasLockForStep(step, pattern, Pan, value)?value:internalData.pan;
        case AttackTime: return HasLockForStep(step, pattern, AttackTime, value)?value:internalData.env1.attack;
        case DecayTime: return HasLockForStep(step, pattern, DecayTime, value)?value:internalData.env1.decay;
        case Env1Target: return HasLockForStep(step, pattern, Env1Target, value)?value:internalData.env1.target;
        case Env1Depth: return HasLockForStep(step, pattern, Env1Depth, value)?value:internalData.env1.depth;
        case AttackTime2: return HasLockForStep(step, pattern, AttackTime2, value)?value:internalData.env2.attack;
        case DecayTime2: return HasLockForStep(step, pattern, DecayTime2, value)?value:internalData.env2.decay;
        case Env2Target: return HasLockForStep(step, pattern, Env2Target, value)?value:internalData.env2.target;
        case Env2Depth: return HasLockForStep(step, pattern, Env2Depth, value)?value:internalData.env2.depth;
        case LFORate: return HasLockForStep(step, pattern, LFORate, value)?value:internalData.lfoRate;
        case LFODepth: return HasLockForStep(step, pattern, LFODepth, value)?value:internalData.lfoDepth;
        case Lfo1Target: return HasLockForStep(step, pattern, Lfo1Target, value)?value:internalData.lfoTarget;
        case RetriggerSpeed: return HasLockForStep(step, pattern, RetriggerSpeed, value)?value:internalData.retriggerSpeed;
        case RetriggerLength: return HasLockForStep(step, pattern, RetriggerLength, value)?value:internalData.retriggerLength;
        case RetriggerFade: return HasLockForStep(step, pattern, RetriggerFade, value)?value:internalData.retriggerFade;
        case Length: return internalData.patterns[pattern].length;
        case DelaySend: return HasLockForStep(step, pattern, DelaySend, value)?value:internalData.delaySend;
        case ReverbSend: return HasLockForStep(step, pattern, ReverbSend, value)?value:internalData.reverbSend;
        case ConditionMode: return HasLockForStep(step, pattern, ConditionMode, value)?value:internalData.conditionMode;
        case ConditionData: return HasLockForStep(step, pattern, ConditionData, value)?value:internalData.conditionData;
    }
    return 0;
}

// used for setting the value in place
// currentPattern is used for alterning things that have per pattern values (pattern length)
// last n
uint8_t& VoiceData::GetParam(uint8_t param, uint8_t lastNotePlayed, uint8_t currentPattern)
{
    if(param == 28)
    {
        return internalData.delaySend;
    }
    if(param == 29)
    {
        return internalData.reverbSend;
    }
    if(GetInstrumentType() != INSTRUMENT_GLOBAL && param == 30)
    {
        return internalData.instrumentType;
    }
    if(param == 48)
    {
        return internalData.octave;
    }
    // shared instrument params
    switch(param)
    {
        case 2: return internalData.cutoff;
        case 3: return internalData.resonance;
        case 4: return internalData.volume;
        case 5: return internalData.pan;
        case 10: return internalData.env2.attack;
        case 11: return internalData.env2.decay;
        case 12: return internalData.lfoRate;
        case 13: return internalData.lfoDepth;
        case 14: return internalData.retriggerSpeed;
        case 15: return internalData.retriggerLength;
        case 16: return internalData.env1.target;
        case 17: return internalData.env1.depth;
        case 18: return internalData.env2.target;
        case 19: return internalData.env2.depth;
        case 20: return internalData.lfoTarget;
        case 21: return internalData.lfoDelay;
        case 22: return internalData.retriggerFade;
        case 24: return internalData.patterns[currentPattern].length;
        case 25: return internalData.patterns[currentPattern].rate;
        case 26: return internalData.conditionMode;
        case 27: return internalData.conditionData;
    }
    if(GetInstrumentType() == INSTRUMENT_MACRO)
    {
        switch (param)
        {
            case 0: return internalData.timbre;
            case 1: return internalData.color;
            case 8: return internalData.env1.attack;
            case 9: return internalData.env1.decay;
            case 31: return internalData.extraTypeUnion.synthShape;
            default:
                break;
        }
    }    
    if(GetInstrumentType() == INSTRUMENT_MIDI)
    {
        switch (param)
        {
            case 0: return internalData.timbre;
            case 1: return internalData.color;
            case 31: return internalData.extraTypeUnion.midiChannel;
            default:
                break;
        }
    }    
    if(GetInstrumentType() == INSTRUMENT_SAMPLE)
    {
        switch (param)
        {
            case 0: return internalData.sampleStart[GetSampler()!=SAMPLE_PLAYER_SLICE?0:lastNotePlayed];
            case 1: return internalData.sampleLength[GetSampler()!=SAMPLE_PLAYER_SLICE?0:lastNotePlayed];
            case 8: return internalData.sampleAttack;
            case 9: return internalData.sampleDecay;
            case 31: return internalData.extraTypeUnion.samplerType;
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

const char *conditionStrings[4] = { 
    "none",
    "Rnd",
    "Len",
};
const char *syncInStrings[4] = { 
    "none",
    "midi",
    "PO",
    "VL",
};

const char *envTargets[7] = { 
    "Vol",
    "Timb",
    "Col",
    "Cut",
    "Res",
    "Pit",
    "Pan"
};

const char *lfoTargets[13] = { 
    "Vol",
    "Timb",
    "Col",
    "Cut",
    "Res",
    "Pit",
    "Pan",
    "Ev1A",
    "Ev1D",
    "Ev2A",
    "Ev2D",
    "E12A",
    "E12D"
};

// this can probably be done with some math. I'm not going to do that tonight, my brain

const uint8_t ConditionalEvery[70] = {
    1, 2, 2, 2, 1, 3, 2, 3, 3, 3, 1, 4, 2, 4, 3, 4, 4, 4,
    1, 5, 2, 5, 3, 5, 4, 5, 5, 5, 1, 6, 2, 6, 3, 6, 4, 6, 5, 6, 6, 6,
    1, 7, 2, 7, 3, 7, 4, 7, 5, 7, 6, 7, 7, 7,
    1, 8, 2, 8, 3, 8, 4, 8, 5, 8, 6, 8, 7, 8, 8, 8
};


bool VoiceData::CheckLockAndSetDisplay(uint8_t step, uint8_t pattern, uint8_t param, uint8_t value, char *paramString)
{
    uint8_t valA = 0;
    // we use the high bit here to signal if we are checking for a step or not
    // so it needs to be stripped befor asking about the specific step
    if((step & 0x80) && HasLockForStep(step&0x7f, pattern, param, valA))
    {
        sprintf(paramString, "%i", valA);
        return true;
    }
    sprintf(paramString, "%i", value);
    return false;
}

void VoiceData::GetParamsAndLocks(uint8_t param, uint8_t step, uint8_t pattern, char *strA, char *strB, uint8_t lastNotePlayed, char *pA, char *pB, bool &lockA, bool &lockB, bool showForStep)
{

    // use the high bit here to signal that we want to actually check the lock for a particular step
    if(showForStep)
        step = step | 0x80;
    
    uint8_t valA = 0, valB = 0;
    InstrumentType instrumentType = GetInstrumentType();
    ConditionModeEnum conditionModeTmp = CONDITION_MODE_NONE;
    switch(param)
    {
        case 14:
            sprintf(strA, "Dely");
            sprintf(strB, "Verb");
            lockA = CheckLockAndSetDisplay(step, pattern, DelaySend, internalData.delaySend, pA);
            lockB = CheckLockAndSetDisplay(step, pattern, ReverbSend, internalData.reverbSend, pB);
            return;
    }
    
    // all non global instruments
    switch (param)
    {
        case 7:
            sprintf(strA, "RTsp");
            sprintf(strB, "RTLn");
            lockA = CheckLockAndSetDisplay(step, pattern, RetriggerSpeed, internalData.retriggerSpeed, pA);
            if(HasLockForStep(step, pattern, RetriggerLength, valB))
            {
                sprintf(pB, "%i", (valB*8)>>8);
                lockB = true;
            }
            else
                sprintf(pB, "%i", (internalData.retriggerLength*8)>>8);
            return;
        case 11:
            sprintf(strA, "RTfd");
            sprintf(strB, "");
            if(HasLockForStep(step, pattern, (RetriggerFade), valB))
            {
                sprintf(pA, "%i", (valB-0x80));
                lockA = true;
            }
            else
                sprintf(pA, "%i", (internalData.retriggerFade-0x80));
            sprintf(pB, "");
            return;
        case 12:
            sprintf(strA, "Len");
            sprintf(strB, "Rate");
            sprintf(pA, "%i", internalData.patterns[pattern].length/4+1);
            sprintf(pB,rates[(internalData.patterns[pattern].rate*7)>>8]);
            return;
        case 13:
            sprintf(strA, "Cnd");
            sprintf(strB, "Rate");
            if(HasLockForStep(step, pattern, 26, valA))
            {
                conditionModeTmp = GetConditionMode(valA);
                lockA = true;
            }
            else
                conditionModeTmp = GetConditionMode();
            sprintf(pA, "%s", conditionStrings[conditionModeTmp]);
            uint8_t tmp = 0;
            uint8_t conditionDataTmp = internalData.conditionData;
            if(HasLockForStep(step, pattern, 27, valB))
            {
                conditionDataTmp = valB;
                lockB = true;
            }            
            switch(conditionModeTmp)
            {
                case CONDITION_MODE_RAND:
                    sprintf(pB, "%i%", ((uint16_t)conditionDataTmp*100)>>8);
                    break;
                case CONDITION_MODE_LENGTH:
                    tmp = ((uint16_t)conditionDataTmp*35)>>8;
                    sprintf(pB, "%i:%i", ConditionalEvery[tmp*2], ConditionalEvery[tmp*2+1]);
                    break;
                default:
                    sprintf(pB, "%i", conditionDataTmp);
                    break;
            }
            return;
    }
    int p = internalData.pan;
    if(instrumentType == INSTRUMENT_MACRO || instrumentType == INSTRUMENT_SAMPLE)
    {
        switch (param)
        {
            case 1:
                sprintf(strA, "Cut");
                sprintf(strB, "Res");
                lockA = CheckLockAndSetDisplay(step, pattern, 2, internalData.cutoff, pA);
                lockB = CheckLockAndSetDisplay(step, pattern, 3, internalData.resonance, pB);
                return;
            // volume / pan
            case 2:
                sprintf(strA, "Volm");
                sprintf(strB, "Pan");
                lockA = CheckLockAndSetDisplay(step, pattern, 4, internalData.volume, pA);
                if(HasLockForStep(step, pattern, 5, valB))
                {
                    p = valB;
                    lockB = true;
                }
                if(p==0x7f)
                {
                    sprintf(pB, "Cent");
                }
                else if(p < 0x80){
                    sprintf(pB, "L:%i", (0x7f-p));
                }
                else
                {
                    sprintf(pB, "R:%i", (p-0x7f));
                }
                return;
            case 3:
                // sprintf(strA, "Octv");
                // sprintf(strB, "");
                // sprintf(pA, "%i", GetOctave());
                // sprintf(pB, "");
                return;
            case 5:
                sprintf(strA, "Atk");
                sprintf(strB, "Dcy");
                lockA = CheckLockAndSetDisplay(step, pattern, 10, internalData.env2.attack, pA);
                lockB = CheckLockAndSetDisplay(step, pattern, 11, internalData.env2.decay, pB);
                return;
            case 6:
                sprintf(strA, "Rate");
                sprintf(strB, "Dpth");
                lockA = CheckLockAndSetDisplay(step, pattern, 12, internalData.lfoRate, pA);
                lockB = CheckLockAndSetDisplay(step, pattern, 13, internalData.lfoDepth, pB);
                return;
            case 8:
                sprintf(strA, "Trgt");
                sprintf(strB, "Dpth");
                if(HasLockForStep(step, pattern, 16, valA))
                {
                    sprintf(pA, "%s", envTargets[(((uint16_t)valA)*Target_Count) >> 8]);
                    lockA = true;
                }
                else
                    sprintf(pA, "%s", envTargets[(((uint16_t)internalData.env1.target)*Target_Count)>>8]);
                
                if(HasLockForStep(step, pattern, 17, valB))
                {
                    sprintf(pB, "%i", (valB-0x80));
                    lockB = true;
                }
                else
                    sprintf(pB, "%i", (internalData.env1.depth-0x80));
                return;
            case 9:
                sprintf(strA, "Trgt");
                sprintf(strB, "Dpth");
                if(HasLockForStep(step, pattern, 18, valA))
                {
                    sprintf(pA, "%s", envTargets[(((uint16_t)valA)*Target_Count) >> 8]);
                    lockA = true;
                }
                else
                    sprintf(pA, "%s", envTargets[(((uint16_t)internalData.env2.target)*Target_Count)>>8]);
                if(HasLockForStep(step, pattern, 19, valB))
                {
                    sprintf(pB, "%i", (valB-0x80));
                    lockB = true;
                }
                else
                    sprintf(pB, "%i", (internalData.env2.depth-0x80));
                return;
            case 10:
                sprintf(strA, "Trgt");
                sprintf(strB, "");
                if(HasLockForStep(step, pattern, 20, valB))
                {
                    sprintf(pA, "%s", lfoTargets[(((uint16_t)valA)*Lfo_Target_Count) >> 8]);
                    lockA = true;
                }
                else
                    sprintf(pA, "%s", lfoTargets[(((uint16_t)internalData.lfoTarget)*Lfo_Target_Count)>>8]);
                sprintf(pB, "");
                return;
            case 12:
                sprintf(strA, "Len");
                sprintf(strB, "Rate");
                sprintf(pA, "%i", internalData.patterns[pattern].length/4+1);
                sprintf(pB,rates[(internalData.patterns[pattern].rate*7)>>8]);
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
                    sprintf(pA, "%i", internalData.sampleStart[lastNotePlayed]);
                    sprintf(pB, "%i", internalData.sampleLength[lastNotePlayed]);
                }
                else
                {
                    sprintf(pA, "%i", internalData.sampleStart[0]);
                    sprintf(pB, "%i", internalData.sampleLength[0]);
                }
                return;
            case 4:
                sprintf(strA, "Atk");
                sprintf(strB, "Dcy");
                lockA = CheckLockAndSetDisplay(step, pattern, 8, internalData.sampleAttack, pA);
                lockB = CheckLockAndSetDisplay(step, pattern, 9, internalData.sampleDecay, pB);
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
                lockA = CheckLockAndSetDisplay(step, pattern, 0, internalData.timbre, pA);
                lockB = CheckLockAndSetDisplay(step, pattern, 1, internalData.color, pB);
                return;
            case 4:
                sprintf(strA, "Atk");
                sprintf(strB, "Dcy");
                lockA = CheckLockAndSetDisplay(step, pattern, 8, internalData.env1.attack, pA);
                lockB = CheckLockAndSetDisplay(step, pattern, 9, internalData.env1.decay, pB);
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
                lockA = CheckLockAndSetDisplay(step, pattern, 0, internalData.timbre, pA);
                lockB = CheckLockAndSetDisplay(step, pattern, 1, internalData.color, pB);
                return;
            // 0
            case 15:
                sprintf(strA, "Type");
                sprintf(strB, "");
                sprintf(pA, "Midi");
                sprintf(pB, "%i", (internalData.extraTypeUnion.midiChannel>>4)+1);
                return;
            default:
                return;
        }
    }
}
uint8_t head_map[] = {
  0x00, 0x00, 0x00, 0x00, 
  0x00, 0x3f, 0xfc, 0x00, 
  0x01, 0xff, 0xff, 0x80, 
  0x0f, 0x00, 0x03, 0xf0, 
  0x10, 0x00, 0x00, 0x18, 
  0x20, 0xe0, 0x07, 0x0c, 
  0x61, 0xf0, 0x0f, 0x8e, 
  0x60, 0xe0, 0x07, 0x0e, 
  0x60, 0x00, 0x00, 0x0e, 
  0x60, 0x20, 0x10, 0x0e, 
  0x20, 0x30, 0x30, 0x1c, 
  0x1c, 0x1f, 0xe0, 0x38, 
  0x0f, 0x00, 0x00, 0x70, 
  0x01, 0xff, 0xff, 0xc0, 
  0x00, 0x3f, 0xfc, 0x00, 
  0x00, 0x00, 0x00, 0x00, 
};

void VoiceData::DrawParamString(uint8_t param, char *str, uint8_t lastNotePlayed, uint8_t currentPattern, uint8_t paramLock, bool showForStep)
{
    ssd1306_t* disp = GetDisplay();
    uint8_t width = 36;
    uint8_t column4 = 128-width;
    if(param == 3)
    {
        // lol
        uint8_t x = 0;
        uint8_t y = 0;
        for(int i=0;i<64;i++)
        {
            for(int b=0;b<8;b++)
            {
                if((head_map[i]&(0x1<<(7-b))) > 0)
                {
                    ssd1306_draw_pixel(disp, x+column4, y);
                }
                else
                {
                    ssd1306_clear_pixel(disp, x+column4, y);
                }
                x++;
                if(x>=32)
                {
                    x = 0;
                    y++;
                }
            }
        }

    }
    else
    {
        bool lockA = false, lockB = false;
        GetParamsAndLocks(param, paramLock, currentPattern, str, str+16, lastNotePlayed, str+32, str+48, lockA, lockB, showForStep);
        if(lockA)
            ssd1306_draw_square_rounded(disp, column4, 0, width, 15);
        if(lockB)
            ssd1306_draw_square_rounded(disp, column4, 17, width, 15);
        ssd1306_draw_string_gfxfont(disp, column4+3, 12, str+32, !lockA, 1, 1, &m6x118pt7b);
        ssd1306_draw_string_gfxfont(disp, column4+3, 17+12, str+48, !lockB, 1, 1, &m6x118pt7b);
        
        ssd1306_draw_string_gfxfont(disp, column4-33, 12, str, true, 1, 1, &m6x118pt7b);    
        ssd1306_draw_string_gfxfont(disp, column4-33, 17+12, str+16, true, 1, 1, &m6x118pt7b);
    }
}


int8_t VoiceData::GetOctave()
{
    return ((int8_t)(internalData.octave/51))-2;
}

/* PARAMETER LOCK BEHAVIOR */
void VoiceData::StoreParamLock(uint8_t param, uint8_t step, uint8_t pattern, uint8_t value)
{
    ParamLock *lock;
    if(GetLockForStep(&lock, step, pattern, param))
    {
        lock->value = value;
        // printf("updated param lock step: %i param: %i value: %i\n", step, param, value);
        return;
    }
    if(lockPool.GetFreeParamLock(&lock))
    {
        if(!lockPool.IsValidLock(lock))
        {
            printf("out of lock space\n failed to add new lock");
            return;
        }
        lock->param = param;
        lock->step = step;
        lock->value = value;
        lock->next = locksForPattern[pattern];
        locksForPattern[pattern] = lockPool.GetLockPosition(lock);
        // printf("added param lock step: %i param: %i value: %i at lock position: %i\n", step, param, value, lockPool.GetLockPosition(lock));
        return;
    }
    printf("failed to add param lock\n");
}
void VoiceData::ClearParameterLocks(uint8_t pattern)
{
    ParamLock* lock = lockPool.GetLock(locksForPattern[pattern]);
    while(lockPool.IsValidLock(lock))
    {
        ParamLock* nextLock = lockPool.GetLock(lock->next);
        lockPool.ReturnLockToPool(lock);
        lock = nextLock;
    }
    locksForPattern[pattern] = ParamLockPool::InvalidLockPosition();
}
void VoiceData::RemoveLocksForStep(uint8_t pattern, uint8_t step)
{
    ParamLock* lock = lockPool.GetLock(locksForPattern[pattern]);
    ParamLock* lastLock = lock;
    while(lockPool.IsValidLock(lock))
    {
        ParamLock* nextLock = lockPool.GetLock(lock->next);
        if(lock->step == step)
        {
            lastLock->next = lock->next;
            lockPool.ReturnLockToPool(lock);
            if(lockPool.GetLockPosition(lock) == locksForPattern[pattern])
            {
                locksForPattern[pattern] = lockPool.GetLockPosition(nextLock);
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
    ParamLock* lock = lockPool.GetLock(locksForPattern[fromPattern]);
    while(lockPool.IsValidLock(lock))
    {
        StoreParamLock(lock->param, lock->step, toPattern, lock->value);
        lock = lockPool.GetLock(lock->next);
    }
}
bool VoiceData::HasLockForStep(uint8_t step, uint8_t pattern, uint8_t param, uint8_t &value)
{
    // because we use the highbit to signal if we are checking a specific step in the callsite, this must be stripped here
    step = step&0x7f;
    ParamLock *lock;
    if(GetLockForStep(&lock, step, pattern, param))
    {
        value = lock->value;
        return true;
    }
    return false;
}
bool VoiceData::HasAnyLockForStep(uint8_t step, uint8_t pattern)
{
    ParamLock* lock = lockPool.GetLock(locksForPattern[pattern]);
    while(lockPool.IsValidLock(lock))
    {
        if(lock->step == step)
        {
            return true;
        }
        lock = lockPool.GetLock(lock->next);
    }
    return false;
}
bool VoiceData::GetLockForStep(ParamLock **lockOut, uint8_t step, uint8_t pattern, uint8_t param)
{
    ParamLock* lock = lockPool.GetLock(locksForPattern[pattern]);
    while(lockPool.IsValidLock(lock))
    {
        if(lock->param == param && lock->step == step)
        {
            *lockOut = lock;
            return true;
        }
        lock = lockPool.GetLock(lock->next);
    }
    return false;
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

    int lostLockCount = 0;
    for(int l=0;l<16*256;l++)
    {
        ParamLock *searchingForLock = voiceData.lockPool.GetLock(l);
        bool foundLock = false;
        {
            for(int p=0; p<16; p++)
            {
                int lockCount = 0;
                ParamLock *lock = voiceData.lockPool.GetLock(voiceData.locksForPattern[p]);
                while(voiceData.lockPool.IsValidLock(lock))
                {
                    if(lock == searchingForLock){
                        foundLock = true;
                        break;
                    }
                    if(lock == voiceData.lockPool.GetLock(lock->next))
                    {
                        break;
                    }
                    lock = voiceData.lockPool.GetLock(lock->next);
                }
            }
        }
    }
    printf("lost lock count: %i\n", lostLockCount);

}