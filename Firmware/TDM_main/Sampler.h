#ifndef sampler_h_
#define sampler_h_

#include "Matrix.h"
#include <Encoder.h>
#include "AudioSamplerSimple.h"
#include "AudioSampleMug.h"

enum SamplerState
{
  Sound,
  Pattern,
  Normal
};
// implementation for the ui sampler
class Sampler
{
  public:
    Sampler(AudioSamplerSimple* samplerCore);
    void update();
    void nextStep();
    
  private:
    void updateIO();
    Matrix m;
    static uint32_t nextTrigger;
    SamplerState state;
    bool writeEnabled;
    int currentChannel;
    int currentKey;
    int step;
    AudioSamplerSimple* samplerCore;
    int pattern[16];
    uint32_t next;
    
    // io handling stuff
    int ioCount;
    int lastIOUpdate;
    long enc1last  = 0;
    long enc2last  = 0;
    long enc1d  = 0;
    long enc2d  = 0;
    bool onPress[5][5];
    bool pressed[5][5];
    float start[16];
    float length[16];
    int lastPressed;
    Encoder Enc1;
    Encoder Enc2;

};
#endif
