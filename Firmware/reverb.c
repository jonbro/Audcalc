#include "reverb.h"

void Delay_init(Delay *delay, int length)
{
    delay->count = 0;
    delay->length = length;
    for(int i=0;i<length;i++)
    {
        delay->buf[i] = 0;
    }
}

int16_t Delay_process(Delay *delay, int16_t in)
{
    int16_t delayed = delay->buf[delay->count];
    delay->buf[delay->count] = in;
    if (++delay->count > delay->length-1)
        delay->count = 0;
    return delayed;
}


int16_t Delay_tap(Delay *delay, int tap)
{
    int offset = tap+delay->count;
    while (offset < 0)
        offset += delay->length;
    while(offset > delay->length-1)
    {
        offset -= delay->length;
    }
    return delay->buf[offset];
}

/*
struct HPF {
   int16_t x0;
   int16_t x1;
   int16_t y0;
   int16_t y1;
   int16_t a0;
   int16_t a1;
   int16_t b;
};
void initHPF(HPF *hpf, int16_t cutoff)
{
    hpf->b = expf(-_2M_PI * cutoff * 1.0f/48000.0f);
    hpf->a0 = (1.0 + hpf->b) / 2.0;
    hpf->a1 = -hpf->a0;
}
int16_t processHPF(HPF *hpf, int16_t in)
{
    hpf->x0 = in;
    hpf->y0 = hpf->a0 * hpf->x0 + hpf->a1 * hpf->x1 + hpf->b * hpf->y1;
    hpf->y1 = hpf->y0;
    hpf->x1 = hpf->x0;
    return hpf->y0;
}

struct LPF {
   int16_t a;
   int16_t b;
   int16_t z;
};
void initLPF(LPF *lpf, int16_t cutoff)
{
    lpf->b = expf(-_2M_PI * cutoff * 1.0f/48000.0f);
    lpf->a = 1.0f - lpf->b;
}
int16_t processLPF(LPF *lpf, int16_t in)
{
    lpf->z =  lpf->a * in + lpf->z * lpf->b;
    return lpf->z;
}
*/
// -------- //
// ALL PASS //
// -------- //
void APFf_init(APFf* ap, int length, float c)
{
    ap->count = 0;
    ap->length = length;
    ap->c = c;
}
float APFf_process(APFf* ap, float in)
{
    float apdelayed = ap->buf[ap->count];
    float inSum = ap->buf[ap->count] = -ap->c * apdelayed + in;
    ap->count = (++ap->count) % ap->length;
    return inSum * ap->c + apdelayed;
}
float APFf_tap(APFf* apf, int tap)
{
    int offset = tap + apf->count;
    while (offset > apf->length)
    {
        offset -= apf->length;
    }
    return apf->buf[offset];
}
void APF_init(APF *ap, int length, int16_t c, float wobbleSpeed, float wobbleAmp)
{
    ap->count = 0;
    ap->length = length;
    ap->c = c;
    ap->wobbleAdd = 0xffffffff / (44100 * wobbleSpeed * 20);  
    ap->wobbleAmp = wobbleAmp;
}
int16_t APF_tap(APF* apf, int tap)
{
    int offset = tap + apf->count;
    while (offset > apf->length)
    {
        offset -= apf->length;
    }
    return apf->buf[offset];
}
int16_t APF_process(APF *ap, int16_t in)
{
    ap->wobblePhase += ap->wobbleAdd;
    q15_t sOut = sin_q15(ap->wobblePhase >> 17);
    sOut = add_q15(mult_q15(sOut, 0x3fff), 0x3fff);
    int32_t tapMult = (int32_t)sOut * ap->wobbleAmp;
    int16_t baseIndex = tapMult >> 15;
    uint32_t remainder = tapMult & 0xffff;
    q15_t a = APF_tap(ap, baseIndex);
    q15_t b = APF_tap(ap, baseIndex + 1);

    int16_t apdelayed = add_q15(((int32_t)a * remainder) >> 16, ((int32_t)a * (65535 - remainder) >> 16));
    //apdelayed = a;
    //int16_t apdelayed = ap->buf[ap->count];
    int32_t temp = -ap->c*apdelayed;
    temp = temp >> 16;
    int32_t inSum = ap->buf[ap->count] = temp+in;
    if (++ap->count > ap->length)
        ap->count = 0;
    temp = inSum*ap->c;
    return (temp>>16)+apdelayed;
}
void WarbleDelay_init(WarbleDelay *d, int length, float wobbleSpeed, uint16_t wobbleAmp)
{
    d->triangle_phase = 0;
    d->wobbleSpeed = wobbleSpeed;
    d->wobbleAdd = 0xffffffff / (44100*wobbleSpeed*20);
    d->wobbleAmp = wobbleAmp;
    d->wobblePhase = 0;
    Delay_init(&d->delay, length);
}
int16_t WarbleDelay_process(WarbleDelay *d, int16_t in)
{
    Delay_process(&d->delay, in);
    d->wobblePhase += d->wobbleAdd;
    q15_t sOut = sin_q15(d->wobblePhase >> 17);
    sOut = add_q15(mult_q15(sOut, 0x3fff), 0x3fff);
    int32_t tapMult = (int32_t)sOut * d->wobbleAmp;
    int16_t baseIndex = tapMult >> 15;
    uint32_t remainder = tapMult & 0xffff;
    q15_t a = Delay_tap(&d->delay, baseIndex);
    q15_t b = Delay_tap(&d->delay, baseIndex+1);
    //return a;
    return add_q15(((int32_t)a * remainder) >> 16, ((int32_t)a * (65535 - remainder) >> 16));

    //return add_q15(mult_q15(a, remainder), mult_q15(b, -remainder));
}
void Reverb_init(Reverb *r)
{
    r->delTime = 0x6fff;
    int16_t initialC = 0x3fff;
    r->lastDel[0] = r->lastDel[1] = 0;
    r->damp[0] = r->damp[1] = 0;
    APF_init(&r->initialAP[0], 113, initialC, 1, 0);
    APF_init(&r->initialAP[1], 162, initialC, 1, 0);
    APF_init(&r->initialAP[2], 241, initialC, 1, 0);
    APF_init(&r->initialAP[3], 399, initialC, 1, 0);
    int16_t ringC = -19660; // -.6
    ringC = initialC;
    APF_init(&r->ringAP[0], 908, ringC, 1, 0);
    APF_init(&r->ringAP[1], 1653, ringC, 1, 0);
    APF_init(&r->ringAP[2], 479, ringC, 1, 0);
    APF_init(&r->ringAP[3], 2656, ringC, 1, 0);
    WarbleDelay_init(&r->wDelay[0], 494, 1.4f*20, 200);
    WarbleDelay_init(&r->wDelay[1], 413, 1.7f*20, 200);
}


void Reverb_process(Reverb *r, int16_t in, q15_t *out)
{
    if((r->callCount++)%2==0)
    {
        out[0] = r->interpolator[0];
        out[1] = r->interpolator[1];
        r->interpolator[0] = in;
        return;
    }

    q15_t last = add_q15(mult_q15(in, 0x3fff), mult_q15(r->interpolator[0], 0x3fff));

    for (int j = 0; j < 4; j++)
    {
        last = APF_process(&r->initialAP[j], last);
    }
    q15_t initial = last;
    q15_t lastDelCopy[2];
    lastDelCopy[0] = r->lastDel[0];
    lastDelCopy[1] = r->lastDel[1];
    
    for (int j = 0; j < 2; j++)
    {
        last = add_q15(mult_q15(lastDelCopy[(j + 1) % 2], r->delTime), -initial);
        last = APF_process(&r->ringAP[j*2], last);
        last = APF_process(&r->ringAP[j*2 + 1], last);
        last = add_q15(mult_q15(last, r->dampAmount), mult_q15(r->damp[j], sub_q15(Q15_MAX, r->dampAmount)));
        r->damp[j] = last;
        last = WarbleDelay_process(&r->wDelay[j], last);
        r->lastDel[j] = last;
    }

    q15_t accumMult = 0x4ccc;
    q15_t accumulator = mult_q15(accumMult, APF_tap(&r->ringAP[0], 103));
    accumulator = sub_q15(accumulator, mult_q15(accumMult, APF_tap(&r->ringAP[1], 1391)));
    accumulator = sub_q15(accumulator, mult_q15(accumMult, Delay_tap(&r->wDelay[0].delay, 203)));
    r->interpolator[0] = out[0] = accumulator;
    
    accumulator = mult_q15(accumMult, APF_tap(&r->ringAP[2], 291));
    accumulator = sub_q15(accumulator, mult_q15(accumMult, APF_tap(&r->ringAP[3], 1391)));
    accumulator = sub_q15(accumulator, mult_q15(accumMult, Delay_tap(&r->wDelay[1].delay, 129)));
    r->interpolator[1] = out[1] = accumulator;
}
