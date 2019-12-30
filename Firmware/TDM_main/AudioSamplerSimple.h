#ifndef audio_sampler_simple_h_
#define audio_sampler_simple_h_

#include "Arduino.h"
#include "AudioStream.h"

class AudioSamplerSimple : public AudioStream
{
  public:
    AudioSamplerSimple() : AudioStream(0, NULL), playing(0), start(0) { }
    void play(const unsigned int *data);
    void setStart(const unsigned int *data, float _start);
    void setLength(const unsigned int *data, float _length);
    virtual void update(void);
  private:
    uint32_t start;
    const unsigned int *next;
    const unsigned int *beginning;
    uint32_t length;
    uint32_t end;
    int16_t prior;
    volatile uint8_t playing;
};
#endif
