#include "Instrument.h"

static const uint16_t kPitchTableStart = 128 * 128;
static const uint16_t kOctave = 12 * 128;

void Instrument::Init(Midi *_midi)
{
    osc.Init();
    currentSegment = ENV_SEGMENT_COMPLETE;
    osc.set_pitch(60<<7);
    osc.set_shape(MACRO_OSC_SHAPE_WAVE_MAP);
    osc.set_parameters(0,0);
    envPhase = 0;
    svf.Init();
    midi = _midi;
    phase_ = 0;    
    
    sampleOffset = 0;
    // hard code the sample lengths
    // for(int i=0;i<16;i++)
    // {
    //     sampleLength[i] = fullSampleLength/16;
    //     sampleStart[i] = sampleLength[i]*i;
    // }
    env.Init();
    env.Trigger(ADSR_ENV_SEGMENT_DEAD);
}

const char *macroparams[16] = { 
    "Timb", "Filt", "VlPn", "Ptch",
    "Env", "EnvF", "EnvP", "EnvT",
    "?", "?", "?", "?",
    "?", "?", "FLTO", "TYPE"
};

const char *sampleparams[16] = { 
    "I/O", "FILT", "VlPn", "Ptch",
    "Env", "EnvF", "EnvP", "EnvT",
    "?", "?", "?", "?",
    "?", "?", "FLTO", "TYPE"
};


// Values are defined in number of samples
void Instrument::SetAHD(uint32_t attackTime, uint32_t holdTime, uint32_t decayTime)
{
    this->attackTime = attackTime;
    this->holdTime = holdTime;
    this->decayTime = decayTime;
    env.Update(0x2b, 0x7f);
}

uint32_t Instrument::ComputePhaseIncrement(int16_t midi_pitch) {
  if (midi_pitch >= kPitchTableStart) {
    midi_pitch = kPitchTableStart - 1;
  }
  
  int32_t ref_pitch = midi_pitch;
  ref_pitch -= kPitchTableStart;
  
  size_t num_shifts = 0;
  while (ref_pitch < 0) {
    ref_pitch += kOctave;
    ++num_shifts;
  }
  
  uint32_t a = lut_sample_increments[ref_pitch >> 4];
  uint32_t b = lut_sample_increments[(ref_pitch >> 4) + 1];
  uint32_t phase_increment = a + \
      (static_cast<int32_t>(b - a) * (ref_pitch & 0xf) >> 4);
  phase_increment >>= num_shifts;
  return phase_increment;
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
        //return;
        for(int i=0;i<SAMPLES_PER_BUFFER;i++)
        {
            int16_t wave = 0;
            // // skip the first 4 bytes, since that contains the sample length + encoding
            // lfs_file_seek(GetLFS(), &sinefile, sampleOffset+4, LFS_SEEK_SET);
            file_read(&wave, sampleOffset*2, 2);
            //lfs_file_read(GetLFS(), &sinefile, &wave, 1);
            
            phase_ += phase_increment;
            sampleOffset+=(phase_>>25);
            phase_-=(phase_&(0xff<<25));
            // should interpolate the sample position
            // *buffer++ = Interpolate824(wave[0], phase_ >> 1);

            // sampleOffset+=2;
            while(sampleOffset > fullSampleLength-1)
            {
                sampleOffset -=fullSampleLength;
            }
            buffer[i] = wave;
            //mult_q15(buffer[i], volume);
        }
        RenderGlobal(sync, buffer, size);
        return;
    }
    osc.Render(sync, buffer, size);
    RenderGlobal(sync, buffer, size);
}
void Instrument::RenderGlobal(const uint8_t* sync, int16_t* buffer, size_t size)
{
    for(int i=0;i<SAMPLES_PER_BUFFER;i++)
    {
        q15_t envval = env.Render() >> 1;

        uint32_t mult = 0;
        uint32_t phase = 0;
        if(enable_env)
        {
            buffer[i] = mult_q15(buffer[i], envval);
        }
        //buffer[i] = svf.Process(buffer[i]);
        buffer[i] = mult_q15(buffer[i], volume);
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
    int note = keyToMidi[key]+12*octave;
    if(instrumentType == INSTRUMENT_SAMPLE)
    {
        playingSlice = key;
        sampleOffset = 0;
        phase_increment = ComputePhaseIncrement(note<<7);
        env.Trigger(ADSR_ENV_SEGMENT_ATTACK);
    }
    if(instrumentType == INSTRUMENT_MACRO)
    {
        enable_env = true;
        osc.set_pitch(note<<7);
        osc.Strike();
        env.Trigger(ADSR_ENV_SEGMENT_ATTACK);
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
        // 3
        case 6:
            octave = ((int8_t)(val/51))-2;
            break;
            

        case 8:
            this->attackTime = val;
            env.Update(this->attackTime>>1, this->decayTime>>1);
            break;
        case 9:
            this->decayTime = val;
            env.Update(this->attackTime>>1, this->decayTime>>1);
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
                // the range on these appear to be 0-127
                svf.set_frequency((val>>1) << 7);
                cutoff = val;
                break;
            case 3:
                // the range on these appear to be 0-127
                svf.set_resonance((val>>1) << 7);
                resonance = val;
                break;
            
            // 2 vol / pan
            case 4:
                volume = val<<7;
                break;
            case 5:
                break;
            // 3
            case 6:
                octave = ((int8_t)(val/51))-2;
                break;
                

            // 4 vol / pan
            case 8:
                this->attackTime = val;
                env.Update(this->attackTime>>1, this->decayTime>>1);
                break;
            case 9:
                this->decayTime = val;
                env.Update(this->attackTime>>1, this->decayTime>>1);
                break;

            case 10:
                this->envTimbre = (int8_t)((int16_t)val)-0x7f;
                break;
            case 11:
                this->envColor = (int8_t)((int16_t)val)-0x7f;
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
    int16_t vala = 0;
    int16_t valb = 0;
    
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
        case 3:
            vala = octave;
            valb = 0x7f;
            break;
        case 4:
            vala = this->attackTime;
            valb = this->decayTime;
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
            case 3:
                vala = octave;
                valb = 0x7f;
                break;
            case 4:
                vala = this->attackTime;
                valb = this->decayTime;
                break;
            case 5:
                vala = this->envTimbre;
                valb = this->envColor;
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
