
#include "audio/dsp.h"
#include "audio/resources.h"
#include "pico/stdlib.h"
#include "reverb.h"

using namespace braids;
#define VERB_LENGTH 8203
#define VERB_AP1 113
#define VERB_AP2 162
#define VERB_AP3 241
#define VERB_AP4 373

#define VERB_AP5 615
#define VERB_AP6 773
#define VERB_D1 915

#define VERB_AP7 513
#define VERB_AP8 849
#define VERB_D2 1515

typedef struct AllPassFilter {
    int16_t *buf;
    int count;
    int length;
    q15_t c;
    uint32_t phase;
    uint16_t phaseOffset;
} AllPassFilter;


class Reverb2 {
 public:
    Reverb2()
    {
        for(int i=0;i<10;i++)
        {
            allpass[i].count = 0;
            allpass[i].c = f32_to_q15(0.7f);
            allpass[i].phaseOffset = phaseOffset[i];
            allpass[i].phase = 0;
        }
        int bufCount = 0;
        allpass[0].buf = buf+bufCount; bufCount+= VERB_AP1; allpass[0].length = VERB_AP1;
        allpass[1].buf = buf+bufCount; bufCount+= VERB_AP2; allpass[1].length = VERB_AP2;
        allpass[2].buf = buf+bufCount; bufCount+= VERB_AP3; allpass[2].length = VERB_AP3;
        allpass[3].buf = buf+bufCount; bufCount+= VERB_AP4; allpass[3].length = VERB_AP4;

        allpass[4].buf = buf+bufCount; bufCount+= VERB_AP5; allpass[4].length = VERB_AP5;
        allpass[5].buf = buf+bufCount; bufCount+= VERB_AP6; allpass[5].length = VERB_AP6;
        allpass[6].buf = buf+bufCount; bufCount+= VERB_D1; allpass[6].length = VERB_D1;

        allpass[8].buf = buf+bufCount; bufCount+= VERB_AP7; allpass[8].length = VERB_AP7;
        allpass[9].buf = buf+bufCount; bufCount+= VERB_AP8; allpass[9].length = VERB_AP8;
        allpass[7].buf = buf+bufCount; bufCount+= VERB_D2; allpass[7].length = VERB_D2;
        position[0] = f32_to_q15(0.9f);
        position[1] = f32_to_q15(-0.9f);
        position[2] = f32_to_q15(1.f);
        position[3] = f32_to_q15(-1.f);
        dampAmount = f32_to_q15(0.9f);
        feedbackAmount = f32_to_q15(.87f);
    }
    inline void process(int16_t in, int16_t& l, int16_t& r)
    {
        for(int i=0;i<4;i++)
        {
            in = ProcessAllPass(in, &allpass[i]);
        }
        // low pass filter the feedback
        feedback = add_q15(mult_q15(feedback, dampAmount), mult_q15(damp[0], sub_q15(Q15_MAX, dampAmount)));
        damp[0] = feedback;

        in = ProcessAllPass(add_q15(in, mult_q15(feedbackAmount, feedback)), &allpass[4]);
        in = ProcessAllPass(in, &allpass[5]);
        in = DelayProcessWobble(in, &allpass[6]);

        in = ProcessAllPass(in, &allpass[8]);
        in = ProcessAllPass(in, &allpass[9]);
        feedback = DelayProcessWobble(in, &allpass[7]);

        int16_t dt1 = DelayTap(810, &allpass[6]);
        int16_t dt2 = DelayTap(611, &allpass[6]);
        int16_t dt3 = DelayTap(937, &allpass[7]);
        int16_t dt4 = DelayTap(1201, &allpass[7]);
        dt3 = mult_q15(dt2, f32_to_q15(0.8));
        dt4 = mult_q15(dt2, f32_to_q15(0.8));

        l = Mix(0, dt1, position[0]);
        r = Mix(0, dt1, 0xffff-position[0]);

        l = Mix(0, dt2, position[1]);
        r = Mix(0, dt2, 0xffff-position[1]);

        l = Mix(0, dt3, position[2]);
        r = Mix(0, dt3, 0xffff-position[2]);

        l = Mix(0, dt4, position[3]);
        r = Mix(0, dt4, 0xffff-position[3]);
    }
 private:
    inline int16_t ProcessAllPass(int16_t in, AllPassFilter *ap)
    {
        // ap->phase += ap->phaseOffset;
        // int16_t lfo = Interpolate824(wav_sine, ap->phase);
        // lfo = mult_q15(lfo, 0x3fff);
        // int32_t offset = ((int32_t)ap->length*(int32_t)(lfo>>1));
        // int16_t apdelayed = TapAp(offset>>16, ap);
        int16_t apdelayed = ap->buf[ap->count];
        int16_t inSum = ap->buf[ap->count] = add_q15(mult_q15(-ap->c, apdelayed), in);
        ap->count = (++ap->count) % ap->length;
        return add_q15(mult_q15(inSum, ap->c), apdelayed);
    }
    inline int16_t DelayWobbleLookup(AllPassFilter *d)
    {
        int16_t lfo = Interpolate824(wav_sine, d->phase);            
        int32_t offset = ((int32_t)d->length*(int32_t)(lfo>>1));
        int16_t a = DelayTap(offset>>16, d);
        int16_t b = DelayTap((offset>>16)+1, d);
        return Mix(a, b, offset&0xffff);
    }
    inline int16_t DelayProcessWobble(int16_t in, AllPassFilter *d)
    {
        DelayProcess(in, d);
        d->phase += d->phaseOffset;
        return DelayWobbleLookup(d);
    }
    inline int16_t DelayProcess(int16_t in, AllPassFilter *d)
    {
        int16_t delayed = d->buf[d->count];
        d->buf[d->count] = in;
        if (++d->count > d->length-1)
            d->count = 0;
        return delayed;
    }
    inline int16_t DelayTap(int tap, AllPassFilter *d)
    {
        int offset = tap+d->count;
        while (offset < 0)
            offset += d->length;
        while(offset > d->length-1)
        {
            offset -= d->length;
        }
        return d->buf[offset];
    }

    // int16_t TapAp(int tap, AllPassFilter *ap)
    // {
    //     int offset = tap+ap->count;
    //     while (offset < 0)
    //         offset += ap->length;
    //     while(offset > ap->length-1)
    //     {
    //         offset -= ap->length;
    //     }
    //     return ap->buf[offset];
    // }
    q15_t damp[2] = {0};
    q15_t dampAmount, feedbackAmount;
    uint16_t phaseOffset[10] = {803, 447, 1243, 1089, 304, 183, 4128, 481, 513, 713};

    AllPassFilter allpass[10];
    int16_t buf[VERB_LENGTH] = {0};
    int16_t feedback = 0;
    int count = 0;
    int length = VERB_LENGTH;
    
    uint16_t position[4] = {0x7fff, 0x7fff, 0x4000, 0xffff};
};