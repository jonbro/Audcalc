#include "Instrument.h"

void Instrument::Init(Midi *_midi)
{
    osc.Init();
    currentSegment = ENV_SEGMENT_COMPLETE;
    osc.set_pitch(54<<7);
    envPhase = 0;
    svf.Init();
    midi = _midi;
    fullSampleLength = AudioSampleAmen_165[0] & 0xffffff;
    sample = (int16_t*)&AudioSampleAmen_165[1];
    sampleOffset = 0;
    // hard code the sample lengths
    for(int i=0;i<16;i++)
    {
        sampleLength[i] = fullSampleLength/16;
        sampleStart[i] = sampleLength[i]*i;
    }
}

const char *macroparams[16] = { 
    "TIMB", "FILT", "VlPn", "?"
    "?", "?", "?", "?"
    "?", "?", "?", "?"
    "?", "?", "FLTO", "TYPE"
};

const char *sampleparams[16] = { 
    "I/O", "FILT", "VlPn", "?"
    "?", "?", "?", "?"
    "?", "?", "?", "?"
    "?", "?", "FLTO", "TYPE"
};


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
    if(instrumentType == INSTRUMENT_SAMPLE)
    {
        if(playingSlice >= 0)
        {
            for(int i=0;i<SAMPLES_PER_BUFFER;i++)
            {
                // this doesn't run in stereo, so just copy every other sample here
                buffer[i] = sample[sampleOffset+sampleStart[playingSlice]];
                sampleOffset++;
                if(sampleOffset > sampleLength[playingSlice] || sampleOffset > fullSampleLength)
                {
                    sampleOffset = 0;
                    playingSlice = -1;
                }
                buffer[i] = mult_q15(buffer[i], volume);
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
        buffer[i] = svf.Process(buffer[i]);
        if(enable_env)
        {
            buffer[i] = mult_q15(buffer[i], env);
        }
        mult_q15(buffer[i], volume);
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

void Instrument::NoteOn(int16_t key, bool livePlay)
{
    // used for editing values for specific keys
    if(livePlay)
    {
        lastPressedKey = key;
    }
    int note = keyToMidi[key];
    if(instrumentType == INSTRUMENT_SAMPLE)
    {
        playingSlice = key;
        sampleOffset = 0;
    }
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
            midi->NoteOff(lastNoteOnPitch-24);
        midi->NoteOn(note-24);
    } 
    lastNoteOnPitch = note;
}

// parameters are opaque to the groovebox
// just use the key id for the parameter, then set internally
// parameter values are doubled so we just get one at a time (0-31)
void Instrument::SetParameter(uint8_t param, uint8_t val)
{
    if(instrumentType == INSTRUMENT_SAMPLE)
    {
        switch (param)
        {
        // sample in point
        case 0:
            sampleStart[lastPressedKey] = val<<7;
            break;

        // sample out point
        case 1:
            sampleLength[lastPressedKey] = val<<7;
            break;
        
        // 2 vol / pan
        case 4:
            volume = val<<7;
            break;
        case 5:
            break;
            

        default:
            break;
        }
    }
    else if(instrumentType == INSTRUMENT_MACRO)
    {
        switch (param)
        {
            // 0 timb
            case 0:
                timbre = val;
                osc.set_parameter_1(val << 7);
                break;
            case 1:
                color = val;
                osc.set_parameter_2(val << 7);
                break;
            
            // 1 filt
            case 2:
                svf.set_frequency(val << 7);
                cutoff = val;
                break;
            case 3:
                svf.set_resonance(val << 7);
                resonance = val;
                break;
            // 2 vol / pan

            case 4:
                volume = val<<7;
                break;
            case 5:
                break;
            
            
            case 30:
                shape = (MacroOscillatorShape)((((uint16_t)val)*46) >> 8);
                osc.set_shape(shape);
                break;
            default:
            break;
        }
    }
}

void Instrument::GetParamString(uint8_t param, char *str)
{
    uint8_t vala = 0;
    uint8_t valb = 0;
    
    if(instrumentType == INSTRUMENT_SAMPLE)
    {
        switch (param)
        {
        // sample in point
        case 0:
            vala = sampleStart[lastPressedKey] >> 7;
            valb = sampleLength[lastPressedKey] >> 7;
            break;

        // volume / pan
        case 2:
            vala = volume >> 7;
            valb = 0x7f;
            break;

        default:
            break;
        }
        sprintf(str, "%s %i %i", sampleparams[param], vala, valb);
    }
    else if(instrumentType == INSTRUMENT_MACRO)
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
                vala = volume >> 7;
                valb = 0x7f;
                break;
            case 15:
                sprintf(str, "TYPE MACRO %s", algo_values[shape]);
                return;
                break;
            default:
                break;
        }
        sprintf(str, "%s %i %i", macroparams[param], vala, valb);
    }
}
