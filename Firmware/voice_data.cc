#include "voice_data.h"
#include "m6x118pt7b.h"


void VoiceData::InitDefaults()
{
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
    
    internalData.delaySend = 0;
    internalData.reverbSend = 0;
    
    internalData.extraTypeUnion.synthShape = 0;
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
            case Timbre: return internalData.timbre;
            case Color: return internalData.color;
        }
    }
    if(GetInstrumentType() == INSTRUMENT_MIDI)
    {
        switch(param)
        {
            case Timbre: return internalData.timbre;
            case MidiHold: return internalData.color;
        }
    }
    if(GetInstrumentType() == INSTRUMENT_SAMPLE)
    {
        switch(param)
        {
            case SampleIn: return internalData.sampleStart[lastNotePlayed];
            case SampleOut: return internalData.sampleLength[lastNotePlayed];
            case AttackTime: return internalData.sampleAttack;
            case DecayTime: return internalData.sampleDecay;
        }
    }
    switch(param)
    {
        case Cutoff: return internalData.cutoff;
        case Resonance: return internalData.resonance;
        case Volume: return internalData.volume;
        case Pan: return internalData.pan;
        case AttackTime: return internalData.env1.attack;
        case DecayTime: return internalData.env1.decay;
        case Env1Target: return internalData.env1.target;
        case Env1Depth: return internalData.env1.depth;
        case AttackTime2: return internalData.env2.attack;
        case DecayTime2: return internalData.env2.decay;
        case Env2Target: return internalData.env2.target;
        case Env2Depth: return internalData.env2.depth;
        case LFORate: return internalData.lfoRate;
        case LFODepth: return internalData.lfoDepth;
        case Lfo1Target: return internalData.lfoTarget;
        case RetriggerSpeed: return internalData.retriggerSpeed;
        case RetriggerLength: return internalData.retriggerLength;
        case RetriggerFade: return internalData.retriggerFade;
        case DelaySend: return internalData.delaySend;
        case ReverbSend: return internalData.reverbSend;
        case ConditionMode: return internalData.conditionMode;
        case ConditionData: return internalData.conditionData;
    }
    return 0;
}

// used for setting the value in place
// currentPattern is used for alterning things that have per pattern values (pattern length)
// last n
uint8_t& VoiceData::GetParam(uint8_t param, uint8_t lastNotePlayed, uint8_t currentPattern)
{
    if(param == 44)
    {
        return internalData.delaySend;
    }
    if(param == 45)
    {
        return internalData.reverbSend;
    }
    if(GetInstrumentType() != INSTRUMENT_GLOBAL && param == 46)
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
        case 12: return internalData.cutoff;
        case 13: return internalData.resonance;
        case 14: return internalData.volume;
        case 15: return internalData.pan;
        case 22: return internalData.env2.attack;
        case 23: return internalData.env2.decay;
        case 24: return internalData.lfoRate;
        case 25: return internalData.lfoDepth;
        case 26: return internalData.retriggerSpeed;
        case 27: return internalData.retriggerLength;
        case 30: return internalData.env1.target;
        case 31: return internalData.env1.depth;
        case 32: return internalData.env2.target;
        case 33: return internalData.env2.depth;
        case 34: return internalData.lfoTarget;
        case 35: return internalData.lfoDelay;
        case 36: return internalData.retriggerFade;
        case 42: return internalData.conditionMode;
        case 43: return internalData.conditionData;
    }
    if(GetInstrumentType() == INSTRUMENT_MACRO)
    {
        switch (param)
        {
            case 10: return internalData.timbre;
            case 11: return internalData.color;
            case 20: return internalData.env1.attack;
            case 21: return internalData.env1.decay;
            case 47: return internalData.extraTypeUnion.synthShape;
            default:
                break;
        }
    }    
    if(GetInstrumentType() == INSTRUMENT_MIDI)
    {
        switch (param)
        {
            case 10: return internalData.timbre;
            case 11: return internalData.color;
            case 47: return internalData.extraTypeUnion.midiChannel;
            default:
                break;
        }
    }    
    if(GetInstrumentType() == INSTRUMENT_SAMPLE)
    {
        switch (param)
        {
            case 10: return internalData.sampleStart[GetSampler()!=SAMPLE_PLAYER_SLICE?0:lastNotePlayed];
            case 11: return internalData.sampleLength[GetSampler()!=SAMPLE_PLAYER_SLICE?0:lastNotePlayed];
            case 20: return internalData.sampleAttack;
            case 21: return internalData.sampleDecay;
            case 47: return internalData.extraTypeUnion.samplerType;
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


bool VoiceData::CheckLockAndSetDisplay(bool showForStep, uint8_t step, uint8_t pattern, uint8_t param, uint8_t value, char *paramString)
{
    uint8_t valA = 0;
    sprintf(paramString, "%i", value);
    return false;
}

void VoiceData::GetParamsAndLocks(uint8_t param, uint8_t step, uint8_t pattern, char *strA, char *strB, uint8_t lastNotePlayed, char *pA, char *pB, bool &lockA, bool &lockB, bool showForStep)
{

    // use the high bit here to signal that we want to actually check the lock for a particular step    
    uint8_t valA = 0, valB = 0;
    InstrumentType instrumentType = GetInstrumentType();
    ConditionModeEnum conditionModeTmp = CONDITION_MODE_NONE;
    switch(param)
    {
        case 22:
            sprintf(strA, "Dely");
            sprintf(strB, "Verb");
            lockA = CheckLockAndSetDisplay(showForStep, step, pattern, DelaySend, internalData.delaySend, pA);
            lockB = CheckLockAndSetDisplay(showForStep, step, pattern, ReverbSend, internalData.reverbSend, pB);
            return;
    }
    
    // all non global instruments
    switch (param)
    {
        case 13:
            sprintf(strA, "RTsp");
            sprintf(strB, "RTLn");
            lockA = CheckLockAndSetDisplay(showForStep, step, pattern, RetriggerSpeed, internalData.retriggerSpeed, pA);
            sprintf(pB, "%i", (internalData.retriggerLength*8)>>8);
            return;
        case 18:
            sprintf(strA, "RTfd");
            sprintf(strB, "");
            sprintf(pA, "%i", (internalData.retriggerFade-0x80));
            sprintf(pB, "");
            return;
        case 21:
            sprintf(strA, "Cnd");
            sprintf(strB, "Rate");
            conditionModeTmp = GetConditionMode();
            sprintf(pA, "%s", conditionStrings[conditionModeTmp]);
            uint8_t tmp = 0;
            uint8_t conditionDataTmp = internalData.conditionData;
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
            case 6:
                sprintf(strA, "Cut");
                sprintf(strB, "Res");
                lockA = CheckLockAndSetDisplay(showForStep, step, pattern, Cutoff, internalData.cutoff, pA);
                lockB = CheckLockAndSetDisplay(showForStep, step, pattern, Resonance, internalData.resonance, pB);
                return;
            // volume / pan
            case 7:
                sprintf(strA, "Volm");
                sprintf(strB, "Pan");
                lockA = CheckLockAndSetDisplay(showForStep, step, pattern, Volume, internalData.volume, pA);
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
            case 8:
                // sprintf(strA, "Octv");
                // sprintf(strB, "");
                // sprintf(pA, "%i", GetOctave());
                // sprintf(pB, "");
                return;
            case 11:
                sprintf(strA, "Atk");
                sprintf(strB, "Dcy");
                lockA = CheckLockAndSetDisplay(showForStep, step, pattern, AttackTime2, internalData.env2.attack, pA);
                lockB = CheckLockAndSetDisplay(showForStep, step, pattern, DecayTime2, internalData.env2.decay, pB);
                return;
            case 12:
                sprintf(strA, "Rate");
                sprintf(strB, "Dpth");
                lockA = CheckLockAndSetDisplay(showForStep, step, pattern, LFORate, internalData.lfoRate, pA);
                lockB = CheckLockAndSetDisplay(showForStep, step, pattern, LFODepth, internalData.lfoDepth, pB);
                return;
            case 15:
                sprintf(strA, "Trgt");
                sprintf(strB, "Dpth");
                sprintf(pA, "%s", envTargets[(((uint16_t)internalData.env1.target)*Target_Count)>>8]);
                
                sprintf(pB, "%i", (internalData.env1.depth-0x80));
                return;
            case 16:
                sprintf(strA, "Trgt");
                sprintf(strB, "Dpth");
                sprintf(pA, "%s", envTargets[(((uint16_t)internalData.env2.target)*Target_Count)>>8]);
                sprintf(pB, "%i", (internalData.env2.depth-0x80));
                return;
            case 17:
                sprintf(strA, "Trgt");
                sprintf(strB, "");
                sprintf(pA, "%s", lfoTargets[(((uint16_t)internalData.lfoTarget)*Lfo_Target_Count)>>8]);
                sprintf(pB, "");
                return;
        }
    }
    if(GetInstrumentType() == INSTRUMENT_SAMPLE)
    {
        switch (param)
        {
            // 0
            case 5:
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
            case 10:
                sprintf(strA, "Atk");
                sprintf(strB, "Dcy");
                lockA = CheckLockAndSetDisplay(showForStep, step, pattern, AttackTime, internalData.sampleAttack, pA);
                lockB = CheckLockAndSetDisplay(showForStep, step, pattern, DecayTime, internalData.sampleDecay, pB);
                return;
            case 23:
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
            case 5:
                sprintf(strA, "Timb");
                sprintf(strB, "Colr");
                lockA = CheckLockAndSetDisplay(showForStep, step, pattern, Timbre, internalData.timbre, pA);
                lockB = CheckLockAndSetDisplay(showForStep, step, pattern, Color, internalData.color, pB);
                return;
            case 10:
                sprintf(strA, "Atk");
                sprintf(strB, "Dcy");
                lockA = CheckLockAndSetDisplay(showForStep, step, pattern, AttackTime, internalData.env1.attack, pA);
                lockB = CheckLockAndSetDisplay(showForStep, step, pattern, DecayTime, internalData.env1.decay, pB);
                return;
            case 23:
                sprintf(strA, "Type");
                sprintf(strB, "");
                sprintf(pA, "Synt");
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
            case 5:
                sprintf(strA, "Vel");
                sprintf(strB, "Hold");
                lockA = CheckLockAndSetDisplay(showForStep, step, pattern, Timbre, internalData.timbre, pA);
                sprintf(pB, "%i", (internalData.color>>4)+1);
                return;
            // 0
            case 23:
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
    if(param == 8)
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


void TestVoiceData()
{

}