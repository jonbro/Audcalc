#ifndef sampler_h_
#define sampler_h_

#include "Matrix.h"
#include <Encoder.h>
//#include "Screen.h"

// implementation for the ui sampler
class Sampler
{
  public:
    Sampler();
    void update();
    void nextStep();
    
  private:
    void updateIO();
    Matrix m;
    //Screen screen;
    static uint32_t nextTrigger;
    bool writeEnabled;
    int currentChannel;
    int currentKey;
    uint32_t next;
    int step;
    
    // io handling stuff
    int ioCount;
    int lastIOUpdate;
    long enc1last  = 0;
    long enc2last  = 0;
    long enc1d  = 0;
    long enc2d  = 0;
    bool onPress[5][5];
    bool pressed[5][5];

    Encoder Enc1;
    Encoder Enc2;

};
#endif
