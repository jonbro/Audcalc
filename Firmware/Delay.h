#include "audio/dsp.h"
#include "audio/resources.h"
#include "pico/stdlib.h"

using namespace braids;
#define DELAY_LENGTH 20000
class Delay2 {
 public:
    Delay2()
    {
        feedback = 0;
        feedbackAmount = 0x4fff;
        phaseOffset = 0x41893;
    }
    inline void process(int16_t in, int16_t& l, int16_t& r)
    {
        in = add_q15(in, mult_q15(feedback, feedbackAmount));
        feedback = l = r = DelayProcess(in);
    }
 private:
    int16_t DelayProcess(int16_t in)
    {
        uint64_t offsetFull = ((uint64_t)length*(uint64_t)(phase)) >> 16;
        int32_t offset = offsetFull >> 16;
        int16_t a = buf[offset];
        int16_t b = buf[(offset-1)<0?length+(offset-1):offset-1];
        int16_t delayed =  Mix(b, a, offsetFull&0xffff);

        int32_t writePos = offset-2;
        if(writePos < 0)
            writePos = length+writePos;
        // fill in the extra samples in case we are writing faster than normal speed 
        while((lastWritePosition+1)%length != writePos)
        {
            lastWritePosition = (lastWritePosition+1)%length;
            buf[lastWritePosition] = in;
        }
        phase += phaseOffset;
        return delayed;
    }

    q15_t  feedbackAmount;
    int16_t feedback;
    int length = DELAY_LENGTH;
    int count = 0;
    uint32_t lastWritePosition = 0;
    uint32_t phase = 0;
    uint32_t phaseOffset = 0x10624; // how far we step
    int16_t buf[DELAY_LENGTH] = {0};
};