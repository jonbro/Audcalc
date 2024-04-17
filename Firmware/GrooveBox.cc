#include "GrooveBox.h"
#include <string.h>
#include "m6x118pt7b.h"
#include <pb_encode.h>
#include <pb_decode.h>
#include "VoiceDataInternal.pb.h"
#include "tlv320driver.h"
#include "multicore_support.h"
#include "GlobalDefines.h"

static inline void u32_urgb(uint32_t urgb, uint8_t *r, uint8_t *g, uint8_t *b) {
    *r = (urgb>>0)&0xff;
    *g = (urgb>>16)&0xff;
    *b = (urgb>>8)&0xff;
}

int16_t temp_buffer[SAMPLES_PER_BLOCK];
GrooveBox* groovebox;

void OnCCChangedBare(uint8_t cc, uint8_t newValue)
{
    groovebox->OnCCChanged(cc, newValue);
}
void GrooveBox::CalculateTempoIncrement()
{
    tempoPhaseIncrement = lut_tempo_phase_increment[songData.GetBpm()];
}
void GrooveBox::init(uint32_t *_color, lua_State *L)
{
    // usbSerialDevice = _usbSerialDevice;
    needsInitialADC = 30;
    groovebox = this;
    midi.Init();
    midi.OnCCChanged = OnCCChangedBare;
    ResetADCLatch();
    tempoPhase = 0;
    for(int i=0;i<VOICE_COUNT;i++)
    {
        instruments[i].Init(&midi, temp_buffer);
        instruments[i].songData = &songData;
    }
    for(int i=0;i<16;i++)
    {
        patterns[i].InitDefaults();
        patterns[i].SetInstrumentType(INSTRUMENT_MACRO);
    }
    this->L = L;
    int error = luaL_dostring(L, "function tempoSync() end");
    if (error) {
        fprintf(stderr, "%s", lua_tostring(L, -1));
        lua_pop(L, 1);  /* pop error message from the stack */
    }
}

int16_t workBuffer[SAMPLES_PER_BLOCK];
int16_t workBuffer3[SAMPLES_PER_BLOCK];
int32_t workBuffer2[SAMPLES_PER_BLOCK*2];
static uint8_t sync_buffer[SAMPLES_PER_BLOCK];
int16_t toDelayBuffer[SAMPLES_PER_BLOCK];
int16_t toReverbBuffer[SAMPLES_PER_BLOCK];
int16_t recordBuffer[128]; // this must be 128 due to the requirements of the filesystem
uint8_t recordBufferOffset = 0;
//absolute_time_t lastRenderTime = -1;
int16_t last_delay = 0;
int16_t last_input;
uint32_t counter = 0;
uint32_t countToHalfSecond = 0;
uint8_t sync_count = 0;
bool audio_sync_state;
uint16_t audio_sync_countdown = 0;
uint32_t samples_since_last_sync = 0;
uint32_t ssls = 0;

void __not_in_flash_func(GrooveBox::Render)(int16_t* output_buffer, int16_t* input_buffer, size_t size)
{
    absolute_time_t renderStartTime = get_absolute_time();
    memset(workBuffer2, 0, sizeof(int32_t)*SAMPLES_PER_BLOCK*2);
    memset(toDelayBuffer, 0, sizeof(int16_t)*SAMPLES_PER_BLOCK);
    memset(toReverbBuffer, 0, sizeof(int16_t)*SAMPLES_PER_BLOCK);
    memset(output_buffer, 0, sizeof(int16_t)*SAMPLES_PER_BLOCK);
    last_input = 0;
    
    // update some song parameters
    delay.SetFeedback(songData.GetDelayFeedback());
    delay.SetTime(songData.GetDelayTime());
    bool hadExternalSync = false;
    //printf("input %i\n", workBuffer2[0]);
    for(int i=0;i<SAMPLES_PER_BLOCK;i++)
    {
        samples_since_last_sync++;
        int16_t input = input_buffer[i*2];
        // collapse input buffer so its easier to copy to the recording device
        // temporarily use the output buffer
        recordBuffer[i+recordBufferOffset] = input;
        
        if(!audio_sync_state){
            if(abs(input) > 0x6fff)
            {
                // need to recalculate the tempo phase here based on the number of samples that have passed since the last sync
                // samples since last sync is 1/8th notes, and we need to get up to 96ppq where the phase overflows at 31bits aka 0x7fffffff
                // 0x7fffffff * 1/(samples_since_last_sync / 2 / 96) - I'm not sure where the 32 is coming from here :/ I just tweaked it until it seemed right
                tempoPhaseIncrement = ((uint64_t)0x7fffffff*32*96)/samples_since_last_sync;
                ssls = samples_since_last_sync;
                samples_since_last_sync = 0;
                audio_sync_countdown = 0x4ff;
                audio_sync_state = true;
                hadExternalSync = true;
                if(waitingForSync)
                {
                    waitingForSync = false;
                    StartPlaying();
                }
            }
        }
        else
        {
            if(abs(input) < 0x1fff && samples_since_last_sync > 0xff)
            {
                audio_sync_state = false;
            }
        }
        if(playThroughEnabled)
        {
            if((songData.GetSyncInMode()&(SyncMode24|SyncModePO)) > 0)
            {
                workBuffer2[i*2] = input_buffer[i*2+1];
            }
            else
            {
                workBuffer2[i*2] = input_buffer[i*2];
            }
            workBuffer2[i*2+1] = input_buffer[i*2+1];
        }
        if(input<0)
        {
            input *= -1;
        }
        last_input = input>last_input?input:last_input;
        if(audio_sync_countdown>0)
            audio_sync_countdown--;
    }

    // our file system only handles appends every 256 bytes, so we need to respect that
    recordBufferOffset+=SAMPLES_PER_BLOCK;
    if(recordBufferOffset==128) recordBufferOffset = 0;
    if(songData.GetSyncInMode() == SyncModeNone)
        CalculateTempoIncrement();
    if(!recording)
    {
        for(int v=0;v<VOICE_COUNT;v++)
        {
            // put in a request to render the other voice on the second core
            memset(sync_buffer, 0, SAMPLES_PER_BLOCK);
            memset(workBuffer, 0, sizeof(int16_t)*SAMPLES_PER_BLOCK);
            bool hasSecondCore = false;

            {
                memset(workBuffer3, 0, sizeof(int16_t)*SAMPLES_PER_BLOCK);
                queue_entry_t entry = {false, v, sync_buffer, workBuffer3};
                queue_add_blocking(&signal_queue, &entry);
                hasSecondCore = true;
                v++;
            }
            // queue_add_blocking(&signal_queue, &entry);
            instruments[v].Render(sync_buffer, workBuffer, SAMPLES_PER_BLOCK);
            // block until second thread render complete
            if(hasSecondCore)
            {
                queue_entry_complete_t result;
                queue_remove_blocking(&renderCompleteQueue, &result);
            }

            // mix in the instrument
            for(int i=0;i<SAMPLES_PER_BLOCK;i++)
            {
                workBuffer2[i*2] += mult_q15(workBuffer[i], 0x7fff-instruments[v].GetPan());
                workBuffer2[i*2+1] += mult_q15(workBuffer[i], instruments[v].GetPan());
                toDelayBuffer[i] = add_q15(toDelayBuffer[i], mult_q15(workBuffer[i], ((int16_t)instruments[v].delaySend)<<7));
                toReverbBuffer[i] = add_q15(toReverbBuffer[i], mult_q15(workBuffer[i], ((int16_t)instruments[v].reverbSend)<<7));
                if(hasSecondCore)
                {
                    workBuffer2[i*2] += mult_q15(workBuffer3[i], 0x7fff-instruments[v-1].GetPan());
                    workBuffer2[i*2+1] += mult_q15(workBuffer3[i], instruments[v-1].GetPan());
                    toDelayBuffer[i] = add_q15(toDelayBuffer[i], mult_q15(workBuffer3[i], ((int16_t)instruments[v-1].delaySend)<<7));
                    toReverbBuffer[i] = add_q15(toReverbBuffer[i], mult_q15(workBuffer3[i], ((int16_t)instruments[v-1].reverbSend)<<7));
                }
            }
        }
        bool clipping = false;
        for(int i=0;i<SAMPLES_PER_BLOCK;i++)
        {
            int16_t* chan = (output_buffer+i*2);
            int32_t mainL = 0;
            int32_t mainR = 0;
            mainL = workBuffer2[i*2];
            mainR = workBuffer2[i*2+1];

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
            // lower verb volume
            lres *= 0xe0;
            rres *= 0xe0;
            lres = lres >> 8;
            rres = rres >> 8;
            mainL += l;
            mainR += r;

            mainL *= 0x40;
            mainR *= 0x40;
            mainL = mainL >> 8;
            mainR = mainR >> 8;


            // one more headroom clip
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

        if(IsPlaying())
        {
            bool tempoPulse = false;
            tempoPhase += tempoPhaseIncrement;
            if(songData.GetSyncInMode() == SyncModeNone)
            {
                if((tempoPhase >> 31) > 0)
                {
                    tempoPhase &= 0x7fffffff;
                    tempoPulse = true;
                }
            }
            if(tempoPulse)
            {
                OnTempoPulse();
            }
        }
    }
    absolute_time_t renderEndTime = get_absolute_time();
    int64_t currentRender = absolute_time_diff_us(renderStartTime, renderEndTime);
    renderTime += currentRender;
    sampleCount++;
    midi.Flush();
}
void GrooveBox::OnTempoPulse()
{
    syncsRequired++;
}
void GrooveBox::TriggerInstrument(uint8_t key, int16_t midi_note, uint8_t step, uint8_t pattern, bool livePlay, VoiceData &voiceData, int channel)
{
    // lets just simplify - gang together the instruments that are above each other
    // 1+5, 2+6, 9+13 etc
    voiceCounter = channel%4+(channel/8)*4;
    Instrument *nextPlay = &instruments[voiceCounter];
    voiceChannel[voiceCounter] = channel;
    //printf("playing file for voice: %i %x\n", channel, voiceData.GetFile());

    nextPlay->NoteOn(key, midi_note, step, pattern, livePlay, voiceData);
    return;    
}
void GrooveBox::TriggerInstrumentMidi(int16_t midi_note, uint8_t step, uint8_t pattern, VoiceData &voiceData, int channel)
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
    uint8_t _key = {0}; 
    nextPlay->NoteOn(_key, midi_note, step, pattern, true, voiceData);
    if(!foundVoice)
    {
        voiceChannel[voiceCounter] = channel;
        voiceCounter = (++voiceCounter)%VOICE_COUNT;
    }
}
bool GrooveBox::GetTrigger(uint voice, uint step, uint8_t &note, uint8_t &key)
{
    note = patterns[voice].GetNotesForPattern(GetCurrentPattern())[step];
    key = patterns[voice].GetKeysForPattern(GetCurrentPattern())[step];
    if(note >> 7 == 1)
    {
        note = note&0x7f;
        return true;
    }
    return false;
}
int GrooveBox::GetNote()
{
    int res = needsNoteTrigger;
    needsNoteTrigger = -1;
    return res;
}
void GrooveBox::OnCCChanged(uint8_t cc, uint8_t newValue)
{
    if(paramSelectMode && lastEditedParam > 0)
    {
        // set the midi mapping
        //lastEditedParam = param*2+1;
        midiMap.SetCCTarget(cc, currentVoice, lastEditedParam, lastKeyPlayed);
    }
    else
    {
        midiMap.UpdateCC(patterns, cc, newValue*2, GetCurrentPattern());
        ResetADCLatch(); // I'm not sure what this does :0 - but without it, we can't update the graphics :(
    }
}
bool GrooveBox::IsPlaying()
{
    return playing;
}

uint32_t screen_lfo_phase = 0;
void GrooveBox::LowBatteryDisplay(ssd1306_t *p)
{
    drawCount++;
    LowBatteryDisplayInternal(p);
    // after a few seconds, force shutdown the system
    const int sixseconds = 30*6;
    if(drawCount > sixseconds)
        hardware_shutdown();
}
void GrooveBox::LowBatteryDisplayInternal(ssd1306_t *p)
{
    // blink the low battery indicator for 2 seconds every 20 seconds
    const int eightseconds = 30*8;
    const int twoseconds = 30*2;
    if(drawCount%eightseconds<twoseconds)
    {
        // low battery display
        ssd1306_clear_square(p, 0, 0, 45, 16);
        ssd1306_draw_square_rounded(p, 2, 2, 36, 12);
        // bump at the top of the battery
        ssd1306_draw_square_rounded(p, 35, 5, 6, 6);
        ssd1306_clear_square_rounded(p, 34, 6, 6, 4);
        // clear center of battery
        ssd1306_clear_square_rounded(p, 3, 3, 34, 10);

        // battery fill
        if(drawCount%20>10)
            ssd1306_draw_square_rounded(p, 4, 4, 6, 8);
    }
}
void GrooveBox::SetGlobalParameter(uint8_t a, uint8_t b, bool setA, bool setB)
{
    // references must be initialized and can't change
    uint8_t* current_a = &songData.GetParam(param*2, GetCurrentPattern());
    // this needs to be special cased just for the octave setting - which is on a global param button, but stored
    // in the active voice
    if(param == 24)
    {
        current_a = &patterns[currentVoice].GetParam(param*2, lastKeyPlayed, GetCurrentPattern());
    }
    uint8_t& current_b = songData.GetParam(param*2+1, GetCurrentPattern());
    if(paramSetA)
    {
        *current_a = a;
        lastAdcValA = a;
    }
    if(paramSetB)
    {
        current_b = b;
        lastAdcValB = b;
    }
}

void GrooveBox::OnAdcUpdate(uint16_t a_in, uint16_t b_in)
{
    // interpolate the input values
    uint16_t interpolationbias = 0xd000; 
    AdcInterpolatedA = (((uint32_t)a_in)*(0xffff-interpolationbias) + ((uint32_t)AdcInterpolatedA)*interpolationbias)>>16;
    AdcInterpolatedB = (((uint32_t)b_in)*(0xffff-interpolationbias) + ((uint32_t)AdcInterpolatedB)*interpolationbias)>>16;
    uint8_t a = AdcInterpolatedA>>4;
    uint8_t b = AdcInterpolatedB>>4; 
    if(needsInitialADC > 0)
    {
        lastAdcValA = a;
        lastAdcValB = b;
        needsInitialADC--;
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
    if(holdingWrite && IsPlaying())
    {
        // only store the param lock if the voice has a trigger for this step
        uint8_t _key = {0};
        uint8_t _note;
        // for now only store parameter locks for steps that have triggers
        // eventually we might want to do "motion recording", but we will need to refactor the Instrument::UpdateVoiceData
        // to correctly request data for the currently playing step (rather than the step where the note was triggered)
        if(GetTrigger(currentVoice, patternStep[currentVoice], _note, _key))
        {
            if(paramSetA)
            {
                lastAdcValA = a;
                patterns[currentVoice].StoreParamLock(param*2, patternStep[currentVoice], GetCurrentPattern(), a);
                parameterLocked = true;
            }
            if(paramSetB)
            {
                lastAdcValB = b;
                patterns[currentVoice].StoreParamLock(param*2+1, patternStep[currentVoice], GetCurrentPattern(), b);
                parameterLocked = true;
            }
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
    uint8_t& current_a = patterns[currentVoice].GetParam(param*2, lastKeyPlayed, GetCurrentPattern());
    uint8_t& current_b = patterns[currentVoice].GetParam(param*2+1, lastKeyPlayed, GetCurrentPattern());
    bool voiceNeedsUpdate = false;
    if(paramSetA)
    {
        // eventually will need to encode the page number in the param - not there yet
        lastEditedParam = param*2;
        current_a = a;
        voiceNeedsUpdate = true;
        lastAdcValA = a;
    }
    if(paramSetB)
    {
        lastEditedParam = param*2+1;
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
            // TODO: FIX ME
            // patterns[recordingTarget].sampleLength[i] = 16;
            // patterns[recordingTarget].sampleStart[i] = i*16;
        }
    }   
}
void GrooveBox::StartPlaying()
{
    playing = true;
    if((songData.GetSyncOutMode()&SyncModeMidi) > 0)
    {
        midi.StopSequence();
        midi.StartSequence();
    }
    ResetPatternOffset();
}