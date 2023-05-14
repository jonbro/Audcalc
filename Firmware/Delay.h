#include "audio/dsp.h"
#include "audio/resources.h"
#include "pico/stdlib.h"

using namespace braids;
#define DELAY_LENGTH 20000
class Delay {
 public:
    Delay()
    {
        feedback = 0;
        feedbackAmount = 0x4fff;
        tapPosition = DELAY_LENGTH-1;
    }
    inline void process(int16_t in, int16_t& l, int16_t& r)
    {
        in = add_q15(in, mult_q15(feedback, feedbackAmount));
        feedback = l = r = DelayTap(tapPosition);
        DelayProcess(in);
    }
    void SetFeedback(uint8_t feedback)
    {
        feedbackAmount = feedback<<7;
    }
    void SetTime(uint8_t time)
    {
        // minimum 1/100 of a second, maximum 1.5 seconds?
        uint16_t min = 64;
        uint16_t max = DELAY_LENGTH-2; 
        tapPosition = (((uint32_t)(max-min)*(uint32_t)(0xff-time)) >> 8) + min;
    }
 private:
    inline int16_t DelayProcess(int16_t in)
    {
        int16_t delayed = buf[count];
        buf[count] = in;
        if (++count > length-1)
            count = 0;
        return delayed;
    }
    inline int16_t DelayTap(uint16_t tap)
    {
        int offset = tap+count;
        if(offset > length-1)
        {
            offset -= length;
        }
        return buf[offset];
    }

    q15_t  feedbackAmount;
    int16_t feedback;
    int length = DELAY_LENGTH;
    int count = 0;
    uint16_t tapPosition = 0;
    uint16_t lastWritePosition = 0;
    int16_t buf[DELAY_LENGTH] = {0};
};