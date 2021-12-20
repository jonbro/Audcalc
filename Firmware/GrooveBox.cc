#include "GrooveBox.h"
#include <string.h>

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return
            ((uint32_t) (r) << 8) |
            ((uint32_t) (g) << 16) |
            (uint32_t) (b);
}
int GrooveBox::GetTrigger(uint voice, uint step)
{
    //memset(color, 0, sizeof(uint32_t)*25);
    return trigger[voice][step]?notes[voice][step]+48:-1;
}
void GrooveBox::UpdateAfterTrigger(uint step)
{
    for(int i=0;i<16;i++)
    {
        int x = i%4;
        int y = i/4;
        int key = x+(y+1)*5;
        if(step == i)
        {
            color[key] = urgb_u32(250, 30, 80);
        }
        else
            color[key] = trigger[currentVoice][i]?urgb_u32(100, 60, 200):urgb_u32(0,0,0);
    }
}

bool GrooveBox::IsPlaying()
{
    return playing;
}
int GrooveBox::GetNote()
{
    int res = needsNoteTrigger;
    if(res >= 0)
        res += 48;
    needsNoteTrigger = -1;
    return res;
}
GrooveBox::GrooveBox(uint32_t *_color)
{
    memset(trigger, 0, 16*16);
    memset(notes, 0, 16*16);
    color = _color;
}
void GrooveBox::UpdateDisplay(ssd1306_t *p)
{
    ssd1306_clear(p);
    char str[10];
    int16_t a = instrumentParamA[0];
    sprintf(str, "a:%d",a<<7);
    ssd1306_draw_string(p, 32, 24, 1, str);
}
void GrooveBox::OnAdcUpdate(uint8_t a, uint8_t b)
{
    instrumentParamA[currentVoice] = a;
    instrumentParamB[currentVoice] = b;
}
uint8_t GrooveBox::GetInstrumentParamA(int voice)
{
    return instrumentParamA[voice];
}
uint8_t GrooveBox::GetInstrumentParamB(int voice)
{
    return instrumentParamB[voice];
}

void GrooveBox::OnKeyUpdate(uint key, bool pressed)
{
    int x=key/5;
    int y=key%5;
    if(x<4 && y>0 && pressed)
    {
        
        int sequenceStep = x+(y-1)*4;
        if(soundSelectMode)
        {
            currentVoice = sequenceStep;
        }
        else if(!writing)
        {
            lastNotePlayed = needsNoteTrigger = sequenceStep;
        }
        else if(liveWrite)
        {
            bool setTrig = trigger[currentVoice][CurrentStep] = true;
            notes[currentVoice][CurrentStep] = sequenceStep;
            color[x+y*5] = urgb_u32(5, 3, 20);
        }
        else
        {
            bool setTrig = trigger[currentVoice][sequenceStep] = !trigger[currentVoice][sequenceStep];
            if(setTrig)
            {
                notes[currentVoice][sequenceStep] = lastNotePlayed;
            }
            color[x+y*5] = trigger[currentVoice][sequenceStep]?urgb_u32(5, 3, 20):urgb_u32(0,0,0);
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
}
