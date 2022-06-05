#include "Instrument.h"

static const uint16_t kPitchTableStart = 128 * 128;
static const uint16_t kOctave = 12 * 128;

void Instrument::Init(Midi *_midi, int16_t *temp_buffer)
{
    osc.Init(temp_buffer);
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
    "lop", "?", "?", "?",
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
        uint32_t filesize = ffs_file_size(GetFilesystem(), file);
        if(filesize == 0 || sampleSegment == SMP_COMPLETE)
        {
            memset(buffer, 0, SAMPLES_PER_BUFFER*2);
            return;
        }

        // this is too slow to do in the loop - which unfortunately makes
        // handling higher octaves quite difficult
        // one thing I could do is add a special read mode to change the stepping in the ffs_read to match the phase increment - i.e. read 1, skip 1 or something like this?
        int16_t wave[128*5]; // lets just get 5 octaves worth? thats only 2k, we should be able to spare it, lol
        ffs_seek(GetFilesystem(), file, sampleOffset*2);
        ffs_read(GetFilesystem(), file, wave, 2*128*5);
        uint32_t startingSampleOffset = sampleOffset;
        for(int i=0;i<SAMPLES_PER_BUFFER;i++)
        {
            if(sampleOffset > sampleEnd - 1)
            {
                if(loopMode == INSTRUMENT_LOOPMODE_NONE)
                {
                    sampleSegment = SMP_COMPLETE;
                    return;
                }
                else
                {
                    while(sampleOffset*2 > filesize - 1)
                    {
                        sampleOffset -= filesize/2;
                    }
                }
            }
            // int16_t wave;
            // ffs_seek(GetFilesystem(), file, sampleOffset*2);
            // ffs_read(GetFilesystem(), file, wave, 2);

            phase_ += phase_increment;
            sampleOffset+=(phase_>>25);
            phase_-=(phase_&(0xff<<25));

            buffer[i] = wave[sampleOffset-startingSampleOffset];
            q15_t envval = env.Render() >> 1;
            if(enable_env)
            {
                buffer[i] = mult_q15(buffer[i], envval);
            }
            buffer[i] = svf.Process(buffer[i]);
            buffer[i] = mult_q15(buffer[i], volume);
        }
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
        if(enable_env)
        {
            buffer[i] = mult_q15(buffer[i], envval);
        }
        buffer[i] = svf.Process(buffer[i]);
        buffer[i] = mult_q15(buffer[i], volume);
    }
}
bool Instrument::IsPlaying()
{
    if(env.segment() == ADSR_ENV_SEGMENT_DEAD)
        return false;
    if(instrumentType == INSTRUMENT_SAMPLE && sampleSegment == SMP_COMPLETE)
        return false;
    return true;
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

void Instrument::UpdateVoiceData(VoiceData &voiceData)
{
    delaySend = voiceData.GetParamValue(DelaySend, lastPressedKey, playingStep, playingPattern);
    volume = ((q15_t)voiceData.GetParamValue(Volume, lastPressedKey, playingStep, playingPattern))<<7;
    if(instrumentType == INSTRUMENT_SAMPLE || instrumentType == INSTRUMENT_MACRO)
    {
        env.Update(voiceData.GetParamValue(AttackTime, lastPressedKey, playingStep, playingPattern)>>1, voiceData.GetParamValue(DecayTime, lastPressedKey, playingStep, playingPattern)>>1);
        svf.set_frequency((voiceData.GetParamValue(Cutoff, lastPressedKey, playingStep, playingPattern)>>1) << 7);
        svf.set_resonance((voiceData.GetParamValue(Resonance, lastPressedKey, playingStep, playingPattern)>>1) << 7);
    }
    if(voiceData.GetInstrumentType() == INSTRUMENT_MACRO)
    {
        // copy parameters from voice
        osc.set_parameter_1(voiceData.GetParamValue(Timbre, lastPressedKey, playingStep, playingPattern) << 7);
        osc.set_parameter_2(voiceData.GetParamValue(Color, lastPressedKey, playingStep, playingPattern) << 7);
    }
    if(voiceData.GetInstrumentType() == INSTRUMENT_SAMPLE)
    {
    }
}
void Instrument::NoteOn(int16_t key, int16_t midinote, uint8_t step, uint8_t pattern, bool livePlay, VoiceData &voiceData)
{
    // used for editing values for specific keys
    if(livePlay)
    {
        lastPressedKey = key;
    }
    playingStep = step;
    playingPattern = pattern;
    int note = keyToMidi[key]+12*voiceData.GetOctave();
    if(globalParams->chromatic > 0x7f)
    {
        uint8_t x = key%4;
        uint8_t y = key/4;
        y = 3-y;
        int16_t inverseKey = x+y*4;
        note = (inverseKey+50)+16*voiceData.GetOctave();
    }
    if(midinote >= 0)
    {
        note = midinote;
    }
    if(voiceData.GetInstrumentType() == INSTRUMENT_SAMPLE)
    {
        UpdateVoiceData(voiceData);
        playingSlice = key;
        file = voiceData.file;
        instrumentType = voiceData.GetInstrumentType();
        uint32_t filesize = ffs_file_size(GetFilesystem(), file);
        if(voiceData.GetSampler() != SAMPLE_PLAYER_PITCH)
        {
            note = 69; // need to figure out a way to fine tune for non-pitched samples
        }
        else
        {
            // clamp it - what kind of wild person needs so many octaves anyways.
            note = note>69+12*4?69+12*4:note;
            key = 0;
        }
        sampleOffset = (voiceData.sampleStart[key]) * (filesize>>9);
        sampleEnd = sampleOffset + (voiceData.sampleLength[key]) * (filesize>>9);
        if(sampleEnd*2 > filesize - 1)
        {
            sampleEnd = filesize/2 -1;
        }
        printf("sample points in %i out %i filesize %i\n", sampleOffset, sampleEnd, filesize);
        // just hardcode note to 69 for now
        phase_increment = ComputePhaseIncrement(note<<7);
        sampleSegment = SMP_PLAYING;
        env.Trigger(ADSR_ENV_SEGMENT_ATTACK);
    }
    if(voiceData.GetInstrumentType() == INSTRUMENT_MACRO)
    {
        // copy parameters from voice
        UpdateVoiceData(voiceData);
        // only set shape on initial trigger
        osc.set_shape(voiceData.GetShape());
        enable_env = true;
        osc.set_pitch(note<<7);
        instrumentType = voiceData.GetInstrumentType();
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
    else if(instrumentType == INSTRUMENT_MIDI)
    {
        if(lastNoteOnPitch >= 0)
            midi->NoteOff(lastNoteOnPitch-24);
        midi->NoteOn(note-24);
    } 
    lastNoteOnPitch = note;
}