#include "audio/dsp.h"
#include "audio/resources.h"
#include "pico/stdlib.h"

using namespace braids;
#define DELAY_LENGTH 10000
class Delay2 {
 public:
    Delay2()
    {
        feedback = 0;
        feedbackAmount = 0x4fff;
    }
    inline void process(int16_t in, int16_t& l, int16_t& r)
    {
        in = add_q15(in, mult_q15(feedback, feedbackAmount));
        feedback = l = r = DelayProcess(in);
    }
 private:
    int16_t DelayProcess(int16_t in)
    {
        int16_t delayed = buf[count];
        buf[count] = in;
        if (++count > length-1)
            count = 0;
        return delayed;
    }
    int16_t DelayTap(int tap)
    {
        int offset = tap+count;
        while (offset < 0)
            offset += length;
        while(offset > length-1)
        {
            offset -= length;
        }
        return buf[offset];
    }

    q15_t  feedbackAmount;
    int16_t feedback;
    int length = DELAY_LENGTH;
    int count = 0;

    int16_t buf[DELAY_LENGTH] = {0};
};