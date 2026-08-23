#include "voice_data.h"
#include "m6x118pt7b.h"
#include "Midi.h"

ParamLockPool VoiceData::lockPool;

// param pads repurposed as midi cc sends on midi tracks (see voice_data.h)
const uint8_t MidiCCPadParams[MIDI_CC_PAGE_COUNT] = {7, 8, 10, 11, 12, 15, 16, 17, 22};

// intervals above the root, in semitones
struct MidiChordShape
{
    const char *name;
    uint8_t count;
    int8_t intervals[4];
};

// Each shape ends on the octave so that stacking it past its own voice count keeps
// producing a normal voicing. Maj at 5 extra voices is
// 4,7,12,16,19 (triad then triad an octave up), not 4,7,16,19,28.
static const MidiChordShape midiChordShapes[MIDI_CHORD_SHAPE_COUNT] = {
    {"Oct",  1, {12,  0,  0,  0}},
    {"5th",  2, { 7, 12,  0,  0}},
    {"Maj",  3, { 4,  7, 12,  0}},
    {"Min",  3, { 3,  7, 12,  0}},
    {"Sus2", 3, { 2,  7, 12,  0}},
    {"Sus4", 3, { 5,  7, 12,  0}},
    {"Dim",  3, { 3,  6, 12,  0}},
    {"Aug",  3, { 4,  8, 12,  0}},
    {"Maj6", 4, { 4,  7,  9, 12}},
    {"Min6", 4, { 3,  7,  9, 12}},
    {"Dom7", 4, { 4,  7, 10, 12}},
    {"Maj7", 4, { 4,  7, 11, 12}},
    {"Min7", 4, { 3,  7, 10, 12}},
    {"Maj9", 4, { 4,  7, 11, 14}},
    {"Min9", 4, { 3,  7, 10, 14}},
};

uint8_t VoiceData::BuildChord(int16_t root, uint8_t rawVoices, uint8_t rawShape, int16_t *notes)
{
    notes[0] = root;
    uint8_t extras = ChordVoiceCount(rawVoices);
    if(extras == 0)
        return 1;
    const MidiChordShape &shape = midiChordShapes[ChordShapeIndex(rawShape)];
    uint8_t written = 1;
    for(uint8_t i=0;i<extras;i++)
    {
        int16_t note = root + shape.intervals[i%shape.count] + 12*(i/shape.count);
        // internal pitch is 12 above the transmitted note, so 139 is midi note 127.
        if(note > 139)
            break;
        notes[written++] = note;
    }
    return written;
}

int8_t VoiceData::MidiCCPageForPad(uint8_t pad)
{
    for(int8_t i=0;i<MIDI_CC_PAGE_COUNT;i++)
    {
        if(MidiCCPadParams[i] == pad)
            return i;
    }
    return -1;
}

// GetParam indices on a cc number page are (pad+PARAM_PAGE_STRIDE)*2 (+1 for knob B)
int8_t VoiceData::MidiCCSlotForParamIndex(uint8_t paramIndex)
{
    uint8_t pad = paramIndex>>1;
    if(pad < PARAM_PAGE_STRIDE)
        return -1;
    int8_t page = MidiCCPageForPad(pad-PARAM_PAGE_STRIDE);
    if(page < 0)
        return -1;
    return page*2 + (paramIndex&1);
}

void VoiceData::FormatMidiCCSlot(uint8_t slot, char *out, bool ccPrefix)
{
    uint8_t target = GetMidiCCSlotTarget(slot);
    if(target == MIDI_CC_SLOT_OFF)
        sprintf(out, "Off");
    else if(target == MIDI_CC_SLOT_PROGRAM)
        sprintf(out, "Prog");
    else
        sprintf(out, ccPrefix ? CC_LIGATURE "%i" : "%i", target - MIDI_CC_SLOT_CC_BASE);
}

void VoiceData::SendMidiCC(Midi *midi, uint8_t slot, uint8_t value)
{
    uint8_t target = GetMidiCCSlotTarget(slot);
    if(target == MIDI_CC_SLOT_OFF)
        return;
    // knobs are 8 bit, cc and program values are 7. Midi owns the de-dup because
    // it has to be keyed on what goes out, not on this voice's slot
    if(target == MIDI_CC_SLOT_PROGRAM)
        midi->ProgramChange(GetMidiChannel(), value>>1);
    else
        midi->ControlChange(GetMidiChannel(), target - MIDI_CC_SLOT_CC_BASE, value>>1);
}

void VoiceData::SendMidiCCs(Midi *midi, const uint8_t *resolvedParams)
{
    int16_t programSlot = -1;
    uint8_t programValue = 0;
    for(uint8_t page=0;page<MIDI_CC_PAGE_COUNT;page++)
    {
        uint8_t pad = MidiCCPadParams[page];
        for(uint8_t ab=0;ab<2;ab++)
        {
            uint8_t slot = page*2+ab;
            uint8_t target = GetMidiCCSlotTarget(slot);
            if(target == MIDI_CC_SLOT_OFF)
                continue; // nothing to look up or send
            if(target == MIDI_CC_SLOT_PROGRAM)
            {
                // held back until the end: bank select (cc0 / cc32) has to reach
                // the synth before the program change that selects within it.
                // More than one slot on program is degenerate, so last one wins.
                programSlot = slot;
                programValue = resolvedParams[pad*2+ab];
                continue;
            }
            SendMidiCC(midi, slot, resolvedParams[pad*2+ab]);
        }
    }
    if(programSlot >= 0)
        SendMidiCC(midi, (uint8_t)programSlot, programValue);
}

void VoiceData::InitDefaults()
{
    for (size_t i = 0; i < 16; i++)
    {
        for (size_t s = 0; s < 64; s++)
            locksForPatternStep[i][s] = ParamLockPool::InvalidLockPosition();
        internalData.patterns[i].rate = 2*37; // 1x
        internalData.patterns[i].length = 15*4; // need to up this to fit into 0xff
    }
    SetDefaultParams();
}

void VoiceData::SetDefaultParams()
{
    internalData.portamento = 0x00;
    internalData.fineTune = 0x80;

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

    // midi chords default to off (root note only)
    internalData.chordVoices = 0x00;
    internalData.chordShape = 0x00;

    // midiCC needs no init: a zero byte decodes to MIDI_CC_SLOT_OFF, so every slot
    // starts silent and a fresh midi track never sprays controllers at a synth
}

bool VoiceDataInternal_encode_locks(pb_ostream_t *ostream, const pb_field_t *field, void * const *arg)
{
    uint16_t (*lp)[64] = (uint16_t(*)[64])(*arg);

    for (int p = 0; p < 16; p++)
    {
        for (int s = 0; s < 64; s++)
        {
            if(lp[p][s] == ParamLockPool::InvalidLockPosition())
                continue;
            if (!pb_encode_tag_for_field(ostream, field))
                return false;
            VoiceDataInternal_LockPointer lock = VoiceDataInternal_LockPointer_init_zero;
            lock.pattern = p;
            lock.step = s;
            lock.pointer = lp[p][s];
            if (!pb_encode_submessage(ostream, VoiceDataInternal_LockPointer_fields, &lock))
            {
                const char * error = PB_GET_ERROR(ostream);
                printf("VoiceDataInternal_encode_locks error: %s", error);
                return false;
            }
        }
    }
    return true;
}
void VoiceData::Serialize(pb_ostream_t *s)
{
    s->bytes_written = 0;
    internalData.version = 1;
    internalData.has_env1 = true;
    internalData.has_env2 = true;
    internalData.which_extraTypeUnion = VoiceDataInternal_synthShape_tag;
    internalData.locksForPattern.funcs.encode = &VoiceDataInternal_encode_locks;
    internalData.locksForPattern.arg = (void*)locksForPatternStep;
    pb_encode_ex(s, VoiceDataInternal_fields, &internalData, PB_ENCODE_DELIMITED);
}
bool VoiceDataInternal_decode_locks(pb_istream_t *stream, const pb_field_iter_t *field, void **arg)
{
    uint16_t (*lp)[64] = (uint16_t(*)[64])(*arg);
    VoiceDataInternal_LockPointer lock = VoiceDataInternal_LockPointer_init_zero;
    if (!pb_decode(stream, VoiceDataInternal_LockPointer_fields, &lock))
        return false;
    if (lock.pattern < 16 && lock.step < 64)
        lp[lock.pattern][lock.step] = lock.pointer;
    return true;
}


void VoiceData::Deserialize(pb_istream_t *s)
{
    internalData.locksForPattern.funcs.decode = &VoiceDataInternal_decode_locks;
    internalData.locksForPattern.arg = (void*)locksForPatternStep;
    if(!pb_decode_ex(s, VoiceDataInternal_fields, &internalData, PB_DECODE_DELIMITED))
    {
        const char * error = PB_GET_ERROR(s);
        printf("VoiceData deserialize error: %s\n", error);
    }
    // count the number of notes for each pattern
    for(int i=0;i<16;i++)
    {
        noteCountForPattern[i] = 0;
        for(int j=0;j<64;j++)
        {
            if((GetNotesForPattern(i)[j] >> 7) == 1)
            {
                noteCountForPattern[i]++;
            }
        }
    }
}
void VoiceData::SerializeStatic(pb_ostream_t *s)
{
    lockPool.Serialize(s);
}

void VoiceData::DeserializeStatic(pb_istream_t *s, VoiceData *patterns)
{
    lockPool.Deserialize(s);
    // Old saves stored all locks for a pattern in one chain at [pattern][0].
    // Each lock carries its correct step. Redistribute now that the pool is loaded.
    for (int v = 0; v < 16; v++) {
        if (patterns[v].internalData.version != 0)
            continue;
        for (int p = 0; p < 16; p++) {
            for (int s = 0; s < 64; s++) {
                uint16_t pos = patterns[v].locksForPatternStep[p][s];
                if (!lockPool.IsValidLock(pos)) continue;
                patterns[v].locksForPatternStep[p][s] = ParamLockPool::InvalidLockPosition();
                while (lockPool.IsValidLock(pos)) {
                    ParamLock *lock = lockPool.GetLock(pos);
                    uint16_t next = lock->next;
                    lock->next = patterns[v].locksForPatternStep[p][lock->step];
                    patterns[v].locksForPatternStep[p][lock->step] = pos;
                    pos = next;
                }
            }
        }
    }
}
// incorporates the lock if any
// length and rate belong to the pattern rather than to one step, so a lock can
// never stand in for them
static bool ParamTakesLocks(uint8_t param)
{
    return param != Length && param != Rate;
}

uint8_t VoiceData::GetParamValue(ParamType param, uint8_t lastNotePlayed, uint8_t step, uint8_t pattern, bool applyLocks)
{
    uint8_t value;
    // a lock for this step stands in for the stored value. A live played note
    // belongs to no step, and so resolves the stored values only
    if(applyLocks && ParamTakesLocks(param) && HasLockForStep(step, pattern, param, value))
        return value;
    // GetParam already holds the one param index to storage mapping, including
    // everything that varies by instrument type
    return GetParam(param, lastNotePlayed, pattern);
}

void VoiceData::FillResolvedParamCache(uint8_t step, uint8_t pattern, uint8_t lastNotePlayed, uint8_t* cache, bool applyLocks)
{
    // one pass over the same mapping GetParam uses, so the cache can never drift
    // from what a single GetParamValue would have returned
    for(uint8_t p=0;p<PARAM_CACHE_SIZE;p++)
        cache[p] = GetParam(p, lastNotePlayed, pattern);

    // a live played note belongs to no step, so it keeps the stored values
    if(!applyLocks)
        return;
    // override with any locks for this step - iterate only this step's chain
    ParamLock* lock = lockPool.GetLock(locksForPatternStep[pattern][step]);
    while(lockPool.IsValidLock(lock))
    {
        if(lock->param < PARAM_CACHE_SIZE && ParamTakesLocks(lock->param))
            cache[lock->param] = lock->value;
        lock = lockPool.GetLock(lock->next);
    }
}

// used for setting the value in place
// currentPattern is used for alterning things that have per pattern values (pattern length)
// last n
uint8_t& VoiceData::GetParam(uint8_t param, uint8_t lastNotePlayed, uint8_t currentPattern)
{
    if(GetInstrumentType() == INSTRUMENT_MIDI)
    {
        // second page of a cc param pad: edits which cc number the slot transmits on
        int8_t ccSlot = MidiCCSlotForParamIndex(param);
        if(ccSlot >= 0)
            return internalData.midiCC[ccSlot];
        // the filter pad drives the chord on midi tracks, so these have to be
        // claimed before the shared switch below hands them to cutoff / resonance
        if(param == ChordVoices) return internalData.chordVoices;
        if(param == ChordShape) return internalData.chordShape;
    }
    if(param == 44)
    {
        return internalData.delaySend;
    }
    if(param == 45)
    {
        return internalData.reverbSend;
    }
    if(param == 46)
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
        case 16: return internalData.portamento;
        case 17: return internalData.fineTune;
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
        case 35: return internalData.lfoShape;
        case 36: return internalData.retriggerFade;
        case 40: return internalData.patterns[currentPattern].length;
        case 41: return internalData.patterns[currentPattern].rate;
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
            // pad 10 is a cc page on midi tracks; it parks its two values in env1
            case 20: return internalData.env1.attack;
            case 21: return internalData.env1.decay;
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
    // nothing maps here for this instrument type. Clear the sink first so a read
    // is always 0, whatever an earlier caller may have written through it
    nothing = 0;
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
const char *envTargets[7] = { 
    "Vol",
    "Timb",
    "Col",
    "Cut",
    "Res",
    "Pit",
    "Pan"
};

const char *lfoShapes[8] = {
    "Sin",
    "Tri",
    "Sqr",
    "Rmp/",
    "Rmp\\",
    "Rnd",
    "SRnd",
    "Sin"  // fallback matches Lfo_Shape_Count
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
    // we use the high bit here to signal if we are checking for a step or not
    // so it needs to be stripped befor asking about the specific step
    if(showForStep && HasLockForStep(step, pattern, param, valA))
    {
        sprintf(paramString, "%i", valA);
        return true;
    }
    sprintf(paramString, "%i", value);
    return false;
}

// Handles the pages that only exist on midi tracks: the chord page and the two
// faces of each cc page (values, and the cc numbers behind a double press).
// Returns false for pages midi shares with the other instrument types.
bool VoiceData::MidiParamsAndLocks(uint8_t param, uint8_t step, uint8_t pattern, char *strA, char *strB, char *pA, char *pB, bool &lockA, bool &lockB, bool showForStep)
{
    uint8_t valA = 0, valB = 0;

    // chords, on the pad the other instruments use for the filter
    if(param == 6)
    {
        sprintf(strA, "Chrd");
        sprintf(strB, "Shp");
        uint8_t voices = internalData.chordVoices;
        if(showForStep && HasLockForStep(step, pattern, ChordVoices, valA))
        {
            voices = valA;
            lockA = true;
        }
        uint8_t shape = internalData.chordShape;
        if(showForStep && HasLockForStep(step, pattern, ChordShape, valB))
        {
            shape = valB;
            lockB = true;
        }
        uint8_t extras = ChordVoiceCount(voices);
        if(extras == 0)
            sprintf(pA, "Off");
        else
            sprintf(pA, "%i", extras);
        sprintf(pB, "%s", midiChordShapes[ChordShapeIndex(shape)].name);
        return true;
    }

    // cc values, labelled with the cc number they go out on
    int8_t page = MidiCCPageForPad(param);
    if(page >= 0)
    {
        FormatMidiCCSlot(page*2,   strA, true);
        FormatMidiCCSlot(page*2+1, strB, true);
        uint8_t aVal = GetParam(param*2, 0, pattern);
        uint8_t bVal = GetParam(param*2+1, 0, pattern);
        if(showForStep && HasLockForStep(step, pattern, param*2, valA))
        {
            aVal = valA;
            lockA = true;
        }
        if(showForStep && HasLockForStep(step, pattern, param*2+1, valB))
        {
            bVal = valB;
            lockB = true;
        }
        // cc values go out as 7 bit, show what the receiver will see
        sprintf(pA, "%i", aVal>>1);
        sprintf(pB, "%i", bVal>>1);
        return true;
    }

    // cc number assignment page (double press of a cc pad)
    if(param >= PARAM_PAGE_STRIDE)
    {
        page = MidiCCPageForPad(param-PARAM_PAGE_STRIDE);
        if(page >= 0)
        {
            sprintf(strA, "CC#A");
            sprintf(strB, "CC#B");
            FormatMidiCCSlot(page*2,   pA, false);
            FormatMidiCCSlot(page*2+1, pB, false);
            return true;
        }
    }
    return false;
}

void VoiceData::GetParamsAndLocks(uint8_t param, uint8_t step, uint8_t pattern, char *strA, char *strB, uint8_t lastNotePlayed, char *pA, char *pB, bool &lockA, bool &lockB, bool showForStep)
{

    // use the high bit here to signal that we want to actually check the lock for a particular step    
    uint8_t valA = 0, valB = 0;
    InstrumentType instrumentType = GetInstrumentType();
    ConditionModeEnum conditionModeTmp = CONDITION_MODE_NONE;
    // midi tracks reshuffle several pads (chords on the filter pad, cc sends on the
    // pads whose audio params do nothing), so they get first refusal on the page
    if(instrumentType == INSTRUMENT_MIDI
        && MidiParamsAndLocks(param, step, pattern, strA, strB, pA, pB, lockA, lockB, showForStep))
    {
        return;
    }
    switch(param)
    {
        case 22:
            sprintf(strA, "Dely");
            sprintf(strB, "Verb");
            lockA = CheckLockAndSetDisplay(showForStep, step, pattern, DelaySend, internalData.delaySend, pA);
            lockB = CheckLockAndSetDisplay(showForStep, step, pattern, ReverbSend, internalData.reverbSend, pB);
            return;
    }
    
    // shared by every instrument type
    switch (param)
    {
        case 13:
            sprintf(strA, "RTsp");
            sprintf(strB, "RTLn");
            lockA = CheckLockAndSetDisplay(showForStep, step, pattern, RetriggerSpeed, internalData.retriggerSpeed, pA);
            if(showForStep && HasLockForStep(step, pattern, RetriggerLength, valB))
            {
                sprintf(pB, "%i", (valB*8)>>8);
                lockB = true;
            }
            else
                sprintf(pB, "%i", (internalData.retriggerLength*8)>>8);
            return;
        case 18:
            sprintf(strA, "RTfd");
            sprintf(strB, "");
            if(showForStep && HasLockForStep(step, pattern, (RetriggerFade), valB))
            {
                sprintf(pA, "%i", (valB-0x80));
                lockA = true;
            }
            else
                sprintf(pA, "%i", (internalData.retriggerFade-0x80));
            sprintf(pB, "");
            return;
        case 20:
            sprintf(strA, "Len");
            sprintf(strB, "Rate");
            sprintf(pA, "%i", internalData.patterns[pattern].length/4+1);
            sprintf(pB,rates[(internalData.patterns[pattern].rate*7)>>8]);
            return;
        case 21:
            sprintf(strA, "Cnd");
            sprintf(strB, "Rate");
            if(showForStep && HasLockForStep(step, pattern, ConditionMode, valA))
            {
                conditionModeTmp = GetConditionMode(valA);
                lockA = true;
            }
            else
                conditionModeTmp = GetConditionMode();
            sprintf(pA, "%s", conditionStrings[conditionModeTmp]);
            uint8_t tmp = 0;
            uint8_t conditionDataTmp = internalData.conditionData;
            if(showForStep && HasLockForStep(step, pattern, ConditionData, valB))
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
                if(showForStep && HasLockForStep(step, pattern, Pan, valB))
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
            case 8:
                sprintf(strA, "Port");
                sprintf(strB, "Fine");
                lockA = CheckLockAndSetDisplay(showForStep, step, pattern, Portamento, internalData.portamento, pA);
                if(showForStep && HasLockForStep(step, pattern, (FineTune), valB))
                {
                    sprintf(pB, "%i", (valB-0x80));
                    lockB = true;
                }
                else
                    sprintf(pB, "%i", (internalData.fineTune-0x80));
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
                if(showForStep && HasLockForStep(step, pattern, Env1Target, valA))
                {
                    sprintf(pA, "%s", envTargets[(((uint16_t)valA)*Target_Count) >> 8]);
                    lockA = true;
                }
                else
                    sprintf(pA, "%s", envTargets[(((uint16_t)internalData.env1.target)*Target_Count)>>8]);
                
                if(showForStep && HasLockForStep(step, pattern, Env1Depth, valB))
                {
                    sprintf(pB, "%i", (valB-0x80));
                    lockB = true;
                }
                else
                    sprintf(pB, "%i", (internalData.env1.depth-0x80));
                return;
            case 16:
                sprintf(strA, "Trgt");
                sprintf(strB, "Dpth");
                if(showForStep && HasLockForStep(step, pattern, Env2Target, valA))
                {
                    sprintf(pA, "%s", envTargets[(((uint16_t)valA)*Target_Count) >> 8]);
                    lockA = true;
                }
                else
                    sprintf(pA, "%s", envTargets[(((uint16_t)internalData.env2.target)*Target_Count)>>8]);
                if(showForStep && HasLockForStep(step, pattern, Env2Depth, valB))
                {
                    sprintf(pB, "%i", (valB-0x80));
                    lockB = true;
                }
                else
                    sprintf(pB, "%i", (internalData.env2.depth-0x80));
                return;
            case 17:
                sprintf(strA, "Trgt");
                sprintf(strB, "Shp");
                if(showForStep && HasLockForStep(step, pattern, Lfo1Target, valA))
                {
                    sprintf(pA, "%s", lfoTargets[(((uint16_t)valA)*Lfo_Target_Count) >> 8]);
                    lockA = true;
                }
                else
                    sprintf(pA, "%s", lfoTargets[(((uint16_t)internalData.lfoTarget)*Lfo_Target_Count)>>8]);
                if(showForStep && HasLockForStep(step, pattern, Lfo1Shape, valB))
                {
                    sprintf(pB, "%s", lfoShapes[(((uint16_t)valB)*Lfo_Shape_Count) >> 8]);
                    lockB = true;
                }
                else
                    sprintf(pB, "%s", lfoShapes[(((uint16_t)internalData.lfoShape)*Lfo_Shape_Count) >> 8]);
                return;
            case 20:
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
                if(showForStep && HasLockForStep(step, pattern, 47, valB))
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
            case 5:
                sprintf(strA, "Vel");
                sprintf(strB, "Hold");
                lockA = CheckLockAndSetDisplay(showForStep, step, pattern, Timbre, internalData.timbre, pA);
                if(showForStep && HasLockForStep(step, pattern, Color, valB))
                {
                    sprintf(pB, "%s", midiHoldLabels[valB>>4]);
                    lockB = true;
                }
                else
                    sprintf(pB, "%s", midiHoldLabels[internalData.color>>4]);
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


int8_t VoiceData::GetOctave()
{
    return ((int8_t)(internalData.octave/51))-2;
}

/* PARAMETER LOCK BEHAVIOR */
void VoiceData::StoreParamLock(uint8_t param, uint8_t step, uint8_t pattern, uint8_t value)
{
    ParamLock *lock;
    // if we find a lock for this step / pattern / param group, we update the value and return
    if(GetLockForStep(&lock, step, pattern, param))
    {
        lock->value = value;
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
        lock->next = locksForPatternStep[pattern][step];
        locksForPatternStep[pattern][step] = lockPool.GetLockPosition(lock);
        return;
    }
    printf("failed to add param lock\n");
}
void VoiceData::ClearParameterLocks(uint8_t pattern)
{
    for (int s = 0; s < 64; s++)
    {
        ParamLock* lock = lockPool.GetLock(locksForPatternStep[pattern][s]);
        while(lockPool.IsValidLock(lock))
        {
            ParamLock* nextLock = lockPool.GetLock(lock->next);
            lockPool.ReturnLockToPool(lock);
            lock = nextLock;
        }
        locksForPatternStep[pattern][s] = ParamLockPool::InvalidLockPosition();
    }
}
void VoiceData::RemoveLocksForStep(uint8_t pattern, uint8_t step)
{
    ParamLock* lock = lockPool.GetLock(locksForPatternStep[pattern][step]);
    while(lockPool.IsValidLock(lock))
    {
        ParamLock* nextLock = lockPool.GetLock(lock->next);
        lockPool.ReturnLockToPool(lock);
        lock = nextLock;
    }
    locksForPatternStep[pattern][step] = ParamLockPool::InvalidLockPosition();
}
void VoiceData::CopyParameterLocks(uint8_t fromPattern, uint8_t toPattern)
{
    for (int s = 0; s < 64; s++)
    {
        ParamLock* lock = lockPool.GetLock(locksForPatternStep[fromPattern][s]);
        while(lockPool.IsValidLock(lock))
        {
            StoreParamLock(lock->param, s, toPattern, lock->value);
            lock = lockPool.GetLock(lock->next);
        }
    }
}
bool VoiceData::HasLockForStep(uint8_t step, uint8_t pattern, uint8_t param, uint8_t &value)
{
    // because we use the highbit to signal if we are checking a specific step in the callsite, this must be stripped here
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
    return lockPool.IsValidLock(locksForPatternStep[pattern][step]);
}
bool VoiceData::GetLockForStep(ParamLock **lockOut, uint8_t step, uint8_t pattern, uint8_t param)
{
    ParamLock* lock = lockPool.GetLock(locksForPatternStep[pattern][step]);
    while(lockPool.IsValidLock(lock))
    {
        if(lock->param == param)
        {
            *lockOut = lock;
            return true;
        }
        lock = lockPool.GetLock(lock->next);
    }
    return false;
}
uint16_t VoiceData::CountLocksForPattern(uint8_t pattern)
{
    uint16_t res = 0;
    for (int s = 0; s < 64; s++)
    {
        ParamLock* lock = lockPool.GetLock(locksForPatternStep[pattern][s]);
        while(lockPool.IsValidLock(lock))
        {
            res++;
            lock = lockPool.GetLock(lock->next);
        }
    }
    return res;
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
                ParamLock *lock = voiceData.lockPool.GetLock(voiceData.locksForPatternStep[p][0]);
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