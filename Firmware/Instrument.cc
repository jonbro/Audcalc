#include "Instrument.h"
#include "AudioSampleSecondbloop.h"

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
    for(int i=0;i<16;i++)
    {
        playingMidiNotes[i] = -1;
    }
    currentMidiNote = 0;
    env.Init();
    env.Trigger(ADSR_ENV_SEGMENT_DEAD);
    env2.Init();
    env2.Trigger(ADSR_ENV_SEGMENT_DEAD);
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
        if(!file || filesize == 0 || sampleSegment == SMP_COMPLETE)
        {
            memset(buffer, 0, SAMPLES_PER_BUFFER*2);
            return;
        }
        // double check the file length
        if(!file->initialized || sampleOffset*2 > file->filesize)
            sampleSegment = SMP_COMPLETE;
        if(sampleSegment == SMP_COMPLETE)
            return;

        // this is too slow to do in the loop - which unfortunately makes
        // handling higher octaves quite difficult
        // one thing I could do is add a special read mode to change the stepping in the ffs_read to match the phase increment - i.e. read 1, skip 1 or something like this?
        int16_t wave[128*5] = {0}; // lets just get 5 octaves worth? thats only 2k, we should be able to spare it, lol
        ffs_seek(GetFilesystem(), file, sampleOffset*2);
        // clamp the read amount to the maximum filesize
        // todo: add a ffs_read that takes a per read stride, so I can fill this more easily, and don't need to load more than necessary
        uint16_t readAmount = (sampleOffset*2+2*128*5)>filesize?filesize-sampleOffset*2:2*128*5;
        ffs_read(GetFilesystem(), file, wave, readAmount);
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
            phase_-=(phase_&(0xfe<<24));

            buffer[i] = wave[sampleOffset-startingSampleOffset];
            
            if(microFade < 0xfa)
            {
                buffer[i] = (((int32_t)buffer[i]*microFade)>>8) + (((int32_t)(0xff-microFade)*(int32_t)lastSample)>>8);
                microFade += 0xa;
            }
            else
            {
                lastSample = buffer[i];
            }
        }
        svf.set_frequency(add_q15(mainCutoff, lastenv2val>>1));
        // I think this might be better for cache locality?
        for(int i=0;i<SAMPLES_PER_BUFFER;i++)
        {
            buffer[i] = svf.Process(buffer[i]);
        }
        for(int i=0;i<SAMPLES_PER_BUFFER;i++)
        {
            lastenv2val = env2.Render();
            q15_t envval = env.Render() >> 1;
            buffer[i] = mult_q15(buffer[i], mult_q15(volume, envval));
        }
        return;
    }

    // could alternately use tick rate stuff. might be way better
    // fast 4bf0000
    // slow fffff
    // holy hell what the fuck is this code, seriously :0
    // should like, maybe, move this into some class or something :P 
    lfo_phase += ((lfo_rate*(0x8bf0000-0xfffff))>>6)+0xfffff;
    q15_t lfo = mult_q15(Interpolate824(wav_sine, lfo_phase), lfo_depth);

    q15_t param1_withMods = param1Base;
    param1_withMods = add_q15(lfo, param1Base);
    q15_t param2_withMods = param2Base;
    q15_t cutoffWithMods = mainCutoff;
    for (size_t i = 0; i < 2; i++)
    {
        EnvTargets t = i==0?env1Target:env2Target;
        q15_t depth = i==0?env1Depth:env2Depth;
        ADSREnvelope* e = i==0?&env:&env2;
        switch (t)
        {
            case Target_Volume:
                break;
            case Target_Timbre:
                param1_withMods = add_q15(param1_withMods, mult_q15(depth, e->valueLin()>>1));
                break;
            case Target_Color:
                param2_withMods = add_q15(param2_withMods, mult_q15(depth, e->valueLin()>>1));
                break;
            case Target_Cutoff:
                cutoffWithMods = add_q15(cutoffWithMods, mult_q15(depth, e->valueLin()>>1));
                break;
            default:
                break;
        }
    }
    
    if(param1_withMods < 0)
    {
        param1_withMods = 0;
    }
    if(param2_withMods < 0)
    {
        param2_withMods = 0;
    }
    if(cutoffWithMods < 0)
    {
        cutoffWithMods = 0;
    }
    pWithMods = cutoffWithMods;
    osc.set_parameter_1(param1_withMods);
    osc.set_parameter_2(param2_withMods);
    osc.Render(sync, buffer, size);
    svf.set_frequency(cutoffWithMods);
    RenderGlobal(sync, buffer, size);
}
void Instrument::RenderGlobal(const uint8_t* sync, int16_t* buffer, size_t size)
{
    for(int i=0;i<SAMPLES_PER_BUFFER;i++)
    {
        lastenv2val = env2.Render();
        lastenvval = env.Render();
        q15_t envval = lastenvval >> 1;
        if(enable_env)
        {
            buffer[i] = mult_q15(buffer[i], envval);
        }
        buffer[i] = svf.Process(buffer[i]);
        
        // not sure why distortion isn't working, revisit at some point
        // int32_t shiftedSample = mult_q15(buffer[i], f32_to_q15(0.5));// + distortionAmount; // I think this is the overdrive amount? 
        // buffer[i] = Interpolate88(ws_moderate_overdrive, shiftedSample);
        // buffer[i] = mult_q15(buffer[i], f32_to_q15(1));

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
    reverbSend = voiceData.GetParamValue(ReverbSend, lastPressedKey, playingStep, playingPattern);
    volume = ((q15_t)voiceData.GetParamValue(Volume, lastPressedKey, playingStep, playingPattern))<<7;
    if(instrumentType == INSTRUMENT_SAMPLE || instrumentType == INSTRUMENT_MACRO)
    {
        env2.Update(voiceData.GetParamValue(AttackTime2, lastPressedKey, playingStep, playingPattern)>>1, voiceData.GetParamValue(DecayTime2, lastPressedKey, playingStep, playingPattern)>>1);
        lfo_depth = (voiceData.GetParamValue(LFODepth, lastPressedKey, playingStep, playingPattern)>>1)<<7;
        lfo_rate = (voiceData.GetParamValue(LFORate, lastPressedKey, playingStep, playingPattern)>>1)<<7;
        mainCutoff = (voiceData.GetParamValue(Cutoff, lastPressedKey, playingStep, playingPattern)>>1) << 7;
        svf.set_frequency(add_q15(mainCutoff, lastenv2val>>1));
        svf.set_resonance((voiceData.GetParamValue(Resonance, lastPressedKey, playingStep, playingPattern)>>1) << 7);
        env1Target = (EnvTargets)((((uint16_t)voiceData.GetParamValue(Env1Target, lastPressedKey, playingStep, playingPattern))*5)>>8);
        env1Depth = (voiceData.GetParamValue(Env1Depth, lastPressedKey, playingStep, playingPattern))<<7;
        env2Target = (EnvTargets)((((uint16_t)voiceData.GetParamValue(Env2Target, lastPressedKey, playingStep, playingPattern))*5)>>8);
        env2Depth = (voiceData.GetParamValue(Env2Depth, lastPressedKey, playingStep, playingPattern))<<7;
        distortionAmount = (voiceData.GetParamValue(DelaySend, lastPressedKey, playingStep, playingPattern))<<7;
    }
    if(voiceData.GetInstrumentType() == INSTRUMENT_MACRO)
    {
        env.Update(voiceData.GetParamValue(AttackTime, lastPressedKey, playingStep, playingPattern)>>1, voiceData.GetParamValue(DecayTime, lastPressedKey, playingStep, playingPattern)>>1);
        // copy parameters from voice
        param1Base = voiceData.GetParamValue(Timbre, lastPressedKey, playingStep, playingPattern) << 7;
        param2Base = voiceData.GetParamValue(Color, lastPressedKey, playingStep, playingPattern) << 7;
    }
    if(voiceData.GetInstrumentType() == INSTRUMENT_SAMPLE)
    {
        env.Update(voiceData.GetParamValue(AttackTime, lastPressedKey, playingStep, playingPattern)>>1, voiceData.GetParamValue(DecayTime, lastPressedKey, playingStep, playingPattern)>>1);
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
        microFade = 0;
        UpdateVoiceData(voiceData);
        playingSlice = key;
        file = voiceData.GetFile();
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
        if(voiceData.GetSampler() == SAMPLE_PLAYER_SEQL)
        {
            sampleOffset = (voiceData.sampleStart[0]) * (filesize>>9);
            sampleEnd = sampleOffset + (voiceData.sampleLength[0]) * (filesize>>9);
            if(sampleEnd*2 > filesize - 1)
            {
                sampleEnd = filesize/2 -1;
            }
            // compute the phase increment so it loops in 16 beats
            // first we need to know the number of samples in the loop?
            uint32_t tempoPhaseIncrement = lut_tempo_phase_increment[globalParams->bpm];
            // 24ppq
            tempoPhaseIncrement = tempoPhaseIncrement + (tempoPhaseIncrement>>1);
            uint32_t tempoPhase = 0;
            uint32_t sampleCount = 0;
            uint8_t beatCount = 0;
            while(beatCount < 6*16)
            {
                tempoPhase += tempoPhaseIncrement;
                sampleCount+=SAMPLES_PER_BUFFER;
                if((tempoPhase >> 31) > 0)
                {
                    tempoPhase &= 0x7fffffff;
                    beatCount++;
                }
            }
            sampleCount-=256;
            phase_increment = (uint32_t)(((uint64_t)(sampleEnd-sampleOffset)<<31)/((uint64_t)sampleCount)); // q32?
            phase_increment = phase_increment>>6; // q25 our overflow value
            // calculate the actual sample offset based on the pressed key
            sampleOffset = (sampleEnd-sampleOffset)/16*key+sampleOffset;//((uint64_t)sampleCount)*((uint32_t)(key))/(uint32_t)16+sampleOffset;
        }
        else
        {
            sampleOffset = (voiceData.sampleStart[key]) * (filesize>>9);
            sampleEnd = sampleOffset + (voiceData.sampleLength[key]) * (filesize>>9);
            if(sampleEnd*2 > filesize - 1)
            {
                sampleEnd = filesize/2 -1;
            }
            phase_increment = ComputePhaseIncrement(note<<7);
        }
        // printf("sample points in %i out %i filesize %i\n", sampleOffset, sampleEnd, filesize);
        // just hardcode note to 69 for now
        phase_ = 0;
        sampleSegment = SMP_PLAYING;
        env.Trigger(ADSR_ENV_SEGMENT_ATTACK);
        env2.Trigger(ADSR_ENV_SEGMENT_ATTACK);
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
        env2.Trigger(ADSR_ENV_SEGMENT_ATTACK);
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
    else if(voiceData.GetInstrumentType() == INSTRUMENT_MIDI)
    {
        uint8_t notesToAllowPlay = (voiceData.GetParamValue(PlayingMidiNotes, lastPressedKey, playingStep, playingPattern)>>4);
        // printf("notes to allow play %i\n", notesToAllowPlay);
        for(int i=0;i<16-notesToAllowPlay;i++)
        {
            uint8_t idx = (i+currentMidiNote+1)%16;
            if(playingMidiNotes[idx] > 0)
            {
                // printf("stopping midi note %i\n", playingMidiNotes[idx]);
                midi->NoteOff(voiceData.GetMidiChannel(), playingMidiNotes[idx]);
                playingMidiNotes[idx] = -1;
            }
        }
        if(playingMidiNotes[currentMidiNote] > 0)
            midi->NoteOff(voiceData.GetMidiChannel(), playingMidiNotes[currentMidiNote]);
        playingMidiNotes[currentMidiNote] = note-12;
        // printf("starting midi note %i\n", playingMidiNotes[currentMidiNote]);
        midi->NoteOn(voiceData.GetMidiChannel(), note-12, voiceData.GetParamValue(Timbre, lastPressedKey, playingStep, playingPattern)>>1);
        currentMidiNote = (currentMidiNote+1)%16;
    } 
    lastNoteOnPitch = note;
}