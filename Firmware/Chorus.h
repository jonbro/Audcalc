
#include "audio/dsp.h"
#include "audio/resources.h"
#include "pico/stdlib.h"

using namespace braids;
#define CHORUS_LENGTH 3000
class Chorus {
 public:
    inline void process(int16_t in, int16_t& l, int16_t& r)
    {
        // in = Mix(in, feedback, 0x1fff);
        DelayProcess(in);
        int32_t mainL = 0;
        int32_t mainR = 0;

        for(int i=0;i<4;i++)
        {
            // there is a bug in this chorus somewhere that is making everything run too slow - look at the reverb code for doing "chorus" style effects for a potential fix
            phase[i] += phaseOffset[i];
            int16_t lfo = Interpolate824(wav_sine, phase[i]);
            int32_t offset = ((int32_t)CHORUS_LENGTH*(int32_t)(lfo>>1));
            int16_t a = DelayTap(offset>>16);
            int16_t b = DelayTap((offset>>16)+1);
            int16_t o = Mix(a, b, offset&0xffff);
            mainL += Mix(0, o, position[i]);
            mainR += Mix(0, o, 0xffff-position[i]);
        }
        l = mainL >> 2;
        r = mainR >> 2;
        // l = 0;//Mix(0, o, position[i]);
        // r = 0;//Mix(0, o, 0xffff-position[i]);
        //feedback = Mix(l, r, 0x7fff);
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
    inline int16_t DelayTap(int16_t tap)
    {
        int offset = tap+count;
        while (offset < 0)
            offset += length;
        return buf[offset%(length-1)];
    }
    int16_t buf[CHORUS_LENGTH];
    int count = 0;
    int length = CHORUS_LENGTH;
    int16_t feedback = 0;
    uint32_t phase[4] = {0, 0, 0, 0};
    uint16_t phaseOffset[4] = {803, 447, 1243, 1089};
    uint16_t position[4] = {0x1000, 0x4000, 0x9eee, 0xdddd};
};