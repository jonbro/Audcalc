#include "Instrument.h"
#include "AudioSampleSecondbloop.h"
#include "GlobalDefines.h"
#include "GrooveBox.h"

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
    // svf.set_mode(SVF_MODE_HP);
    // svf.set_punch(0);
    midi = _midi;
    phase_ = 0;    
    sampleOffset = 0;
    for(int i=0;i<16;i++)
    {
        midiNoteStates[i].note = -1;
        midiNoteStates[i].ticksRemaining = -1;
    }
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
q15_t Instrument::GetLfoState()
{
    return mult_q15(Interpolate824(wav_sine, lfo_phase), lfo_depth);
}
void __not_in_flash_func(Instrument::Render)(const uint8_t* sync, int16_t* buffer, size_t size)
{
    if(instrumentType == INSTRUMENT_MIDI)
    {
        for(int i=0;i<SAMPLES_PER_BLOCK;i++)
        {
            noteOffSchedule--;
            if(noteOffSchedule == 0)
            {
                //midi->NoteOff();
            }
        }
        return;
    }
    
    q15_t param1_withMods = param1Base;
    q15_t param2_withMods = param2Base;
    q15_t cutoffWithMods = mainCutoff;
    q15_t pitchWithMods = pitch;
    panWithMods = panning;
    q15_t lfo = GetLfoState();
    switch(lfo1Target)
    {
        case Lfo_Target_Volume:
            break;
        case Lfo_Target_Timbre:
            param1_withMods = add_q15(param1_withMods, lfo);
            break;
        case Lfo_Target_Color:
            param2_withMods = add_q15(param2_withMods, lfo);
            break;
        case Lfo_Target_Cutoff:
            cutoffWithMods = add_q15(cutoffWithMods, lfo);
            break;
        case Lfo_Target_Pitch:
            pitchWithMods = pitchWithMods+(lfo>>4);
            break;
        case Lfo_Target_Pan:
            panWithMods = add_q15(panWithMods, lfo);
            break;
        default:
            break;
    }

    // could alternately use tick rate stuff. might be way better
    // fast 4bf0000
    // slow fffff
    // holy hell what the fuck is this code, seriously :0
    // should like, maybe, move this into some class or something :P 
    uint32_t lfoPhaseIncrement = lut_tempo_phase_increment[songData->GetBpm()];
    // 24ppq
    lfoPhaseIncrement = lfoPhaseIncrement + (lfoPhaseIncrement>>1);
    lfoPhaseIncrement = lfoPhaseIncrement/((lfo_rate>>4)+2);
    //int32_t phaseOff = ((uint32_t)(0xffff-Interpolate824(lut_env_expo, (0x7fff-lfo_rate)<<16))<<13)-0x7fffff; 
    lfo_phase += lfoPhaseIncrement;//((0xffff-Interpolate824(lut_env_expo, (0x7fff-lfo_rate)<<16))<<13)-0x7ffff; //((lfo_rate*(0xabf0000-0xfffff))>>4)+0xfffff;

    for (size_t i = 0; i < 2; i++)
    {
        EnvTargets t = i==0?env1Target:env2Target;
        int16_t depth = i==0?env1Depth:env2Depth;
        depth = (depth-0x4000)*2; // center at zero
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
            case Target_Pitch:
                pitchWithMods = pitchWithMods+(mult_q15(depth, e->value()>>1)>>4);
                break;
            case Target_Pan:
                panWithMods = add_q15(panWithMods, mult_q15(depth, e->valueLin()>>1));
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
    svf.set_frequency(cutoffWithMods);

    if(instrumentType == INSTRUMENT_SAMPLE)
    {
        uint32_t filesize = ffs_file_size(GetFilesystem(), file);
        if(!file || filesize == 0 || sampleSegment == SMP_COMPLETE)
        {
            memset(buffer, 0, SAMPLES_PER_BLOCK*2);
            return;
        }
        // double check the file length
        if(!file->initialized || sampleOffset*2 > file->filesize)
            sampleSegment = SMP_COMPLETE;
        if(sampleSegment == SMP_COMPLETE)
        {
            for(int i=0;i<SAMPLES_PER_BLOCK;i++)
            {
                buffer[i] = 0;
            }
            return;
        }

        // this is too slow to do in the loop - which unfortunately makes
        // handling higher octaves quite difficult
        // one thing I could do is add a special read mode to change the stepping in the ffs_read to match the phase increment - i.e. read 1, skip 1 or something like this?
        // int16_t wave[128*5] = {0}; // lets just get 5 octaves worth? thats only 2k, we should be able to spare it, lol
        // ffs_seek(GetFilesystem(), file, sampleOffset*2);
        // // clamp the read amount to the maximum filesize
        // // todo: add a ffs_read that takes a per read stride, so I can fill this more easily, and don't need to load more than necessary
        // uint16_t readAmount = (sampleOffset*2+2*128*5)>filesize?filesize-sampleOffset*2:2*128*5;
        // ffs_read(GetFilesystem(), file, wave, readAmount);
        uint32_t startingSampleOffset = sampleOffset;
        for(int i=0;i<SAMPLES_PER_BLOCK;i++)
        {
            if(sampleOffset > sampleEnd - 1)
            {
                if(loopMode == INSTRUMENT_LOOPMODE_NONE)
                {
                    sampleSegment = SMP_COMPLETE;
                    buffer[i] = 0;
                    continue;
                }
                else
                {
                    while(sampleOffset*2 > filesize - 1)
                    {
                        sampleOffset -= filesize/2;
                    }
                }
            }
            int16_t wave;
            ffs_seek(GetFilesystem(), file, sampleOffset*2);
            ffs_read(GetFilesystem(), file, &wave, 2);

            phase_ += phase_increment;
            sampleOffset+=(phase_>>25);
            phase_-=(phase_&(0xfe<<24));

            buffer[i] = wave;//[sampleOffset-startingSampleOffset];
            
            if(microFade < 0xfa)
            {
                buffer[i] = (((int32_t)buffer[i]*microFade)>>8);
                microFade += 0xa;
            }
            // else
            // {
            //     lastSample = buffer[i];
            // }
        }
    }
    else if(instrumentType == INSTRUMENT_MACRO)
    {
        osc.set_pitch(pitchWithMods);
        osc.set_parameter_1(param1_withMods);
        osc.set_parameter_2(param2_withMods);
        osc.Render(sync, buffer, SAMPLES_PER_BLOCK);
    }
    RenderGlobal(sync, buffer, size);
}
void Instrument::RenderGlobal(const uint8_t* sync, int16_t* buffer, size_t size)
{
    for(int i=0;i<SAMPLES_PER_BLOCK;i++)
    {
        lastenv2val = env2.Render();
        lastenvval = env.Render();
        q15_t envval = lastenvval >> 1;
        if(enable_env)
        {
            buffer[i] = mult_q15(buffer[i], envval);
        }
        // buffer[i] = Interpolate88(ws_violent_overdrive, buffer[i] + 32768);

        buffer[i] = svf.Process(buffer[i]);
        
        // not sure why distortion isn't working, revisit at some point
        // int32_t shiftedSample = mult_q15(buffer[i], f32_to_q15(0.5));// + distortionAmount; // I think this is the overdrive amount? 
        // buffer[i] = Interpolate88(ws_moderate_overdrive, shiftedSample);
        // buffer[i] = mult_q15(buffer[i], f32_to_q15(1));
        buffer[i] = mult_q15(buffer[i], retriggerVolume);
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

void Instrument::UpdateVoiceData(VoiceData &voiceData)
{
    lfo1Target = (LfoTargets)((((uint16_t)voiceData.GetParamValue(Lfo1Target, lastPressedKey, playingStep, playingPattern))*Lfo_Target_Count)>>8);

    delaySend = voiceData.GetParamValue(DelaySend, lastPressedKey, playingStep, playingPattern);
    reverbSend = voiceData.GetParamValue(ReverbSend, lastPressedKey, playingStep, playingPattern);
    volume = ((q15_t)voiceData.GetParamValue(Volume, lastPressedKey, playingStep, playingPattern))<<7;
    panning = ((q15_t)voiceData.GetParamValue(Pan, lastPressedKey, playingStep, playingPattern))<<7;

    q15_t env1A = voiceData.GetParamValue(AttackTime, lastPressedKey, playingStep, playingPattern)<<7;
    q15_t env1D = voiceData.GetParamValue(DecayTime, lastPressedKey, playingStep, playingPattern)<<7;
    q15_t env2A = voiceData.GetParamValue(AttackTime2, lastPressedKey, playingStep, playingPattern)<<7;
    q15_t env2D = voiceData.GetParamValue(DecayTime2, lastPressedKey, playingStep, playingPattern)<<7;
    q15_t lfoState = GetLfoState();
    switch(lfo1Target)
    {
        case Lfo_Target_Env1Attack:
            env1A = add_q15(env1A, lfoState);
            break;
        case Lfo_Target_Env2Attack:
            env2A = add_q15(env2A, lfoState);
            break;
        case Lfo_Target_Env1Decay:
            env1D = add_q15(env1D, lfoState);
            break;
        case Lfo_Target_Env2Decay:
            env2D = add_q15(env1A, lfoState);
            break;
        case Lfo_Target_Env12Attack:
            env1A = add_q15(env1A, lfoState);
            env2A = add_q15(env2A, lfoState);
            break;
        case Lfo_Target_Env12Decay:
            env1D = add_q15(env1D, lfoState);
            env2D = add_q15(env1D, lfoState);
            break;
        default:
            break;
    }
    if(env1D < 0) env1D = 0;
    if(env1A < 0) env1A = 0;
    if(env2D < 0) env2D = 0;
    if(env2A < 0) env2A = 0;
    if(instrumentType == INSTRUMENT_SAMPLE || instrumentType == INSTRUMENT_MACRO)
    {
        env.Update(env1A>>8, env1D>>8);
        env2.Update(env2A>>8, env2D>>8);
        lfo_depth = (voiceData.GetParamValue(LFODepth, lastPressedKey, playingStep, playingPattern)>>1)<<7;
        lfo_rate = (voiceData.GetParamValue(LFORate, lastPressedKey, playingStep, playingPattern)>>1)<<7;
        mainCutoff = (voiceData.GetParamValue(Cutoff, lastPressedKey, playingStep, playingPattern)>>1) << 7;
        svf.set_frequency(add_q15(mainCutoff, lastenv2val>>1));
        svf.set_resonance((voiceData.GetParamValue(Resonance, lastPressedKey, playingStep, playingPattern)>>1) << 7);
        env1Target = (EnvTargets)((((uint16_t)voiceData.GetParamValue(Env1Target, lastPressedKey, playingStep, playingPattern))*Target_Count)>>8);
        env1Depth = (voiceData.GetParamValue(Env1Depth, lastPressedKey, playingStep, playingPattern))<<7;
        env2Target = (EnvTargets)((((uint16_t)voiceData.GetParamValue(Env2Target, lastPressedKey, playingStep, playingPattern))*Target_Count)>>8);
        env2Depth = (voiceData.GetParamValue(Env2Depth, lastPressedKey, playingStep, playingPattern))<<7;
        distortionAmount = (voiceData.GetParamValue(DelaySend, lastPressedKey, playingStep, playingPattern))<<7;
    }
    if(voiceData.GetInstrumentType() == INSTRUMENT_MACRO)
    {
        // copy parameters from voice
        param1Base = voiceData.GetParamValue(Timbre, lastPressedKey, playingStep, playingPattern) << 7;
        param2Base = voiceData.GetParamValue(Color, lastPressedKey, playingStep, playingPattern) << 7;
    }
}

void __not_in_flash_func(Instrument::TempoPulse)(VoiceData &voiceData)
{
    for(int i=0;i<16;i++)
    {
        if(midiNoteStates[i].note < 0)
            continue;
        midiNoteStates[i].ticksRemaining--;
        if(midiNoteStates[i].ticksRemaining <= 0)
        {
            midi->NoteOff(voiceData.GetMidiChannel(), midiNoteStates[i].note-12);
            midiNoteStates[i].note = -1;
        }
    }
    if(retriggersRemaining == 0)
        return;
    retriggerNextPulse -= 1;
    if(retriggerNextPulse == 0)
    {
        retriggersRemaining--;
        retriggerNextPulse = playingVoice->GetParamValue(RetriggerSpeed, lastPressedKey, playingStep, playingPattern);
        retriggerNextPulse = (((uint16_t)retriggerNextPulse*8)>>8) * 4;
        Retrigger();
    }
}
void Instrument::ClearRetriggers()
{
    retriggersRemaining = 0;
}
void __not_in_flash_func(Instrument::Retrigger)()
{
    if(playingVoice->GetInstrumentType() == INSTRUMENT_SAMPLE)
    { 
        sampleSegment = SMP_PLAYING;
        sampleOffset = lastTriggerSampleOffset;
        env.Trigger(ADSR_ENV_SEGMENT_ATTACK);
        env2.Trigger(ADSR_ENV_SEGMENT_ATTACK);
        retriggerVolume = add_q15(retriggerVolume, retriggerFade);
    }
    if(playingVoice->GetInstrumentType() == INSTRUMENT_MACRO)
    {
        enable_env = true;
        osc.Strike();
        env.Trigger(ADSR_ENV_SEGMENT_ATTACK);
        env2.Trigger(ADSR_ENV_SEGMENT_ATTACK);
        retriggerVolume = add_q15(retriggerVolume, retriggerFade);
    }
    else if(playingVoice->GetInstrumentType() == INSTRUMENT_MIDI)
    {
    }
}

void __not_in_flash_func(Instrument::NoteOn)(uint8_t key, int16_t midinote, uint8_t step, uint8_t pattern, bool livePlay, VoiceData &voiceData)
{
    if(livePlay)
    {
        lastPressedKey = key;
    }
    playingStep = step;
    playingPattern = pattern;
    playingVoice = &voiceData;
    int note = midinote;
    if(!livePlay)
    {
        retriggersRemaining = voiceData.GetParamValue(RetriggerLength, lastPressedKey, playingStep, playingPattern);
        retriggerNextPulse = voiceData.GetParamValue(RetriggerSpeed, lastPressedKey, playingStep, playingPattern);
        retriggersRemaining = ((uint16_t)retriggersRemaining*8)>>8;
        retriggerNextPulse = (((uint16_t)retriggerNextPulse*8)>>8) * 4;

        int16_t fade = voiceData.GetParamValue(RetriggerFade, lastPressedKey, playingStep, playingPattern) << 7;
        fade = (fade-0x3fff)*-2; // center at zero
        
        // lets just fade out all the retriggers - so the retrigger volume multiplier starts at full, and fades down
        if(fade > 0)
        {
            retriggerVolume = sub_q15(f32_to_q15(1.0f), fade);
        }
        else
        {
            retriggerVolume = f32_to_q15(1.0f);
        }
        // how much we are going to fade per retrigger
        fade /= retriggersRemaining;
        retriggerFade = fade; // how much we change every frame
    }
    else
    {
        retriggerVolume = f32_to_q15(1.0f);
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
            key = {0};
        }
        if(voiceData.GetSampler() == SAMPLE_PLAYER_SEQL)
        {
            sampleOffset = (voiceData.GetSampleStart(0)) * (filesize>>9);
            sampleEnd = sampleOffset + (voiceData.GetSampleLength(0)) * (filesize>>9);
            if(sampleEnd*2 > filesize - 1)
            {
                sampleEnd = filesize/2 -1;
            }
            // compute the phase increment so it loops in 16 beats
            // first we need to know the number of samples in the loop?
            uint32_t tempoPhaseIncrement = lut_tempo_phase_increment[songData->GetBpm()];
            // 24ppq
            tempoPhaseIncrement = tempoPhaseIncrement + (tempoPhaseIncrement>>1);
            uint32_t tempoPhase = 0;
            uint32_t sampleCount = 0;
            uint8_t beatCount = 0;
            while(beatCount < 6*16)
            {
                tempoPhase += tempoPhaseIncrement;
                sampleCount+=SAMPLES_PER_BLOCK;
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
            lastTriggerSampleOffset = sampleOffset = (sampleEnd-sampleOffset)/16*key+sampleOffset;//((uint64_t)sampleCount)*((uint32_t)(key))/(uint32_t)16+sampleOffset;
        }
        else
        {
            sampleOffset = (voiceData.GetSampleStart(key)) * (filesize>>9);
            sampleEnd = sampleOffset + (voiceData.GetSampleLength(key)) * (filesize>>9);
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
        pitch = note<<7;
        osc.set_pitch(pitch);
        instrumentType = voiceData.GetInstrumentType();
        osc.Strike();
        env.Trigger(ADSR_ENV_SEGMENT_ATTACK);
        env2.Trigger(ADSR_ENV_SEGMENT_ATTACK);
    }
    else if(voiceData.GetInstrumentType() == INSTRUMENT_MIDI)
    {
        
        uint8_t noteHoldTime = (voiceData.GetParamValue(MidiHold, lastPressedKey, playingStep, playingPattern)>>4)+1;

        noteHoldTime *= GrooveBox::getTickCountForRateIndex((voiceData.GetRateForPattern(playingPattern)*7)>>8);

        // determine if this note is already playing
        voiceData.GetParamValue(Timbre, lastPressedKey, playingStep, playingPattern)>>1;
        bool noteIsPlaying = false;
        for(int i=0;i<16;i++)
        {
            if(midiNoteStates[i].note == note)
            {
                noteIsPlaying = true;
                midiNoteStates[i].ticksRemaining = 96;
            }
        }
        if(!noteIsPlaying)
        {
            int16_t lowestTime = 0x7fff;
            uint8_t lowestTimeIdx = 0;
            bool triggered = false;
            for(int i=0;i<16;i++)
            {
                if(midiNoteStates[i].ticksRemaining < lowestTime)
                {
                    lowestTime = midiNoteStates[i].ticksRemaining;
                    lowestTimeIdx = i;
                }
                if(midiNoteStates[i].note < 0)
                {
                    // found a channel that is no longer playing, we can use this one to trigger
                    midi->NoteOn(voiceData.GetMidiChannel(), note-12, voiceData.GetParamValue(Timbre, lastPressedKey, playingStep, playingPattern)>>1);
                    midiNoteStates[i].note = note;
                    midiNoteStates[i].ticksRemaining = noteHoldTime;
                    triggered = true;
                    break;
                }
            }
            if(!triggered)
            {
                // need to steal a channel
                midi->NoteOff(voiceData.GetMidiChannel(), midiNoteStates[lowestTimeIdx].note-12);
                midi->NoteOn(voiceData.GetMidiChannel(), note-12, voiceData.GetParamValue(Timbre, lastPressedKey, playingStep, playingPattern)>>1);
                midiNoteStates[lowestTimeIdx].note = note;
                midiNoteStates[lowestTimeIdx].ticksRemaining = noteHoldTime;
            }
        }
    } 
    lastNoteOnPitch = note;
}