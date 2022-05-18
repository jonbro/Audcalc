#include "GrooveBox.h"
#include <string.h>
#define SAMPLES_PER_BUFFER 128

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return
            ((uint32_t) (r) << 8) |
            ((uint32_t) (g) << 16) |
            (uint32_t) (b);
}
static inline void u32_urgb(uint32_t urgb, uint8_t *r, uint8_t *g, uint8_t *b) {
    *r = (urgb>>8)&0xff;
    *g = (urgb>>16)&0xff;
    *b = (urgb)&0xff;
}

GrooveBox::GrooveBox(uint32_t *_color)
{
    Delay_init(&delay, 10000);
    midi.Init();
    for(int i=0;i<8;i++)
    {
        instruments[i].Init(&midi);
        if(i==0)
            instruments[i].SetType(INSTRUMENT_SAMPLE);
        else if(i==7)
        {
            instruments[i].SetType(INSTRUMENT_GLOBAL);
            instruments[7].globalParamSet = &globalParamSet;
        }
        else
            instruments[i].SetType(INSTRUMENT_MACRO);
        instruments[i].SetOscillator(MACRO_OSC_SHAPE_CSAW);
        instruments[i].SetAHD(4000, 1000, 20000);
        ffs_open(GetFilesystem(), &files[i], i);
        instruments[i].SetFile(&files[i]);
    }
    memset(trigger, 0, 16*16*16);
    memset(notes, 0, 16*16*16);
    color = _color;
}

int16_t workBuffer[SAMPLES_PER_BUFFER];
int16_t workBuffer2[SAMPLES_PER_BUFFER];
static uint8_t sync_buffer[SAMPLES_PER_BUFFER];
int16_t toDelayBuffer[SAMPLES_PER_BUFFER];
//absolute_time_t lastRenderTime = -1;
int16_t last_delay = 0;
int16_t last_input;
void GrooveBox::Render(int16_t* output_buffer, int16_t* input_buffer, size_t size)
{
    absolute_time_t renderStartTime = get_absolute_time();
    memset(output_buffer, 0, size);
    //memset(workBuffer2, 0, sizeof(int16_t)*SAMPLES_PER_BUFFER);
    last_input = 0;
    //printf("input %i\n", workBuffer2[0]);
    for(int i=0;i<SAMPLES_PER_BUFFER;i++)
    {

        int16_t input = workBuffer2[i] = input_buffer[i*2];
        if(input<0)
        {
            input *= -1;
        }
        last_input = input>last_input?input:last_input;
        toDelayBuffer[i] = mult_q15(workBuffer2[i], ((int16_t)globalParamSet.input_fx_send)<<6);
    }
    if(!recording)
    {
        for(int v=0;v<8;v++)
        {
            if(v==1 && recording)
                continue;
            memset(sync_buffer, 0, SAMPLES_PER_BUFFER);
            memset(workBuffer, 0, sizeof(int16_t)*SAMPLES_PER_BUFFER);
            instruments[v].Render(sync_buffer, workBuffer, SAMPLES_PER_BUFFER);
            // mix in the instrument
            for(int i=0;i<SAMPLES_PER_BUFFER;i++)
            {
                q15_t instrument = mult_q15(workBuffer[i], 0x4fff);
                workBuffer2[i] = add_q15(workBuffer2[i], workBuffer[i]);
                toDelayBuffer[i] = add_q15(toDelayBuffer[i], mult_q15(workBuffer[i], ((int16_t)instruments[v].delaySend)<<6));
            }
        }
        for(int i=0;i<SAMPLES_PER_BUFFER;i++)
        {
            int16_t delay_feedback = mult_q15(last_delay, 0x4fff);
            toDelayBuffer[i] = add_q15(delay_feedback, toDelayBuffer[i]);
            last_delay = Delay_process(&delay, toDelayBuffer[i]);
            workBuffer2[i] = add_q15(workBuffer2[i], last_delay);
        }
        for(int i=0;i<SAMPLES_PER_BUFFER;i++)
        {
            // this can all be done outside this loop?
            int requestedNote = GetNote();
            if(requestedNote >= 0)
            {
                instruments[currentVoice].NoteOn(requestedNote, requestedNote > 15?requestedNote:-1, true);
            }
            if(IsPlaying())
            {
                if(nextTrigger-- == 0)
                {
                    for(int v=0;v<8;v++)
                    {
                        int requestedNote = GetTrigger(v, CurrentStep);
                        if(requestedNote >= 0)
                        {
                            instruments[v].NoteOn(requestedNote, requestedNote > 15?requestedNote:-1, false);
                        }
                    }
                    UpdateAfterTrigger(CurrentStep);
                    CurrentStep = (++CurrentStep)%16;
                    if(CurrentStep==0)
                    {
                        chainStep = (++chainStep)%patternChainLength;
                    }
                    nextTrigger = 60u*44100u/globalParamSet.bpm/4;
                }
            }
            int16_t* chan = (output_buffer+i*2);
            chan[0] = workBuffer2[i];
            chan[1] = workBuffer2[i];
        }
    }

    if(recording)
    { 
        ffs_append(GetFilesystem(), &files[currentVoice], workBuffer2, SAMPLES_PER_BUFFER*2);
        for(int i=0;i<SAMPLES_PER_BUFFER;i++)
        {
            int16_t* chan = (output_buffer+i*2);
            chan[0] = workBuffer2[i];
            chan[1] = workBuffer2[i];
        }
    }
}
int GrooveBox::GetTrigger(uint voice, uint step)
{
    if(trigger[patternChain[chainStep]*256+voice*16+step])
        return notes[patternChain[chainStep]*256+voice*16+step];
    else
        return -1;
}
int GrooveBox::GetNote()
{
    int midi_note = midi.GetNote();
    int res = midi_note >= 0 ? midi_note : needsNoteTrigger;
    needsNoteTrigger = -1;
    return res;
}
void GrooveBox::UpdateAfterTrigger(uint step)
{
}

bool GrooveBox::IsPlaying()
{
    return playing;
}
void GrooveBox::UpdateDisplay(ssd1306_t *p)
{
    ssd1306_clear(p);
    char str[32];
    ssd1306_set_string_color(p, false);

    if(soundSelectMode)
    {
        sprintf(str, "Snd: %i", currentVoice);
        ssd1306_draw_string(p, 0, 8, 1, str);
    }
    else if(patternSelectMode)
    {
        sprintf(str, "Pat");
        ssd1306_draw_string(p, 0, 8, 1, str);
        if(patternChainLength>0)
        {
            for(int i=0;i<patternChainLength;i++)
            {
                int boxX = 24+(i%8)*12;
                int boxY = 6+12*(i/8);
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
    else if(paramSelectMode)
    {
        ssd1306_draw_string(p, 0, 8, 1, "Prm");
    }
    if(!patternSelectMode)
    {
        instruments[currentVoice].GetParamString(param, str);
        ssd1306_draw_string(p, 0, 24, 1, str);
    }
    if(recording)
    {
        int filesize = ffs_file_size(GetFilesystem(), &files[currentVoice]);

        sprintf(str, "Remaining: %i", (16*1024*1024-0x40000-filesize)/88100);
        ssd1306_draw_string(p, 0, 0, 1, str);
    }
    // grid
    for(int i=0;i<16;i++)
    {
        int x = i%4;
        int y = i/4;
        int key = x+(y+1)*5;
        if(CurrentStep == i && IsPlaying())
        {
            color[key] = urgb_u32(250, 30, 80);
        }
        else if(trigger[patternChain[chainStep]*256+currentVoice*16+i])
        {
            color[key] = urgb_u32(100, 60, 200);
        }
        else
        {
            // fade to black
            uint8_t r,g,b;
            u32_urgb(color[key], &r, &g, &b);
            const uint8_t fade_speed = 0xaf; 
            color[key] = urgb_u32(((uint16_t)r*fade_speed)>>8, ((uint16_t)g*fade_speed)>>8, ((uint16_t)b*fade_speed)>>8);
        }
    }
    // play light
    if(IsPlaying())
    {
        if(CurrentStep%4==0)
        {
            color[19] = urgb_u32(40, 200, 40);
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
    hardware_set_amp(globalParamSet.amp_enabled);
    ssd1306_draw_line(p, 0, 0, last_input>>8, 0);
}

void GrooveBox::OnAdcUpdate(uint8_t a, uint8_t b)
{
    if(abs((int16_t)lastAdcValA-(int16_t)a) > 2)
    {
        instruments[currentVoice].SetParameter(param*2, a);
        lastAdcValA = a;
    }
    if(abs((int16_t)lastAdcValB-(int16_t)b) > 2)
    {
        instruments[currentVoice].SetParameter(param*2+1, b);
        lastAdcValB = b;
    }
}

void GrooveBox::OnKeyUpdate(uint key, bool pressed)
{
    int x=key/5;
    int y=key%5;
    
    // erase mode
    if(x==4&&y==1)
    {
        erasing = true;
        //file_erase();
        erasing = false;
    }
    // record mode
    if(x==4&&y==0)
    {
        if(pressed)
        {
            recordingLength = 0;
            recording = true;
        }
        else
        {
            // finish recording
            recording = false;
        }
    }
    if(x<4 && y>0 && pressed)
    {
        
        int sequenceStep = x+(y-1)*4;
        if(soundSelectMode)
        {
            currentVoice = sequenceStep;
        }
        else if(paramSelectMode)
        {
            param = sequenceStep;
        }
        else if(patternSelectMode)
        {
            if(holdingWrite)
            {
                // copy pattern
                int currentPattern = patternChain[chainStep];
                
                for (size_t voice = 0; voice < 16; voice++)
                {
                    for (size_t step = 0; step < 16; step++)
                    {
                        notes[sequenceStep*256+voice*16+step] = notes[currentPattern*256+voice*16+step];
                        trigger[sequenceStep*256+voice*16+step] = trigger[currentPattern*256+voice*16+step];
                    }
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
                }
                if(patternChainLength<15)
                {
                    patternChain[patternChainLength++] = sequenceStep;
                }
            }
        }
        else if(!writing)
        {
            lastNotePlayed = needsNoteTrigger = sequenceStep;
        }
        else if(liveWrite)
        {
            bool setTrig = trigger[patternChain[chainStep]*256+currentVoice*16+CurrentStep] = 1;
            notes[patternChain[chainStep]*256+currentVoice*16+CurrentStep] = sequenceStep;
            color[x+y*5] = urgb_u32(5, 3, 20);
        }
        else
        {
            bool setTrig = trigger[patternChain[chainStep]*256+currentVoice*16+sequenceStep] = !trigger[patternChain[chainStep]*256+currentVoice*16+sequenceStep];
            if(setTrig)
            {
                notes[patternChain[chainStep]*256+currentVoice*16+sequenceStep] = lastNotePlayed;
            }
            color[x+y*5] = trigger[patternChain[chainStep]*256+currentVoice*16+sequenceStep]?urgb_u32(100, 0, 0):urgb_u32(0,0,0);
        }
    }
    // play
    if(x==4 && y==3 && pressed)
    {
        if(holdingWrite)
        {
            liveWrite = true;
            if(!playing)
            {
                playing = true;
                CurrentStep = 0;
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
                CurrentStep = 0;
            else
                UpdateAfterTrigger(-1);
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

    // pattern select
    if(x==1 && y==0)
    {
        patternSelectMode = pressed;
        if(pressed)
        {
            nextPatternSelected = false;
        }
    }

    // param select
    if(x==2 && y==0)
    {
        paramSelectMode = pressed;
    }
}
#define SAVE_SIZE 16*16*16

void GrooveBox::Serialize()
{
    Serializer s;
    // erase the existing file first
    ffs_file writefile;
    ffs_open(GetFilesystem(), &writefile, 101);
    ffs_erase(GetFilesystem(), &writefile);
    s.Init();
    uint8_t *n = notes;
    uint8_t *t = trigger;
    for(int i=0;i<SAVE_SIZE;i++)
    {
        s.AddData(*t);
        s.AddData(*n);
        n++;
        t++;
    }
    s.Finish();

}
void GrooveBox::Deserialize()
{
    // should probably put this is some different class
    Serializer s;
    s.Init();
    if(!s.writeFile.initialized)
        return;
    uint8_t *n = notes;
    uint8_t *t = trigger;
    for(int i=0;i<SAVE_SIZE;i++)
    {
        uint8_t tmp1, tmp2;
        *t = tmp1 = s.GetNextValue(); 
        *n = tmp2 = s.GetNextValue();
        t++;
        n++;
    } 
    // erase so we are prepared to write the file the next time
    s.Erase();
    // reload the trigger settings
    for (size_t i = 0; i < 16; i++)
    {
        color[i%4+(i/4)*5+5] = GetTrigger(currentVoice, i)?urgb_u32(3, 20, 7):urgb_u32(0,0,0);
    }
    
}