
#pragma once

#include "audio/dsp.h"
#include "audio/resources.h"
#include "pico/stdlib.h"
#include "GlobalDefines.h"

using namespace braids;
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

// the ten lines are in one shared buffer, add their sizes so the buffer size
// exactly matches the requried length
#define VERB_LENGTH (VERB_AP1+VERB_AP2+VERB_AP3+VERB_AP4+VERB_AP5+VERB_AP6+VERB_D1+VERB_AP7+VERB_AP8+VERB_D2)

// the loop gain at the top of the feedback control - kept below unity so that a
// fully open knob is a very long tail rather than a runaway that never clears.
// two caps to pick between by ear: HIGH is a ~42s tail, LOW is ~14s and clips the
// output about half as often for the same send
#define VERB_FEEDBACK_CAP_HIGH 31129 // 0.95, RT60 ~42s
#define VERB_FEEDBACK_CAP_LOW  27853 // 0.85, RT60 ~14s
// the darkest the damping lowpass is allowed to get
#define VERB_MIN_DAMP 1638 // 0.05
// how far the modulated allpasses swing, in samples - the allpass lengths need 
// to be double this
#define VERB_MAX_MOD_DEPTH 48
// the tank runs on a signal shifted down by this much and the output taps shift it
// back up. this is what keeps the allpass sum (-0.7*delayed + in, so up to 1.7x)
// inside q15 - without it the recursion clips internally, which sounds far worse
// than clipping at the output. deliberately a fixed amount and NOT tied to the
// feedback setting: how hard the tank gets driven is the player's call, through the
// send and feedback controls. drive it hard enough and the output clamp will clip
#define VERB_HEADROOM_SHIFT 2

// the reverb loop only runs on every other sample, and a full wrap of the 32 bit
// phase is one trip around wav_sine, convert hundredths of a Hz to a
// phase increment
#define VERB_LFO_INC(hz_x100) \
    ((uint32_t)(((uint64_t)(hz_x100) * 0x100000000ULL) / (100ULL * (AUDIO_SAMPLE_RATE / 2))))

typedef struct AllPassFilter {
    int16_t *buf;   // first sample of this line's slice of the shared buffer
    int16_t *end;   // one past the last sample - what the wrap compares against
    int16_t *write; // walks along the slice and resets when it runs off the end
    int length;
    q15_t c;
    uint32_t phase;
    uint32_t phaseOffset;
} AllPassFilter;


class Reverb2 {
 public:
    Reverb2()
    {
        // spread of slow, mutually detuned LFO rates
        static const uint32_t lfoIncrement[10] = {
            VERB_LFO_INC(73),  VERB_LFO_INC(111), VERB_LFO_INC(91),  VERB_LFO_INC(127),
            VERB_LFO_INC(83),  VERB_LFO_INC(119), VERB_LFO_INC(67),  VERB_LFO_INC(103),
            VERB_LFO_INC(97),  VERB_LFO_INC(131)
        };
        for(int i=0;i<10;i++)
        {
            allpass[i].c = f32_to_q15(0.7f);
            allpass[i].phaseOffset = lfoIncrement[i];
            allpass[i].phase = 0;
        }
        // laid out in the order the tank runs them, so the shared buffer is walked
        // in sequence
        int16_t *next = buf;
        InitLine(&allpass[0], next, VERB_AP1);
        InitLine(&allpass[1], next, VERB_AP2);
        InitLine(&allpass[2], next, VERB_AP3);
        InitLine(&allpass[3], next, VERB_AP4);

        InitLine(&allpass[4], next, VERB_AP5);
        InitLine(&allpass[5], next, VERB_AP6);
        InitLine(&allpass[6], next, VERB_D1);

        InitLine(&allpass[8], next, VERB_AP7);
        InitLine(&allpass[9], next, VERB_AP8);
        InitLine(&allpass[7], next, VERB_D2);

        SetFeedback(VERB_DEFAULT_FEEDBACK);
        SetDamping(VERB_DEFAULT_DAMPING);
        SetModulation(VERB_DEFAULT_MODULATION);
    }
    // 0 = no tail, 255 = whichever feedback cap is selected (very long decay)
    void SetFeedback(uint8_t feedback)
    {
        feedbackKnob = feedback;
        UpdateFeedback();
    }
    // 0 = no damping (the feedback loop stays bright), 255 = heavily lowpassed (dark)
    void SetDamping(uint8_t damping)
    {
        dampAmount = (q15_t)(Q15_MAX - ((((uint32_t)damping)*(Q15_MAX-VERB_MIN_DAMP))>>8));
        dampInv = sub_q15(Q15_MAX, dampAmount);
    }
    // detunes the two tank allpasses to keep the tail from settling into a fixed
    // set of ringing modes. 0 uses the plain fixed-length allpasses instead
    void SetModulation(uint8_t depth)
    {
        modDepth = (int32_t)((((uint32_t)depth)*VERB_MAX_MOD_DEPTH)>>8);
    }
    // picks which of the two feedback caps the top of the knob reaches
    void SetLowFeedbackCap(bool on)
    {
        lowCap = on;
        UpdateFeedback();
    }
    int16_t last_l, last_r;
    inline void process(int16_t in, int16_t& l, int16_t& r)
    {
        if((++count)%2==0)
        {
            l = last_l;
            r = last_r;
            return;
        }
        Tank(in, l, r);
    }
 private:
    // the knob and the cap both feed the same value, so they share this
    void UpdateFeedback()
    {
        uint32_t cap = lowCap ? VERB_FEEDBACK_CAP_LOW : VERB_FEEDBACK_CAP_HIGH;
        feedbackAmount = (q15_t)((((uint32_t)feedbackKnob)*cap)>>8);
    }
    // pinned to RAM: too big to inline into Render, so without this it lands in
    // flash and runs through the XIP cache, which would make the reverb the only
    // part of the audio path whose timing depends on what else is running
    inline void __not_in_flash_func(Tank)(int16_t in, int16_t& l, int16_t& r)
    {
        // rounded, not a bare shift - truncating here injects a DC offset into the
        // tank (measured -12 LSB at the output, vs -4.6 rounded)
        in = (int16_t)((in + (1<<(VERB_HEADROOM_SHIFT-1))) >> VERB_HEADROOM_SHIFT);
        for(int i=0;i<4;i++)
        {
            in = ProcessAllPass(in, &allpass[i]);
        }
        // low pass filter the feedback. the gains sum to Q15_MAX, so this is a
        // contraction and can't leave q15
        feedback = (q15_t)(MultCoeff(feedback, dampAmount) + MultCoeff(damp[0], dampInv));
        damp[0] = feedback;

        in = ProcessTankAllPass(add_q15(in, MultCoeff(feedbackAmount, feedback)), &allpass[4]);
        in = ProcessAllPass(in, &allpass[5]);
        in = DelayProcess(in, &allpass[6]);
        in = ProcessTankAllPass(in, &allpass[8]);
        in = ProcessAllPass(in, &allpass[9]);
        feedback = DelayProcess(in, &allpass[7]);

        int16_t dt1 = DelayTap(310, &allpass[6]);
        int16_t dt2 = DelayTap(611, &allpass[6]);
        int16_t dt3 = DelayTap(937, &allpass[7]);
        int16_t dt4 = DelayTap(1201, &allpass[7]);

        // the four taps can sum to about twice full scale, so accumulate wide,
        // undo the headroom shift and clamp once. clamping each partial sum instead
        // loses the tap that would have pulled it back under
        int32_t accL = Mix(0, dt1, position[0]);
        int32_t accR = Mix(0, dt1, 0xffff-position[0]);

        accL += Mix(0, dt2, position[1]);
        accR += Mix(0, dt2, 0xffff-position[1]);

        accL += Mix(0, dt3, position[2]);
        accR += Mix(0, dt3, 0xffff-position[2]);

        accL += Mix(0, dt4, position[3]);
        accR += Mix(0, dt4, 0xffff-position[3]);

        last_l = l = (q15_t)__SSAT(accL<<VERB_HEADROOM_SHIFT, 16);
        last_r = r = (q15_t)__SSAT(accR<<VERB_HEADROOM_SHIFT, 16);
    }
    void InitLine(AllPassFilter *d, int16_t *&next, int length)
    {
        d->buf = next;
        d->end = next+length;
        d->write = next;
        d->length = length;
        next += length;
    }
    // need to do a multiply rounded so we don't get a bias due to bitshifting.
    // no saturation: `c` is always a coefficient, and |c| <= Q15_MAX bounds the
    // product to q15 on its own. M0+ has no SSAT instruction so that check is two
    // compares and two branches every time it can't fire
    static inline q15_t MultCoeff(q15_t a, q15_t c)
    {
        return (q15_t)((((q31_t)a*(q31_t)c) + 0x4000) >> 15);
    }
    // the write pointer is the only thing that moves, and it only ever runs one
    // sample past the end, so a compare beats the modulo the RP2040 has to call a
    // library routine for
    static inline void Advance(AllPassFilter *d)
    {
        if(++d->write >= d->end)
            d->write = d->buf;
    }
    // reads `tap` samples forward of the write pointer, which is the sample
    // (length-tap) old. tap has to be inside the line for the single wrap to hold
    static inline int16_t *TapPointer(AllPassFilter *d, int tap)
    {
        int16_t *p = d->write+tap;
        if(p >= d->end)
            p -= d->length;
        return p;
    }
    inline int16_t ProcessAllPass(int16_t in, AllPassFilter *ap)
    {
        int16_t apdelayed = *ap->write;
        int16_t inSum = *ap->write = add_q15(MultCoeff(-ap->c, apdelayed), in);
        Advance(ap);
        return add_q15(MultCoeff(inSum, ap->c), apdelayed);
    }
    inline int16_t ProcessTankAllPass(int16_t in, AllPassFilter *ap)
    {
        if(modDepth == 0)
            return ProcessAllPass(in, ap);
        return ProcessAllPassMod(in, ap);
    }
    // an allpass whose read tap slides around under an LFO while the write stays put.
    // the tap is centred on modDepth and swings +/- modDepth so it can never go
    // negative - the old wobble code let it, which indexed behind the buffer
    inline int16_t ProcessAllPassMod(int16_t in, AllPassFilter *ap)
    {
        int16_t lfo = Interpolate824(wav_sine, ap->phase);
        ap->phase += ap->phaseOffset;
        // 16.16 sample offset, so 0 <= (offset>>16) <= 2*modDepth
        int32_t offset = (modDepth<<16) + (modDepth*(int32_t)lfo)*2;
        int16_t *a = TapPointer(ap, offset>>16);
        int16_t *b = a+1;
        if(b >= ap->end)
            b -= ap->length;
        // the crossfade weight is the fraction of the offset - the old code passed
        // the raw LFO here, which is a different quantity entirely
        int16_t apdelayed = Mix(*a, *b, offset&0xffff);
        int16_t inSum = *ap->write = add_q15(MultCoeff(-ap->c, apdelayed), in);
        Advance(ap);
        return add_q15(MultCoeff(inSum, ap->c), apdelayed);
    }
    inline int16_t DelayProcess(int16_t in, AllPassFilter *d)
    {
        int16_t delayed = *d->write;
        *d->write = in;
        Advance(d);
        return delayed;
    }
    inline int16_t DelayTap(int tap, AllPassFilter *d)
    {
        return *TapPointer(d, tap);
    }

    q15_t damp[2] = {0};
    q15_t dampAmount, dampInv, feedbackAmount;
    int32_t modDepth = 0;

    AllPassFilter allpass[10];
    int16_t buf[VERB_LENGTH] = {0};
    int16_t feedback = 0;
    uint8_t count = 0;

    // where each output tap sits in the stereo field, as the 0..0xffff balance
    // Mix() takes. these used to be assigned q15 pan positions, and the negative
    // ones wrapped to just past half, which collapsed the whole reverb to mono
    uint16_t position[4] = {0x2000, 0xE000, 0xC000, 0x4000};
    uint8_t feedbackKnob = 0;
    bool lowCap = false;
};
