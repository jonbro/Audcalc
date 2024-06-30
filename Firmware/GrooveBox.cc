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
void OnSync()
{
    groovebox->OnMidiSync();
}
void OnStart()
{
    groovebox->OnMidiStart();
}
void OnContinue()
{
    groovebox->OnMidiContinue();
}
void OnStop()
{
    groovebox->OnMidiStop();
}
void OnPosition(uint16_t position)
{
    groovebox->OnMidiPosition(position);
}
void OnNoteOn(uint8_t channel, uint8_t note, uint8_t velocity)
{
    groovebox->OnMidiNote(note);
}
void OnNoteOff(uint8_t channel, uint8_t note, uint8_t velocity)
{

}

void GrooveBox::CalculateTempoIncrement()
{
    tempoPhaseIncrement = lut_tempo_phase_increment[songData.GetBpm()];
}
void GrooveBox::init(uint32_t *_color)
{
    // usbSerialDevice = _usbSerialDevice;
    needsInitialADC = 30;
    groovebox = this;
    midi.Init();

    midi.OnCCChanged = OnCCChangedBare;
    midi.OnSync = OnSync;
    midi.OnStart = OnStart;
    midi.OnStop = OnStop;
    midi.OnPosition = OnPosition;
    midi.OnContinue = OnContinue;
    
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
    Deserialize();
    //PrintLostLockData();
    // int lostLockCount = GetLostLockCount();
    // if(lostLockCount > 0)
    //     printf("\n************\nJUST LOST LOCKS TEH FUCK\n****************\n");
    // printf("lost lock count: %i\n", lostLockCount);
    // we do this in a second pass so the
    // deserialized pointers aren't pointing to the wrong places
    for(int i=0;i<16;i++)
    {
        ffs_open(GetFilesystem(), &files[i], (1<<8)|i);
        patterns[i].SetFile(&files[i]);
    }
    CalculateTempoIncrement();
    color = _color;
    lastKeyPlayed = {static_cast<unsigned int>(0 & 0xf)};
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

        int requestedNote = GetNote();
        if(requestedNote >= 0)
        {
            patterns[currentVoice].ClearNextRequestedStep();
            uint8_t _key = lastKeyPlayed;
            TriggerInstrument(_key, requestedNote, 0, 0, true, patterns[currentVoice], currentVoice);
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
            else if(songData.GetSyncInMode() == SyncModeMidi)
            {
                if((beatCounter[19]+1)%4 != 0)
                {
                    if((tempoPhase >> 31) > 0)
                    {
                        tempoPhase &= 0x7fffffff;
                        tempoPulse = true;
                    }
                }
            }
            else
            {
                if(hadExternalSync)
                {
                    if((beatCounter[19]+1)%48 == 0)
                    {
                        tempoPhase = 0;
                        tempoPulse = true;
                    }
                    else
                    {
                        // we got the external sync earlier than we were expecting, and need to do catchup
                        while(beatCounter[19] != 0)
                        {
                            OnTempoPulse();
                        }
                    }
                }
                // if we are running from an external clock, and the next pulse would be the clock pulse of the external sync, then wait for it to pulse
                else
                {
                    if((beatCounter[19]+1)%48 != 0)
                    {
                        if((tempoPhase >> 31) > 0)
                        {
                            tempoPhase &= 0x7fffffff;
                            tempoPulse = true;
                        }
                    }
                }
            }
            if(tempoPulse)
            {
                OnTempoPulse();
            }
        }
    }
    if((songData.GetSyncOutMode()&(SyncModePO|SyncMode24)) > 0)
    {
        for(int i=0;i<SAMPLES_PER_BLOCK;i++)
        {
            int16_t* chan = (output_buffer+i*2);
            chan[0] = 0;
        }
        if(sync_count > 0)
        {
            for(int i=0;i<SAMPLES_PER_BLOCK;i++)
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
    }

    if(recording)
    {
        if(recordBufferOffset == 0)
            ffs_append(GetFilesystem(), &files[recordingTarget], recordBuffer, 256);
        for(int i=0;i<SAMPLES_PER_BLOCK;i++)
        {
            int16_t* chan = (output_buffer+i*2);
            chan[0] = workBuffer2[i*2];
            chan[1] = workBuffer2[i*2+1];
        }
    }
    absolute_time_t renderEndTime = get_absolute_time();
    int64_t currentRender = absolute_time_diff_us(renderStartTime, renderEndTime);
    renderTime += currentRender;
    sampleCount++;
    midi.Flush();
}

void GrooveBox::OnMidiNote(int note)
{
    if( note >= 0)
    {
        patterns[currentVoice].ClearNextRequestedStep();
        TriggerInstrumentMidi(note, 0, 0, patterns[currentVoice], currentVoice);
    }
}

void GrooveBox::OnMidiSync()
{
    if(songData.GetSyncInMode() != SyncModeMidi)
        return;
    // need to recalculate our samples since last sync, etc
    tempoPhaseIncrement = ((uint64_t)0x7fffffff*16*96)/samples_since_last_sync;
    if(!IsPlaying())
        return;
    if((beatCounter[19]+1)%8 == 0)
    {
        tempoPhase = 0;
        OnTempoPulse();
    }
    else
    {
        // we got the external sync earlier than we were expecting, and need to do catchup
        while((beatCounter[19]+1)%8 != 0)
        {
            OnTempoPulse();
        }
    }
}
void GrooveBox::OnMidiStart()
{
    if(songData.GetSyncInMode() != SyncModeMidi)
        return;
    StartPlaying();
}
void GrooveBox::OnMidiStop()
{
    if(songData.GetSyncInMode() != SyncModeMidi)
        return;
    StopPlaying();
}
void GrooveBox::OnMidiContinue()
{
    if(songData.GetSyncInMode() != SyncModeMidi)
        return;
    ContinuePlaying();
}
void GrooveBox::OnMidiPosition(uint16_t position)
{
    if(songData.GetSyncInMode() != SyncModeMidi)
        return;
    // position is given in 16th notes, so we need to tempo pulse up to the current song position from zero
    // * 6 to get 24ppq, * 4 to get 96 ppq
    ResetPatternOffset();
    position = position * 6 * 4;
    chainStep = 0;
    for(int i=0;i<position;i++)
    {
        OnTempoPulse(true);
    }
}


void GrooveBox::OnTempoPulse(bool advanceOnly)
{
    bool needsMidiSync = false;
    // send the sync pulse to all the instruments
    if(!advanceOnly)
    {
        for(int v=0;v<VOICE_COUNT;v++)
        {
            instruments[v].TempoPulse(patterns[v]);
        }
    }
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
    // four extra counters
    // 17: the pattern change counter
    // 18: the pulse sync output counter
    // 19: the midi sync
    // 20: input pulse counter
    for(int v=0;v<20;v++)
    {
        bool needsTrigger = beatCounter[v]==0;
        beatCounter[v]++;
        uint8_t rate = 0;
        switch(v)
        {
            case 16: // global pattern (pattern change counter) always uses 16th notes
                rate = 2;
                break;
            case 17: // pocket operator / pulse sync
                if((songData.GetSyncOutMode() & SyncModePO) > 0)
                    rate = 4;
                else if((songData.GetSyncOutMode() & SyncMode24) > 0)
                    rate = 7;
                else
                    rate = 0;
                break;
            case 18: // midi sync
                rate = 7;
                break;
            case 19: // input sync
                rate = 8;
                break;
            default:
                rate = ((patterns[v].GetRateForPattern(GetCurrentPattern())*7)>>8);
        }
        beatCounter[v] = beatCounter[v]%getTickCountForRateIndex(rate);
        if(!needsTrigger)
            continue;
        if(v==17){
            sync_count = 6;
            continue;
        }
        if(v==18)
        {
            needsMidiSync = true;
            continue;
        }
        if(v==19)
        {
            continue;
        }
        // never trigger for the global pattern
        uint8_t requestedNote;
        uint8_t requestedKey = {0};
        if(!advanceOnly && v!=16 && v!=17 && GetTrigger(v, patternStep[v], requestedNote, requestedKey))
        {
            hadTrigger = hadTrigger|(1<<v);
            if((allowPlayback>>v)&0x1)
            {
                bool trigger = true;
                // check to see if the conditions allow us to play
                ConditionModeEnum conditionMode = patterns[v].GetConditionMode(patterns[v].GetParamValue(ConditionMode, requestedKey, patternStep[v], GetCurrentPattern()));
                uint8_t conditionData = patterns[v].GetParamValue(ConditionData, requestedKey, patternStep[v], GetCurrentPattern());
                switch(conditionMode)
                {
                    case CONDITION_MODE_RAND:
                        if(conditionData < (get_rand_32()>>24))
                            trigger = false;
                        break;
                    case CONDITION_MODE_LENGTH:
                        uint8_t tmp = ((uint16_t)conditionData*35)>>8;
                        trigger = ConditionalEvery[tmp*2]-1 == patternLoopCount[v]%ConditionalEvery[tmp*2+1];
                        break;
                }
                if(trigger)
                {
                    patterns[v].SetNextRequestedStep(patternStep[v]);
                    TriggerInstrument(requestedKey, requestedNote, patternStep[v], GetCurrentPattern(), false, patterns[v], v);
                }
            }
        }
        // we need to special case 16: the pattern change counter
        if(v==16)
        {
            patternStep[v] = (patternStep[v]+1)%songData.GetLength(GetCurrentPattern());
        }
        else
        {
            patternStep[v] = (patternStep[v]+1)%(patterns[v].GetLength(GetCurrentPattern()));
            if(patternStep[v] == 0)
            {
                patternLoopCount[v]++;
            }
        }
    }
    if(!advanceOnly && needsMidiSync && ((songData.GetSyncOutMode()&SyncModeMidi) > 0))
    {
        midi.TimingClock();
    }
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
void GrooveBox::SaveAndShutdown()
{
    // store song here
    erasing = true;
    driver_set_mute(true);
    Serialize();
    hardware_shutdown();
}
void GrooveBox::UpdateDisplay(ssd1306_t *p)
{
    drawCount++;
    framesSinceLastTouch++;
    screen_lfo_phase += 0x2ffffff;

    // 5 minutes & 20 minutes 
    #define TWOMINS 0x1c20>>1
    #define FIVEMINS 0x4650>>1
    #define TWENTYMINS 0x11940>>1
    bool disableAutoShutdown = false;
    if(!disableAutoShutdown && (!IsPlaying() && framesSinceLastTouch > TWENTYMINS))
    {
        SaveAndShutdown();
    }
    // if(usbSerialDevice->NeedsSongData())
    // {
    //     SerializeToSerial();
    // }
    // if(usbSerialDevice->PrepareReceiveData())
    // {
    //     DeserializeFromSerial();
    // }


    ssd1306_clear(p);
    char str[64];
    ssd1306_set_string_color(p, false);
    // if(audio_sync_countdown > 0)
    // {
    //     ssd1306_draw_square(p, 0,0,3,3);
    // }

    //printf("raw: 0x%03x, volt: %f V\n", result, result * conversion_factor * 2.3368f);
    // 3.1f here is a number that I just typed in until it lined up correctly with the measurement from the board
    // but I don't actually understand if this is the correct number or not?
    // and even worse, at one point I had 2.34 in here, and I assume that number came from the same procedure
    // so whats the correct number? no idea.

    // sprintf(str, "%ir%02x, v%f",  hardware_has_usb_power()?1:0, hardware_get_battery_level(), hardware_get_battery_level() * conversion_factor * 3.1f);
    // ssd1306_draw_string_gfxfont(p, 3, 12, str, true, 1, 1, &m6x118pt7b);
    // // sprintf(str, "%i",);
    // // ssd1306_draw_string_gfxfont(p, 3, 12, str, true, 1, 1, &m6x118pt7b);
    if(clearTime > 0 && holdingEscape)
    {
        sprintf(str, "clear pat %i in %i", GetCurrentPattern()+1, clearTime/30);
        clearTime--;
        if(clearTime == 0)
        {
            // empty the current pattern
            for (size_t voice = 0; voice < 16; voice++)
            {
                for (size_t j = 0; j < 64; j++)
                {
                    patterns[voice].GetNotesForPattern(GetCurrentPattern())[j] = 0;
                }
            }
        }
        ssd1306_draw_string_gfxfont(p, 3, 12, str, true, 1, 1, &m6x118pt7b);
        return;
    }
    else if(holdingArm && holdingEscape)
    {
        sprintf(str, "press red to erase sample");
        ssd1306_draw_string_gfxfont(p, 3, 12, str, true, 1, 1, &m6x118pt7b);
        for(int i=0;i<16;i++)
        {
            int x = i%4;
            int y = i/4;
            int key = x+(y+1)*5;
            if(files[i].initialized)
            {
                color[key] = urgb_u32(200, 50, 50);
            }
            else
            {
                color[key] = urgb_u32(0,0,0);
            }
        }
        return;
    }
    else if(erasing)
    {
        ssd1306_clear(p);
        sprintf(str, "ERASING...");
        // ssd1306_draw_square_rounded(p, 0, 17, width, 15);
        ssd1306_draw_string_gfxfont(p, 3, 12, str, true, 1, 1, &m6x118pt7b);
        return;
    }
    else if(holdingArm && !recording)
    {
        sprintf(str, "hold red to sample");
        // ssd1306_draw_square_rounded(p, 0, 17, width, 15);
        ssd1306_draw_string_gfxfont(p, 3, 12, str, true, 1, 1, &m6x118pt7b);
        ssd1306_draw_square(p, 0, 17, last_input>>8, 32-17);
        for(int i=0;i<16;i++)
        {
            int x = i%4;
            int y = i/4;
            int key = x+(y+1)*5;
            if(!files[i].initialized)
            {
                color[key] = urgb_u32(200, 50, 50);
            }
            else
            {
                color[key] = urgb_u32(0,0,0);
            }
        }
        return;
    }
    else if(shutdownTime > 0 && holdingEscape)
    {
        sprintf(str, "shutdown in %i", shutdownTime/30);
        ssd1306_draw_string_gfxfont(p, 3, 12, str, true, 1, 1, &m6x118pt7b);
        shutdownTime--;  
        if(shutdownTime == 0)
        {
            SaveAndShutdown();
        }
        return;
    }
    else if(clearTime == 0 && patternSelectMode && holdingEscape)
    {
        sprintf(str, "pat %i cleared", GetCurrentPattern()+1);
        ssd1306_draw_string_gfxfont(p, 3, 12, str, true, 1, 1, &m6x118pt7b);
        return;
    }
    else if(recording)
    {
        uint32_t allFileSizes = 0;
        for(int i=0;i<16;i++)
        {
            allFileSizes += ffs_file_size(GetFilesystem(), &files[i]);
        }
        sprintf(str, "Sampling to %i", recordingTarget+1);
        ssd1306_draw_string_gfxfont(p, 3, 12, str, true, 1, 1, &m6x118pt7b);
        sprintf(str, "%i Secs Remaining", (16*1024*1024-0x40000-allFileSizes)/64000);
        ssd1306_draw_string_gfxfont(p, 3, 17+12, str, true, 1, 1, &m6x118pt7b);
        return;
    }
    else if(patternSelectMode && holdingWrite)
    {
        sprintf(str, "copy pat %i to", GetCurrentPattern()+1);
        ssd1306_draw_string_gfxfont(p, 3, 12, str, true, 1, 1, &m6x118pt7b);
        return;
    }
    else if(soundSelectMode && holdingWrite)
    {
        sprintf(str, "copy voice %i to", currentVoice+1);
        ssd1306_draw_string_gfxfont(p, 3, 12, str, true, 1, 1, &m6x118pt7b);
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
        // sprintf(str, "%i", AdcInterpolatedA>>2);
        // ssd1306_draw_square_rounded(p, 0, 17, width, 15);
        ssd1306_draw_string_gfxfont(p, 3, 17+12, str, true, 1, 1, &m6x118pt7b);
    }

    // copy pattern

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
    else
    {
        if(selectedGlobalParam)
        {
            // special casing the octave display
            songData.DrawParamString(param, GetCurrentPattern(), str, patterns[currentVoice].GetOctave());
        }
        else
        {
            uint8_t paramLockSignal = storingParamLockForStep;
            if(holdingWrite)
                paramLockSignal = patternStep[currentVoice];
            patterns[currentVoice].DrawParamString(param, str, lastKeyPlayed, GetCurrentPattern(), 0x7f&paramLockSignal, 0x80&storingParamLockForStep);
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
        else if(holdingArm && !recording)
        {
            if(&files[i].filesize == 0)
            {
                color[key] = urgb_u32(200, 50, 50);
            }
        }
        else
        {
            uint8_t note;
            uint8_t _key = {0};
            if((patternStep[currentVoice]-1<0?patterns[currentVoice].GetLength(GetCurrentPattern())-1:patternStep[currentVoice]-1)-editPage[currentVoice]*16 == i && IsPlaying())
            {
                color[key] = urgb_u32(100, 100, 100);
            }
            else if(GetTrigger(currentVoice, i+16*editPage[currentVoice], note, _key))
            {
                // has a parameter lock
                if(patterns[currentVoice].HasAnyLockForStep(i+16*editPage[currentVoice], GetCurrentPattern()))
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
    if(!hardware_has_usb_power())
    {
        float bLev = hardware_get_battery_level_float();
        if(bLev < 3.6f)
        {
            SaveAndShutdown();
        }
        else if(bLev < 3.7f)
        {
            LowBatteryDisplayInternal(p);
        }
        
    }
    if(soundSelectMode)
    {
        powerHoldTime++;
        if(powerHoldTime > 30*2)
        {
            ssd1306_clear_square(p, 0, 0, 45, 16);
            sprintf(str, "v:%.2f",  hardware_get_battery_level_float());
            ssd1306_draw_string_gfxfont(p, 3, 12, str, true, 1, 1, &m6x118pt7b);
        }
    }
    else
    {
        powerHoldTime = -1;
    }

    // uint16_t param = instruments[currentVoice%4+(currentVoice/8)*4].pWithMods;

    // ssd1306_draw_square(p, 0, 0, param>>8, 1);
    // ssd1306_clear_square(p,0,0,128,32);
    // sprintf(str, "%i", ssls);
    // ssd1306_draw_string(p, 0,0, 1, str);

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
    ResetPatternOffset();
    ContinuePlaying();
}
void GrooveBox::ContinuePlaying()
{
    playing = true;
    if((songData.GetSyncOutMode()&SyncModeMidi) > 0)
    {
        midi.StopSequence();
        midi.StartSequence();
    }
}
void GrooveBox::StopPlaying()
{
    playing = false;
    for(int i=0;i<VOICE_COUNT;i++)
    {
        instruments[i].ClearRetriggers();
    }
    if((songData.GetSyncOutMode()&SyncModeMidi) > 0)
        midi.StopSequence();
}
void GrooveBox::OnKeyUpdate(uint key, bool pressed)
{
    framesSinceLastTouch = 0;
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
        editPage[currentVoice] = (++editPage[currentVoice])%(patterns[currentVoice].GetLengthForPattern(GetCurrentPattern())/64+1);
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
            uint8_t note;
            uint8_t key = {0};
            if(GetTrigger(currentVoice, sequenceStep+editPage[currentVoice]*16, note, key))
            {
                if(!parameterLocked){
                    patterns[currentVoice].GetNotesForPattern(GetCurrentPattern())[sequenceStep+editPage[currentVoice]*16] = 0;
                    patterns[currentVoice].RemoveLocksForStep(GetCurrentPattern(), sequenceStep);
                }
            }
            else
            {
                patterns[currentVoice].GetNotesForPattern(GetCurrentPattern())[sequenceStep+editPage[currentVoice]*16] = (0x7f&lastNotePlayed)|0x80;
                patterns[currentVoice].GetKeysForPattern(GetCurrentPattern())[sequenceStep+editPage[currentVoice]*16] = lastKeyPlayed;
            }
        }

    }
    if(x<4 && y>0 && pressed)
    {
        int sequenceStep = x+(y-1)*4;
        if(holdingEscape && holdingArm)
        {
            if(pressed && files[sequenceStep].initialized)
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
            if(pressed && !recording && !files[sequenceStep].initialized)
            {
                recordingLength = 0;
                recordingTarget = sequenceStep;
                recording = true;
            }
            else if(recording)
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
            uint8_t targetParam = x+y*5;
            // hardcode handlers for the delay edit
            selectedGlobalParam = false;
            if(targetParam == 19 || targetParam == 22)
            {
                switch(param)
                {
                    case 22:
                        targetParam = 22+25;
                        selectedGlobalParam = true;
                        break;
                }
            }
            if(targetParam != param)
                paramSetA = paramSetB = false;
            param = targetParam;
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
                    patterns[voice].CopyPattern(currentPattern, sequenceStep);
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
                        ResetPatternOffset();
                    }
                    patternChain[patternChainLength++] = sequenceStep;
                }
            }
        }
        else if(!writing)
        {
            lastNotePlayed = needsNoteTrigger = songData.GetNote(sequenceStep, patterns[currentVoice].GetOctave());
            lastKeyPlayed = sequenceStep;
            ResetADCLatch();
        }
        else if(liveWrite)
        {
            uint16_t noteToModify = patternStep[currentVoice];
            uint8_t newValue = (0x7f&songData.GetNote(sequenceStep, patterns[currentVoice].GetOctave()))|0x80;
            patterns[currentVoice].GetNotesForPattern(patternChain[chainStep])[noteToModify] = newValue;
            patterns[currentVoice].GetKeysForPattern(patternChain[chainStep])[noteToModify] = sequenceStep;
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
            uint8_t targetParam = x+y*5;
            if(selectedGlobalParam && param == targetParam)
            {
                targetParam = (targetParam+25)%50;
            }
            if(param != targetParam)
                paramSetA = paramSetB = false;
            selectedGlobalParam = true;
            param = targetParam;
        } 
        else if(holdingWrite)
        {
            liveWrite = true;
            if(!playing)
            {
                if((songData.GetSyncInMode()&(SyncMode24|SyncModePO)) != 0)
                {
                    waitingForSync = true;
                }
                else
                {
                    StartPlaying();
                }
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
            if(!playing)
            {
                if((songData.GetSyncInMode()&(SyncMode24|SyncModePO)) != 0)
                {
                    waitingForSync = true;
                }
                else
                {
                    StartPlaying();
                }
            }
            else
            {
                StopPlaying();
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
        if(pressed && paramSelectMode)
        {
            // just hardcode for now
            uint8_t targetParam = x+y*5;
            // record key param 1: 24
            // record key param 2: 49
            switch(param)
            {
                case 24:
                    targetParam = 49;
                    break;
                default:
                    targetParam = 24;
                    break;
            }
            if(param != targetParam)
                paramSetA = paramSetB = false;
            selectedGlobalParam = true;
            param = targetParam;
        }
        else{
            holdingWrite = pressed;
            if(pressed)
            {
                // need to clear param sets so we don't immediately start doing parameter records
                paramSetA = paramSetB = false;
                liveWrite = false;
                writing = !writing;
                color[x+y*5] = writing?urgb_u32(20, 10, 12):urgb_u32(0,0,0);
            }
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
        shutdownTime = 30*3;
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
                clearTime = 90;
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

/* FILELIST
*  --------
*  0xffff: global data (last playing song)
*  0-15: song data
*  (1<<8)&0-15: sample data
*/
#define GLOBAL_DATA_FILEID 0x7fff
bool serialize_callback(pb_ostream_t *stream, const uint8_t *buf, size_t count)
{
   Serializer *s = (Serializer*) stream->state;
    for (size_t i = 0; i < count; i++)
    {
        s->AddData(buf[i]);
    }
    return true;
}
void GrooveBox::Serialize()
{
    // copy the floating bits of data
    songData.SetPlayingPattern(playingPattern);
    songData.StorePatternChain(patternChain);
    songData.SetPatternChainLength(patternChainLength);

    // erase the existing file first
    ffs_file globalDataFile;
    erasing = true;
    ffs_open(GetFilesystem(), &globalDataFile, GLOBAL_DATA_FILEID);
    ffs_erase(GetFilesystem(), &globalDataFile);
    erasing = false;
    
    Serializer globalDataSerializer;
    globalDataSerializer.Init(GLOBAL_DATA_FILEID);

    pb_ostream_t serializerStream = {&serialize_callback, &globalDataSerializer, SIZE_MAX, 0};
    pb_encode_ex(&serializerStream, GlobalData_fields, &globalData, PB_ENCODE_DELIMITED);
    globalDataSerializer.Finish();

    ffs_file songFile;
    erasing = true;
    ffs_open(GetFilesystem(), &songFile, globalData.songId);
    ffs_erase(GetFilesystem(), &songFile);
    erasing = false;

    Serializer s;
    s.Init(0);
    serializerStream = {&serialize_callback, &s, SIZE_MAX, 0};
    songData.Serialize(&serializerStream);

    for(int i=0;i<16;i++)
    {
        patterns[i].Serialize(&serializerStream);
    }
    serializerStream.bytes_written = 0;
    VoiceData::SerializeStatic(&serializerStream);

    s.Finish();
    printf("serialized song file size %i\n", s.writeFile.filesize);
}

bool serialize_to_serial_callback(pb_ostream_t *stream, const uint8_t *buf, size_t count)
{
//    USBSerialDevice *s = (USBSerialDevice*) stream->state;
//    s->SendData(buf, count);
   return true;
}

void GrooveBox::SerializeToSerial()
{
    // usbSerialDevice->ResetLineBreak();
    // printf("Here comes songdata:\n");
    // pb_ostream_t serializerStream = {&serialize_to_serial_callback, usbSerialDevice, SIZE_MAX, 0};
    // songData.Serialize(&serializerStream);
    // for(int i=0;i<16;i++)
    // {
    //     patterns[i].Serialize(&serializerStream);
    // }
    // serializerStream.bytes_written = 0;
    // VoiceData::SerializeStatic(&serializerStream);
    // usbSerialDevice->SignalSongDataComplete();
    // const char readyMsg[7] = "/ready";
    // usbSerialDevice->SendData((uint8_t*)&readyMsg, 7);
    // printf("completed send.\n");
}

bool deserialize_callback(pb_istream_t *stream, uint8_t *buf, size_t count)
{
    Serializer *s = (Serializer*) stream->state;
    for (size_t i = 0; i < count; i++)
    {
        buf[i] = s->GetNextValue();
    }
    return true;
}

void GrooveBox::Deserialize()
{
    // setup our song data
    songData.InitDefaults();
    
    // should probably put this is some different class
    Serializer globalDataSerializer;
    globalDataSerializer.Init(GLOBAL_DATA_FILEID);
    // my first migration?
    if(!globalDataSerializer.writeFile.initialized)
    {
        globalData.version = 1;
        // erase all the old sample data that is living where the song data now lives
        for(int i=0;i<16;i++)
        {
            ffs_file sampleFile;
            erasing = true;
            ffs_open(GetFilesystem(), &sampleFile, i);
            ffs_erase(GetFilesystem(), &sampleFile);
            erasing = false;
        }
        // clear out the old songfile
        {
            erasing = true;
            ffs_file preProtobufSong;
            ffs_open(GetFilesystem(), &preProtobufSong, 101);
            ffs_erase(GetFilesystem(), &preProtobufSong);
            erasing = false;
        }
        return;
    }

    // because we don't have a "real" song id, we can just rely on the zero'd data - which stores the song id in position 0
    Serializer s;
    s.Init(globalData.songId);
    pb_istream_t serializerStream = {&deserialize_callback, &s, SIZE_MAX};
    songData.Deserialize(&serializerStream);

    // load pattern data
    for(int i=0;i<16;i++)
    {
        patterns[i].Deserialize(&serializerStream);
    }
    VoiceData::DeserializeStatic(&serializerStream);
    
    playingPattern = songData.GetPlayingPattern();
    songData.LoadPatternChain(patternChain);
    patternChainLength = songData.GetPatternChainLength();

    printf("loaded filesize %i\n", s.writeFile.filesize);
}

bool deserialize_from_serial_callback(pb_istream_t *stream, uint8_t *buf, size_t count)
{
    // USBSerialDevice *s = (USBSerialDevice*) stream->state;
    // if(!s->GetData(buf, count))
    //     return false;
    return true;
}

void GrooveBox::DeserializeFromSerial()
{
    // // setup our song data
    // songData.InitDefaults();

    // // because we don't have a "real" song id, we can just rely on the zero'd data - which stores the song id in position 0
    // pb_istream_t serializerStream = {&deserialize_from_serial_callback, usbSerialDevice, SIZE_MAX};
    // // send the signal that we are ready to recieve
    // const char readyMsg[7] = "/ready";
    // usbSerialDevice->SendData((uint8_t*)&readyMsg, 7);

    // songData.Deserialize(&serializerStream);
    // // load pattern data
    // for(int i=0;i<16;i++)
    // {
    //     patterns[i].Deserialize(&serializerStream);
    // }
    // VoiceData::DeserializeStatic(&serializerStream);
    // usbSerialDevice->SignalSongReceiveComplete();
    // printf("finished deserialize from serial\n");
    // ResetADCLatch();
}