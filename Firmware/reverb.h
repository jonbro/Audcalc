#ifndef REVERB_H_
#define REVERB_H_

#include "q15.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Delay {
    int16_t buf[494];
    int count;
    int length;
} Delay;
void Delay_init(Delay *delay, int length);
int16_t Delay_process(Delay *delay, int16_t in);
int16_t Delay_tap(Delay *delay, int tap);

typedef struct APFf
{
    float buf[2656];
    int count;
    int length;
    float c;
} APFf;
void APFf_init(APFf* ap, int length, float c);
float APFf_process(APFf* ap, float in);
float APFf_tap(APFf* apf, int tap);
typedef struct APF {
    int16_t buf[2656];
    int count;
    int length;
    int16_t c;
    uint32_t wobblePhase;
    uint32_t wobbleAdd;
    uint32_t wobbleAmp;

} APF;

void APF_init(APF *ap, int length, int16_t c, float wobbleSpeed, float wobbleAmp);
int16_t APF_tap(APF* apf, int tap);
int16_t APF_process(APF *ap, int16_t in);

typedef struct WarbleDelay {
    Delay delay;
    int16_t last;
    uint16_t triangle_phase;
    int32_t wobbleSpeed;
    uint32_t wobblePhase;
    uint32_t wobbleAdd;
    float wobbleAmp;
} WarbleDelay;
void WarbleDelay_init(WarbleDelay *d, int length, float wobbleSpeed, uint16_t wobbleAmp);
int16_t WarbleDelay_process(WarbleDelay *d, int16_t in);

typedef struct Reverb {
    APF initialAP[4];
    APF ringAP[4];
    WarbleDelay wDelay[2];
    q15_t lastDel[2];
    q15_t damp[2];
    q15_t delTime;
    q15_t dampAmount;
    int callCount;
    q15_t interpolator[2];
} Reverb;
void Reverb_init(Reverb *r);
void Reverb_process(Reverb *r, int16_t in, q15_t *out);

#ifdef __cplusplus
}
#endif

#endif // REVERB_H_