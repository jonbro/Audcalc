#include "AudioSamplerSimple.h"
#include "utility/dspinst.h"

void AudioSamplerSimple::setStart(const unsigned int *data, float _start)
{
  uint32_t format;
  format = *data;
  length = format & 0xFFFFFF;

  start = (float)length * _start*0.25f ;
}
void AudioSamplerSimple::setLength(const unsigned int *data, float _length)
{
  uint32_t format;
  format = *data;
  length = format & 0xFFFFFF;

  end = (float)length * (1.0f-_length);
}
void AudioSamplerSimple::play(const unsigned int *data)
{
  uint32_t format;

  playing = 0;
  prior = 0;
  format = *data++;
  next = data+start;
  beginning = data;
  length = format & 0xFFFFFF;
  length = min(length-start, max(0, length-end));
  
  playing = format >> 24;
  Serial.println(playing);
}

extern "C" {
extern const int16_t ulaw_decode_table[256];
};

void AudioSamplerSimple::update(void)
{
  audio_block_t *block;
  const unsigned int *in;
  int16_t *out;
  uint32_t tmp32, consumed;
  int16_t s0, s1, s2, s3, s4;
  int i;

  if (!playing) return;
  block = allocate();
  if (block == NULL) return;

  //Serial.write('.');

  out = block->data;
  in = next;
  s0 = prior;

  switch (playing) {
    case 0x01: // u-law encoded, 44100 Hz
    for (i=0; i < AUDIO_BLOCK_SAMPLES; i += 4) {
      tmp32 = *in++;
      *out++ = ulaw_decode_table[(tmp32 >> 0) & 255];
      *out++ = ulaw_decode_table[(tmp32 >> 8) & 255];
      *out++ = ulaw_decode_table[(tmp32 >> 16) & 255];
      *out++ = ulaw_decode_table[(tmp32 >> 24) & 255];
    }
    consumed = AUDIO_BLOCK_SAMPLES;
    break;

    default:
    release(block);
    playing = 0;
    return;
  }
  prior = s0;
  next = in;
  if (length > consumed) {
    length -= consumed;
  } else {
    playing = 0;
  }
  transmit(block);
  release(block);
}

#define B2M_88200 (uint32_t)((double)4294967296000.0 / AUDIO_SAMPLE_RATE_EXACT / 2.0)
#define B2M_44100 (uint32_t)((double)4294967296000.0 / AUDIO_SAMPLE_RATE_EXACT) // 97352592
#define B2M_22050 (uint32_t)((double)4294967296000.0 / AUDIO_SAMPLE_RATE_EXACT * 2.0)
#define B2M_11025 (uint32_t)((double)4294967296000.0 / AUDIO_SAMPLE_RATE_EXACT * 4.0)
