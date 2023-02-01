#include "GrooveBox.h"
#include <string.h>
#include "m6x118pt7b.h"

#define SAMPLES_PER_BUFFER 128

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return
            ((uint32_t) (r) << 0) |
            ((uint32_t) (g) << 16) |
            ((uint32_t) (b) << 8);
}
static inline void u32_urgb(uint32_t urgb, uint8_t *r, uint8_t *g, uint8_t *b) {
    *r = (urgb>>0)&0xff;
    *g = (urgb>>16)&0xff;
    *b = (urgb>>8)&0xff;
}

int16_t temp_buffer[SAMPLES_PER_BUFFER];

void GrooveBox::CalculateTempoIncrement()
{
    tempoPhaseIncrement = lut_tempo_phase_increment[globalData.bpm];
    // 24ppq
    tempoPhaseIncrement = tempoPhaseIncrement + (tempoPhaseIncrement>>1);
    // tempoPhaseIncrement >>= 1;
    // 4 ppq
    // tempoPhaseIncrement = tempoPhaseIncrement >> 1;
}
GrooveBox::GrooveBox(uint32_t *_color)
{
    needsInitialADC = true;
    midi.Init();
    ResetADCLatch();
    tempoPhase = 0;
    for(int i=0;i<VOICE_COUNT;i++)
    {
        instruments[i].Init(&midi, temp_buffer);
        instruments[i].globalData = &globalData;
    }
    globalData.bpm = 119;
    for(int i=0;i<16;i++)
    {
        patterns[i].SetInstrumentType(INSTRUMENT_MACRO);
    }
    printf("voice data size %u\n", (sizeof(patterns[0])*16-sizeof(patterns[0].notes)*16));
    Deserialize();

    // we do this in a second pass so the
    // deserialized pointers aren't pointing to the wrong places
    for(int i=0;i<16;i++)
    {
        if(i==15)
            continue;
        ffs_open(GetFilesystem(), &files[i], i);
        patterns[i].SetFile(&files[i]);
    }
    CalculateTempoIncrement();
    color = _color;
}

int16_t workBuffer[SAMPLES_PER_BUFFER];
int32_t workBuffer2[SAMPLES_PER_BUFFER];
static uint8_t sync_buffer[SAMPLES_PER_BUFFER];
int16_t toDelayBuffer[SAMPLES_PER_BUFFER];
int16_t toReverbBuffer[SAMPLES_PER_BUFFER];
int16_t recordBuffer[SAMPLES_PER_BUFFER];
//absolute_time_t lastRenderTime = -1;
int16_t last_delay = 0;
int16_t last_input;
uint32_t counter = 0;
uint32_t countToHalfSecond = 0;
uint8_t sync_count = 0;
void GrooveBox::Render(int16_t* output_buffer, int16_t* input_buffer, size_t size)
{
    absolute_time_t renderStartTime = get_absolute_time();
    memset(workBuffer2, 0, sizeof(int32_t)*SAMPLES_PER_BUFFER);
    memset(toDelayBuffer, 0, sizeof(int16_t)*SAMPLES_PER_BUFFER);
    memset(toReverbBuffer, 0, sizeof(int16_t)*SAMPLES_PER_BUFFER);
    memset(output_buffer, 0, sizeof(int16_t)*SAMPLES_PER_BUFFER);
    last_input = 0;
    
    //printf("input %i\n", workBuffer2[0]);
    for(int i=0;i<SAMPLES_PER_BUFFER;i++)
    {
        int16_t input = input_buffer[i*2];
        // collapse input buffer so its easier to copy to the recording device
        // temporarily use the output buffer
        recordBuffer[i] = input;
        if(playThroughEnabled)
        {
            workBuffer2[i] = input;
        }
        if(input<0)
        {
            input *= -1;
        }
        last_input = input>last_input?input:last_input;
    }
    CalculateTempoIncrement();
    if(!recording)
    {
        for(int v=0;v<VOICE_COUNT;v++)
        {
            memset(sync_buffer, 0, SAMPLES_PER_BUFFER);
            memset(workBuffer, 0, sizeof(int16_t)*SAMPLES_PER_BUFFER);
            instruments[v].Render(sync_buffer, workBuffer, SAMPLES_PER_BUFFER);
            // mix in the instrument
            for(int i=0;i<SAMPLES_PER_BUFFER;i++)
            {
                // scale everything down
                // workBuffer[i] = mult_q15(workBuffer[i], 0x4fff);
                workBuffer2[i] += workBuffer[i];
                toDelayBuffer[i] = add_q15(toDelayBuffer[i], mult_q15(workBuffer[i], ((int16_t)instruments[v].delaySend)<<7));
                toReverbBuffer[i] = add_q15(toReverbBuffer[i], mult_q15(workBuffer[i], ((int16_t)instruments[v].reverbSend)<<7));
            }
        }
        bool clipping = false;
        for(int i=0;i<SAMPLES_PER_BUFFER;i++)
        {
            int16_t* chan = (output_buffer+i*2);
            int32_t mainL = 0;
            int32_t mainR = 0;
            mainL = workBuffer2[i];
            mainR = workBuffer2[i];

            int16_t l = 0;
            int16_t r = 0;
            delay.process(toDelayBuffer[i], l, r);
            int32_t lres = l;
            int32_t rres = r;
            // lower delay volume
            lres *= 0xe0;
            rres *= 0xe0;
            lres = lres >> 8;
            rres = rres >> 8;
            mainL += lres;
            mainR += rres;

            verb.process(toReverbBuffer[i], l, r);
            lres = l;
            rres = r;
            // lower delay volume
            lres *= 0xe0;
            rres *= 0xe0;
            lres = lres >> 8;
            rres = rres >> 8;
            mainL += l;
            mainR += r;

            chan[0] = mainL;
            chan[1] = mainR;
            
            // lets do a master volume
            // mainL *= 0xe0;
            // mainR *= 0xe0;
            // mainL = mainL >> 8;
            // mainR = mainR >> 8;

            // one more headroom clip (rip :( )

            // and then hard limit so we don't get overflows
            int32_t max = 0x7fff;
            if(mainL > max)
            {
                clipping = true;
                mainL = max;
            }
            if(mainR > max)
            {
                clipping = true;
                mainR = max;
            }
            if(mainL < -max)
            {
                clipping = true;
                mainL = -max;
            }
            if(mainR < -max)
            {
                clipping = true;
                mainR = -max;
            }

            chan[0] = mainL;
            chan[1] = mainR;

        }
        // if(clipping)
        // {
        //     for(int i=0;i<SAMPLES_PER_BUFFER;i++)
        //     {
        //         int16_t* chan = (output_buffer+i*2);
        //         chan[0] = 0;
        //         chan[1] = 0;
        //     }
        // }
        // if(clipping) printf("clipping.");
        // this can all be done outside this loop?

        // handle the midi triggering
        int midiNote = midi.GetNote();
        if( midiNote >= 0)
        {
            patterns[currentVoice].ClearNextRequestedStep();
            TriggerInstrumentMidi(midiNote, midiNote, 0, 0, patterns[currentVoice], currentVoice);
        }

        int requestedNote = GetNote();
        if(requestedNote >= 0)
        {
            patterns[currentVoice].ClearNextRequestedStep();
            TriggerInstrument(requestedNote, requestedNote > 15?requestedNote:-1, 0, 0, true, patterns[currentVoice], currentVoice);
        }
 
        if(IsPlaying())
        {
            bool tempoPulse = false;
            tempoPhase += tempoPhaseIncrement;
            if((tempoPhase >> 31) > 0)
            {
                tempoPhase &= 0x7fffffff;
                tempoPulse = true;
            }
            if(tempoPulse)
            {
                if((globalData.GetSyncOutMode()&SyncOutModeMidi) > 0)
                    midi.TimingClock();
                
                // advance chain if the global pattern just overflowed on the last beat counter
                if(beatCounter[16]==0 && patternStep[16] == 0)
                {
                    chainStep = (++chainStep)%patternChainLength;
                    // if we are moving to a new pattern, reset all the steps to zero
                    if(patternChain[chainStep]!=playingPattern)
                    {
                        playingPattern = patternChain[chainStep];
                        ResetPatternOffset();
                    }
                }

                // two extra counters
                // 17: the pattern change counter
                // 18: the sync output counter
                for(int v=0;v<18;v++)
                {
                    bool needsTrigger = beatCounter[v]==0;
                    beatCounter[v]++;

                    // global pattern (pattern change counter) always uses 16th notes
                    uint8_t rate = 0;
                    switch(v)
                    {
                        case 16:
                            rate = 2;
                            break;
                        case 17:
                            if((globalData.GetSyncOutMode() & SyncOutModePO) > 0)
                                rate = 4;
                            else if((globalData.GetSyncOutMode() & SyncOutMode24) > 0)
                                rate = 7;
                            else
                                rate = 0;
                            break;
                        default:
                            rate = (patterns[v].rate[GetCurrentPattern()]*7)>>8;
                    }
                    switch(rate)
                    {
                        case 0: // 2x
                            beatCounter[v] = beatCounter[v]%3;
                            break;
                        case 1: // 3/2x
                            beatCounter[v] = beatCounter[v]%4;
                            break;
                        case 2: // 1x
                            beatCounter[v] = beatCounter[v]%6;
                            break;
                        case 3: // 3/4x
                            beatCounter[v] = beatCounter[v]%8;
                            break;
                        case 4: // 1/2x
                            beatCounter[v] = beatCounter[v]%12;
                            break;
                        case 5: // 1/4x
                            beatCounter[v] = beatCounter[v]%24;
                            break;
                        case 6: // 1/8x
                            beatCounter[v] = beatCounter[v]%48;
                            break;
                        case 7: // 24 ppq sync
                            beatCounter[v] = 0;
                            break;
                    }
                    if(!needsTrigger)
                        continue;
                    if(v==17){
                        sync_count = 6;
                        continue;
                    }
                    // never trigger for the global pattern
                    int requestedNote = (v==16||v==17)?-1:GetTrigger(v, patternStep[v]);
                    if(requestedNote >= 0)
                    {
                        hadTrigger = hadTrigger|(1<<v);
                        if((allowPlayback>>v)&0x1)
                        {
                            patterns[v].SetNextRequestedStep(patternStep[v]);
                            TriggerInstrument(requestedNote, requestedNote > 15?requestedNote:-1, patternStep[v]|0x80, GetCurrentPattern(), false, patterns[v], v);
                        }
                    }
                    patternStep[v] = (patternStep[v]+1)%(patterns[v].GetLength(GetCurrentPattern()));
                }
            }
        }
    }
    if(sync_count > 0 && (globalData.GetSyncOutMode()&(SyncOutModePO|SyncOutMode24)) > 0)
    {
        for(int i=0;i<SAMPLES_PER_BUFFER;i++)
        {
            int16_t* chan = (output_buffer+i*2);
            // we need to provide the peak to peak swing so pocket operator can hear the sync signal
            if(sync_count > 3)
            {
                chan[0] = -32767;
            }
            else if(sync_count > 0)
            {
                chan[0] = 32767;
            }
            else
            {
                chan[0] = 0;
            }
        }
        sync_count--;
    }
    {
        sync_count = 0;
    }

    if(recording)
    {
        ffs_append(GetFilesystem(), &files[recordingTarget], recordBuffer, SAMPLES_PER_BUFFER*2);
        for(int i=0;i<SAMPLES_PER_BUFFER;i++)
        {
            int16_t* chan = (output_buffer+i*2);
            chan[0] = workBuffer2[i];
            chan[1] = workBuffer2[i];
        }
    }
    absolute_time_t renderEndTime = get_absolute_time();
    int64_t currentRender = absolute_time_diff_us(renderStartTime, renderEndTime);
    renderTime += currentRender;
    sampleCount++;
    midi.Flush();
}
void GrooveBox::TriggerInstrument(int16_t pitch, int16_t midi_note, uint8_t step, uint8_t pattern, bool livePlay, VoiceData &voiceData, int channel)
{
    // lets just simplify - gang together the instruments that are above each other
    // 1+5, 2+6, 9+13 etc
    voiceCounter = channel%4+(channel/8)*4;
    Instrument *nextPlay = &instruments[voiceCounter];
    voiceChannel[voiceCounter] = channel;
    //printf("playing file for voice: %i %x\n", channel, voiceData.GetFile());

    nextPlay->NoteOn(pitch, midi_note, step, pattern, livePlay, voiceData);
    return;    
    // Instrument *nextPlay = &instruments[voiceCounter];
    // // determine if any of the voices are done playing
    // bool foundVoice = false;
    // for (size_t i = 0; i < VOICE_COUNT; i++)
    // {
    //     if(!instruments[i].IsPlaying())
    //     {
    //         nextPlay = &instruments[i];
    //         foundVoice = true;
    //         voiceChannel[i] = channel;
    //         break;
    //     }
    // }
    // nextPlay->NoteOn(pitch, midi_note, step, pattern, livePlay, voiceData);
    // if(!foundVoice)
    // {
    //     voiceChannel[voiceCounter] = channel;
    //     voiceCounter = (++voiceCounter)%VOICE_COUNT;
    // }
}
void GrooveBox::TriggerInstrumentMidi(int16_t pitch, int16_t midi_note, uint8_t step, uint8_t pattern, VoiceData &voiceData, int channel)
{
    Instrument *nextPlay = &instruments[channel];
    // determine if any of the voices are done playing
    bool foundVoice = false;
    for (size_t i = 0; i < VOICE_COUNT; i++)
    {
        if(!instruments[i].IsPlaying())
        {
            nextPlay = &instruments[i];
            foundVoice = true;
            voiceChannel[i] = channel;
            break;
        }
    }
    nextPlay->NoteOn(pitch, midi_note, step, pattern, true, voiceData);
    if(!foundVoice)
    {
        voiceChannel[voiceCounter] = channel;
        voiceCounter = (++voiceCounter)%VOICE_COUNT;
    }
}
int GrooveBox::GetTrigger(uint voice, uint step)
{
    uint8_t note = patterns[voice].notes[GetCurrentPattern()*64+step];
    if(note >> 7 == 1)
    {
        return note&0x7f;
    }
    return -1;
}
int GrooveBox::GetNote()
{
    int res = needsNoteTrigger;
    needsNoteTrigger = -1;
    return res;
}

bool GrooveBox::IsPlaying()
{
    return playing;
}
void GrooveBox::UpdateDisplay(ssd1306_t *p)
{
    drawCount++;
    // if(sampleCount > 500)
    // {
    //     renderTime /= sampleCount;
    //     printf("renderTime us: %i\n", (int)renderTime);
    //     renderTime = 0;
    //     sampleCount = 0;
    // }

    ssd1306_clear(p);
    char str[64];
    ssd1306_set_string_color(p, false);
    if(clearTime > 0 && holdingEscape)
    {
        sprintf(str, "clear pat %i in %i", GetCurrentPattern()+1, clearTime/60);
        clearTime--;
        if(clearTime == 0)
        {
            // empty the current pattern
            for (size_t voice = 0; voice < 16; voice++)
            {
                for (size_t j = 0; j < 64; j++)
                {
                    patterns[voice].notes[GetCurrentPattern()*64+j] = 0;
                }
            }
        }
        ssd1306_draw_string(p, 0, 8, 1, str);
        return;
    }
    else if(holdingArm && holdingEscape)
    {
        sprintf(str, "press 1-16 to erase sample");
        ssd1306_draw_string_gfxfont(p, 3, 12, str, true, 1, 1, &m6x118pt7b);
        return;
    }
    else if(holdingArm && !recording)
    {
        sprintf(str, "hold 1-16 to record");
        // ssd1306_draw_square_rounded(p, 0, 17, width, 15);
        ssd1306_draw_string_gfxfont(p, 3, 12, str, true, 1, 1, &m6x118pt7b);
        ssd1306_draw_square(p, 0, 17, last_input>>8, 32-17);
        return;
    }
    else if(shutdownTime > 0 && holdingEscape)
    {
        sprintf(str, "shutdown in %i", shutdownTime/60);
        ssd1306_draw_string_gfxfont(p, 3, 12, str, true, 1, 1, &m6x118pt7b);
        shutdownTime--;  
        if(shutdownTime == 0)
        {
            erasing = true;
            Serialize();
            hardware_shutdown();
        }
        return;
    }
    else if(hardware_get_battery_level() < 115)
    {
        sprintf(str, "Low Battery!");
        if((drawCount % 90) < 45)
            ssd1306_draw_string_gfxfont(p, 3, 12, str, true, 1, 1, &m6x118pt7b);
        sprintf(str, "Save and charge.");
        ssd1306_draw_string_gfxfont(p, 3, 17+12, str, true, 1, 1, &m6x118pt7b);
        return;
    }
    else if(clearTime == 0 && patternSelectMode && holdingEscape)
    {
        sprintf(str, "pat %i cleared", GetCurrentPattern()+1);
        ssd1306_draw_string(p, 0, 8, 1, str);
        return;
    }
    else if(!patternSelectMode)
    {
        int width = 45;
        sprintf(str, "Snd %i", currentVoice+1);
        if(soundSelectMode)
            ssd1306_draw_square_rounded(p, 0, 0, width, 15);
        ssd1306_draw_string_gfxfont(p, 3, 12, str, !soundSelectMode, 1, 1, &m6x118pt7b);
        // sprintf(str, "Snd %i", currentVoice);
        sprintf(str, "Pat %i", GetCurrentPattern()+1);
        // ssd1306_draw_square_rounded(p, 0, 17, width, 15);
        ssd1306_draw_string_gfxfont(p, 3, 17+12, str, true, 1, 1, &m6x118pt7b);
    }

    // copy pattern
    if(patternSelectMode && holdingWrite)
    {
        sprintf(str, "copy pat %i to", GetCurrentPattern()+1);
        ssd1306_draw_string(p, 0, 8, 1, str);
        return;
    }

    if(soundSelectMode && holdingWrite)
    {
        sprintf(str, "copy voice %i to", currentVoice+1);
        ssd1306_draw_string(p, 0, 8, 1, str);
        return;
    }

    if(patternSelectMode && clearTime < 0 && !holdingWrite)
    {
        ssd1306_draw_string(p, 0, 8, 1, "chn");
        if(patternChainLength>0)
        {
            for(int i=0;i<patternChainLength;i++)
            {
                int boxX = 24+(i%8)*12;
                int boxY = 8+12*(i/8);
                if(i==chainStep)
                {
                    ssd1306_draw_square(p, boxX, boxY+10, 15, 1);
                }
                int pDisp = patternChain[i]+1;
                sprintf(str, "%i", pDisp);
                int offset = pDisp < 10?3:0;
                ssd1306_draw_string(p, boxX+2+offset, boxY+2, 1, str);
            }
        }
    }
    else if(recording)
    {
        int filesize = ffs_file_size(GetFilesystem(), &files[recordingTarget]);
        sprintf(str, "Remaining: %i", (16*1024*1024-0x40000-filesize)/88100);
        ssd1306_draw_string(p, 0, 0, 1, str);
    }
    else
    {
        if(selectedGlobalParam)
        {
            globalData.DrawParamString(param, GetCurrentPattern(), str);
        }
        else
        {
            uint8_t paramLockSignal = storingParamLockForStep;
            if(holdingWrite)
                paramLockSignal = 0x80|(0x7f&patternStep[currentVoice]);
            patterns[currentVoice].DrawParamString(param, str, lastNotePlayed, GetCurrentPattern(), paramLockSignal);
        }
    }
    uint8_t fade_speed = 0xaf;
    if(holdingMute)
    {
        fade_speed = 0xcf;
    }
    // grid
    for(int i=0;i<16;i++)
    {
        int x = i%4;
        int y = i/4;
        int key = x+(y+1)*5;
        if(holdingMute)
        {
            if(allowPlayback>>i & 0x1)
            {
                color[key] = urgb_u32(30, 120, 250);
            }
            if(hadTrigger & (1<<i))
            {
                color[key] = urgb_u32(30, 80, 250);
            }
        }
        else if(patternSelectMode)
        {
            if(GetCurrentPattern() == i)
            {
                color[key] = urgb_u32(10, 200, 45);
            }

        }
        else if(soundSelectMode)
        {
            if(currentVoice == i)
            {
                color[key] = urgb_u32(10, 40, 255);
            } 
            // if the voice just triggered
            if(hadTrigger & (1<<i))
            {
                color[key] = urgb_u32(50, 50, 50);
            }
        }
        else
        {
            if((patternStep[currentVoice]-1<0?patterns[currentVoice].GetLength(GetCurrentPattern())-1:patternStep[currentVoice]-1)-editPage[currentVoice]*16 == i && IsPlaying())
            {
                color[key] = urgb_u32(100, 100, 100);
            }
            else if(GetTrigger(currentVoice, i+16*editPage[currentVoice])>=0)
            {
                // has a parameter lock
                if(patterns[currentVoice].HasAnyLockForStep(0x80|(i+16*editPage[currentVoice]), GetCurrentPattern()))
                {
                    color[key] = urgb_u32(180, 60, 220);
                }
                else
                {
                    color[key] = urgb_u32(100, 60, 200);
                }
            }
        }

        {
            // fade to black
            uint8_t r,g,b;
            u32_urgb(color[key], &r, &g, &b);
            color[key] = urgb_u32(((uint16_t)r*fade_speed)>>8, ((uint16_t)g*fade_speed)>>8, ((uint16_t)b*fade_speed)>>8);
        }
    }
    // right side buttons
    if(liveWrite || writing)
    {
        color[24] = urgb_u32(255, 40, 40);
    }
    // play light
    if(IsPlaying())
    {
        if(CurrentStep%4==0)
        {
            if(liveWrite)
            {
                color[19] = urgb_u32(255, 40, 40);
            }
            else
            {
                color[19] = urgb_u32(40, 255, 40);
            }
        }
        else
        {
            uint8_t r,g,b;
            u32_urgb(color[19], &r, &g, &b);
            const uint8_t fade_speed = 0xef; 
            g = ((uint16_t)g*fade_speed)>>8;
            color[19] = urgb_u32(((uint16_t)r*fade_speed)>>8, g>20?g:20, ((uint16_t)b*fade_speed)>>8);
        }
    }
    else
    {
        color[19] = urgb_u32(0, 0, 0);
    }
    // update various slow hardware things
    playThroughEnabled = hardware_line_in_detected();
    hardware_set_mic(!playThroughEnabled);
        

    // input level monitor
    // ssd1306_draw_line(p, 0, 0, last_input>>8, 0);
    // draw the current page within the pattern that we are editing 
    for (size_t i = 0; i < 4; i++)
    {
        if((i+1) > ((patterns[currentVoice].GetLength(GetCurrentPattern())-1)/16+1))
            break;
        ssd1306_draw_square_rounded(p, 48, i*8+1, 6, 6);
        if(editPage[currentVoice] != i)
        {
            ssd1306_clear_square(p, 48+1, i*8+1+1, 4, 4);
        }
    }
    hadTrigger = 0;
    
    uint16_t param = instruments[currentVoice%4+(currentVoice/8)*4].pWithMods;

    ssd1306_draw_square(p, 0, 0, param>>8, 1);
}
void GrooveBox::SetGlobalParameter(uint8_t a, uint8_t b, bool setA, bool setB)
{
    uint8_t& current_a = globalData.GetParam(param*2, GetCurrentPattern());
    uint8_t& current_b = globalData.GetParam(param*2+1, GetCurrentPattern());
    if(paramSetA)
    {
        current_a = a;
        lastAdcValA = a;
    }
    if(paramSetB)
    {
        current_b = b;
        lastAdcValB = b;
    }
}
void GrooveBox::OnAdcUpdate(uint8_t a, uint8_t b)
{
    if(needsInitialADC)
    {
        lastAdcValA = a;
        lastAdcValB = b;
        needsInitialADC = false;
        return;
    }
    // check to see if the adcs have moved since we last changed the page
    if(!paramSetA)
    {
        if(lastAdcValA>a)
        {
            if(lastAdcValA-a > 3)
            {
                paramSetA = true;
            }
        }
        else
        {
            if(a-lastAdcValA > 3)
            {
                paramSetA = true;
            }
        }
    }
    if(!paramSetB)
    {
        if(lastAdcValB>b)
        {
            if(lastAdcValB-b > 3)
            {
                paramSetB = true;
            }
        }
        else
        {
            if(b-lastAdcValB > 3)
            {
                paramSetB = true;
            }
        }
    }
    if(selectedGlobalParam)
        return SetGlobalParameter(a, b, paramSetA, paramSetB);
    // live param recording
    if(holdingWrite)
    {
        if(paramSetA)
        {
            lastAdcValA = a;
            patterns[currentVoice].StoreParamLock(param*2, patternStep[currentVoice]&0x7f, GetCurrentPattern(), a);
            parameterLocked = true;
        }
        if(paramSetB)
        {
            lastAdcValB = b;
            patterns[currentVoice].StoreParamLock(param*2+1, patternStep[currentVoice]&0x7f, GetCurrentPattern(), b);
            parameterLocked = true;
        }
        return;
    }
    // should use the distance from the parameter to trigger this
    if(storingParamLockForStep >> 7 == 1)
    {
        if(paramSetA)
        {
            lastAdcValA = a;
            patterns[currentVoice].StoreParamLock(param*2, storingParamLockForStep&0x7f, GetCurrentPattern(), a);
            parameterLocked = true;
        }
        if(paramSetB)
        {
            lastAdcValB = b;
            patterns[currentVoice].StoreParamLock(param*2+1, storingParamLockForStep&0x7f, GetCurrentPattern(), b);
            parameterLocked = true;
        }
        return;
    }
    uint8_t& current_a = patterns[currentVoice].GetParam(param*2, lastNotePlayed, GetCurrentPattern());
    uint8_t& current_b = patterns[currentVoice].GetParam(param*2+1, lastNotePlayed, GetCurrentPattern());
    bool voiceNeedsUpdate = false;
    if(paramSetA)
    {
        current_a = a;
        voiceNeedsUpdate = true;
        lastAdcValA = a;
    }
    if(paramSetB)
    {
        current_b = b;
        voiceNeedsUpdate = true;
        lastAdcValB = b;
    }
    if(voiceNeedsUpdate)
    {
        for (size_t i = 0; i < 8; i++)
        {
            if(voiceChannel[i] == currentVoice)
            {
                instruments[i].UpdateVoiceData(patterns[currentVoice]);
            }
        }
    }
}
void GrooveBox::OnFinishRecording()
{
        // determine how the file should be split
    if(patterns[recordingTarget].GetSampler() == SAMPLE_PLAYER_SLICE)
    {
        for (size_t i = 0; i < 16; i++)
        {
            patterns[recordingTarget].sampleLength[i] = 16;
            patterns[recordingTarget].sampleStart[i] = i*16;
        }
    }
   
}
void GrooveBox::OnKeyUpdate(uint key, bool pressed)
{
    int x=key/5;
    int y=key%5;
    
    // record mode
    if(x==4&&y==0)
    {
        holdingArm = pressed;
        if(!holdingArm && recording)
        {
            // finish recording
            recording = false;
            OnFinishRecording();
        }
    }
    if(x==3&&y==0)
    {
        holdingEscape = pressed;
        if(!holdingEscape)
        {
            clearTime = -1;
            shutdownTime = -1;
        }
    }
    if(x==4&&y==1)
    {
        holdingMute = pressed;
    }
    if(x==4&&y==0)
    {
        holdingArm = pressed;
        if(!holdingArm && recording)
        {
            // finish recording
            recording = false;
            OnFinishRecording();
        }
    }
    // page select
    if(x==4&&y==2 && pressed)
    {
        editPage[currentVoice] = (++editPage[currentVoice])%(patterns[currentVoice].length[GetCurrentPattern()]/64+1);
    }
    if(x<4 && y>0 && !pressed)
    {
        if( !soundSelectMode
            && !paramSelectMode
            && !patternSelectMode
            && !liveWrite
            && !holdingMute
            && writing)
        {
            // if we are releasing, then commit the input if there wasn't a parameter lock stored
            // todo: track if we stored a param lock
            int sequenceStep = x+(y-1)*4;
            storingParamLockForStep = 0;
            ResetADCLatch();
            if(GetTrigger(currentVoice, sequenceStep+editPage[currentVoice]*16)>=0)
            {
                if(!parameterLocked){
                    patterns[currentVoice].notes[GetCurrentPattern()*64+sequenceStep+editPage[currentVoice]*16] = 0;
                    patterns[currentVoice].RemoveLocksForStep(GetCurrentPattern(), sequenceStep);
                }
            }
            else
            {
                patterns[currentVoice].notes[GetCurrentPattern()*64+sequenceStep+editPage[currentVoice]*16] = (0x7f&lastNotePlayed)|0x80;
            }
        }

    }
    if(x<4 && y>0 && pressed)
    {
        int sequenceStep = x+(y-1)*4;
        if(holdingEscape && holdingArm)
        {
            if(pressed)
            {
                erasing = true;
                holdingEscape = false;
                holdingArm = false;
                ffs_erase(GetFilesystem(), &files[sequenceStep]);
                erasing = false;
            }
        }
        else if(holdingArm)
        {
            if(pressed && !recording)
            {
                recordingLength = 0;
                recordingTarget = sequenceStep;
                recording = true;
            }
            else
            {
                // finish recording
                recording = false;
                OnFinishRecording();
            }
        }
        else if(soundSelectMode)
        {

            if(holdingWrite && sequenceStep!=currentVoice && sequenceStep != 15)
            {
                // copy voice
                patterns[sequenceStep].CopyFrom(patterns[currentVoice]);
            }
            else
            {
                // go to new voice
                if(sequenceStep != currentVoice)
                    ResetADCLatch();
                currentVoice = sequenceStep;
            }

        }
        else if(paramSelectMode)
        {
            if(sequenceStep != param)
                paramSetA = paramSetB = false;
            param = sequenceStep;
            selectedGlobalParam = false;
        }
        else if(holdingMute)
        {
            // toggle the mute state of this channel
            allowPlayback ^= 1 << sequenceStep;
        }
        else if(patternSelectMode)
        {
            if(holdingWrite && sequenceStep!=GetCurrentPattern())
            {
                // copy pattern
                int currentPattern = patternChain[0];
                for (size_t voice = 0; voice < 16; voice++)
                {
                    for (size_t step = 0; step < 64; step++)
                    {
                        patterns[voice].notes[sequenceStep*64+step] = patterns[voice].notes[currentPattern*64+step]; 
                    }
                    // copy the pattern lengths as well
                    patterns[voice].length[sequenceStep] = patterns[voice].length[currentPattern];
                    patterns[voice].rate[sequenceStep] = patterns[voice].rate[currentPattern];
                    // clear all the parameter locks for the target pattern
                    patterns[voice].ClearParameterLocks(sequenceStep);
                    // copy any parameter locks the pattern has
                    patterns[voice].CopyParameterLocks(currentPattern, sequenceStep);
                }
            }
            else
            {
                // immediately insert the next pattern, and reset the chain length to 1
                if(!nextPatternSelected)
                {
                    patternChainLength = 0;
                    nextPatternSelected = true;
                    chainStep = 0;
                    ResetADCLatch();
                }
                if(patternChainLength<15)
                {
                    if(!playing && patternChainLength == 0)
                    {
                        playingPattern = sequenceStep;
                    }
                    patternChain[patternChainLength++] = sequenceStep;
                }
            }
        }
        else if(!writing)
        {
            lastNotePlayed = needsNoteTrigger = sequenceStep;
            ResetADCLatch();
        }
        else if(liveWrite)
        {
            uint16_t noteToModify = patternChain[chainStep]*64+patternStep[currentVoice];
            uint8_t newValue = (0x7f&sequenceStep)|0x80;
            patterns[currentVoice].notes[noteToModify] = newValue;
        }
        else // writing
        {
            ResetADCLatch();
            parameterLocked = false;
            // see if we are going to commit a param lock
            storingParamLockForStep = (0x7f&(sequenceStep+editPage[currentVoice]*16))|0x80;
        }
    }
    // play
    if(x==4 && y==3 && pressed)
    {
        if(paramSelectMode)
        {
            // just hardcode for now
            if(param != x+y*5)
                paramSetA = paramSetB = false;
            selectedGlobalParam = true;
            param = x+y*5;
        } 
        else if(holdingWrite)
        {
            liveWrite = true;
            if(!playing)
            {
                playing = true;
                if((globalData.GetSyncOutMode()&SyncOutModeMidi) > 0)
                {
                    midi.StopSequence();
                    midi.StartSequence();
                }
                ResetPatternOffset();
            }
        }
        else if(liveWrite)
        {
            liveWrite = false;
            writing = false;
            playing = true;
        }
        else
        {
            playing = !playing;
            if(playing)
            {
                if((globalData.GetSyncOutMode()&SyncOutModeMidi) > 0)
                    midi.StartSequence();
                ResetPatternOffset();
            }
            else
            {
                if((globalData.GetSyncOutMode()&SyncOutModeMidi) > 0)
                    midi.StopSequence();
            }
        }
        if(liveWrite)
        {
            color[x+y*5] = urgb_u32(20, 0, 7);
        }
        else
        {
            color[x+y*5] = playing?urgb_u32(3, 20, 7):urgb_u32(0,0,0);
            if(!writing)
            {
                color[24] = 0;
            }
        }
    }
    // write
    if(x==4 && y==4)
    {
        holdingWrite = pressed;
        if(pressed)
        {
            liveWrite = false;
            writing = !writing;
            color[x+y*5] = writing?urgb_u32(20, 10, 12):urgb_u32(0,0,0);
        }
    }
    
    // sound select mode
    if(x==0 && y==0)
    {
        soundSelectMode = pressed;
    }

    if(holdingEscape && soundSelectMode)
    {
        // start countdown to save & shutdown / bootselect
        shutdownTime = 60*3;
    }

    // pattern select
    if(x==1 && y==0)
    {
        if(pressed && paramSelectMode)
        {
            if(param != x+y*5)
                paramSetA = paramSetB = false;
            selectedGlobalParam = true;
            param = x+y*5;
        }
        else
        {
            if(holdingEscape && pressed)
            {
                clearTime = 180*2;
            }
            patternSelectMode = pressed;
            if(pressed)
            {
                nextPatternSelected = false;
            }
        }
    }

    // param select
    if(x==2 && y==0)
    {
        paramSelectMode = pressed;
    }
}
#define SAVE_SIZE 16*16*16
#define SAVE_VERSION 15
void GrooveBox::Serialize()
{
    Serializer s;
    // erase the existing file first
    ffs_file writefile;
    erasing = true;
    ffs_open(GetFilesystem(), &writefile, 101);
    ffs_erase(GetFilesystem(), &writefile);
    s.Init();
    // version
    s.AddData(SAVE_VERSION);

    // save pattern data
    uint8_t *patternReader = (uint8_t*)patterns;
    for (size_t i = 0; i < sizeof(VoiceData)*16; i++)
    {
        s.AddData(*patternReader);
        patternReader++;
    }
    // save the parameter locks
    VoiceData::SerializeStatic(s);
    
    patternReader = (uint8_t*)&globalData;
    for (size_t i = 0; i < sizeof(GlobalData); i++)
    {
        s.AddData(*patternReader);
        patternReader++;
    }


    s.Finish();
    printf("serialized file size %i\n", s.writeFile.filesize);
}
void GrooveBox::Deserialize()
{
    // should probably put this is some different class
    Serializer s;
    s.Init();
    if(!s.writeFile.initialized)
        return;
    // save version
    if(s.GetNextValue() != SAVE_VERSION)
        return;

    // load pattern data
    uint8_t *patternReader = (uint8_t*)patterns;
    for (size_t i = 0; i < sizeof(VoiceData)*16; i++)
    {
        *patternReader = s.GetNextValue();
        patternReader++;
    }
    
    VoiceData::DeserializeStatic(s);
    patternReader = (uint8_t*)&globalData;
    for (size_t i = 0; i < sizeof(GlobalData); i++)
    {
        *patternReader = s.GetNextValue();
        patternReader++;
    }

    printf("loaded filesize %i\n", s.writeFile.filesize);
}