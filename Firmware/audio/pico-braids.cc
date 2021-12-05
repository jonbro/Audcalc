#include <stdio.h>
#include <math.h>

#if PICO_ON_DEVICE

#include "hardware/clocks.h"
#include "hardware/structs/clocks.h"

#endif

#if USE_AUDIO_I2S
#include "pico/audio_i2s.h"
#elif USE_AUDIO_PWM
#include "pico/audio_pwm.h"
#elif USE_AUDIO_SPDIF
#include "pico/audio_spdif.h"
#endif

#include "macro_oscillator.h"
#include "dspinst.h"


#define SINE_WAVE_TABLE_LEN 2048
#define SAMPLES_PER_BUFFER 256

using namespace braids;

static int16_t sine_wave_table[SINE_WAVE_TABLE_LEN];
static uint8_t sync_buffer[SAMPLES_PER_BUFFER];

struct audio_buffer_pool *init_audio() {

    static audio_format_t audio_format = {
#if USE_AUDIO_SPDIF
            .sample_freq = 44100,
#else
            .sample_freq = 44100,
#endif
            .format = AUDIO_BUFFER_FORMAT_PCM_S16,
            .channel_count = 1
    };

    static struct audio_buffer_format producer_format = {
            .format = &audio_format,
            .sample_stride = 2
    };

    struct audio_buffer_pool *producer_pool = audio_new_producer_pool(&producer_format, 3,
                                                                      SAMPLES_PER_BUFFER); // todo correct size
    bool __unused ok;
    const struct audio_format *output_format;

#if USE_AUDIO_I2S
    struct audio_i2s_config config = {
            .data_pin = PICO_AUDIO_I2S_DATA_PIN,
            .clock_pin_base = PICO_AUDIO_I2S_CLOCK_PIN_BASE,
            .dma_channel = 0,
            .pio_sm = 0,
    };

    output_format = audio_i2s_setup(&audio_format, &config);
    if (!output_format) {
        panic("PicoAudio: Unable to open audio device.\n");
    }

    ok = audio_i2s_connect(producer_pool);
    assert(ok);
    audio_i2s_set_enabled(true);
#elif USE_AUDIO_PWM
    output_format = audio_pwm_setup(&audio_format, -1, &default_mono_channel_config);
    if (!output_format) {
        panic("PicoAudio: Unable to open audio device.\n");
    }
    ok = audio_pwm_default_connect(producer_pool, false);
    assert(ok);
    audio_pwm_set_enabled(true);
#elif USE_AUDIO_SPDIF
    output_format = audio_spdif_setup(&audio_format, &audio_spdif_default_config);
    if (!output_format) {
        panic("PicoAudio: Unable to open audio device.\n");
    }
    //ok = audio_spdif_connect(producer_pool);
    ok = audio_spdif_connect(producer_pool);
    assert(ok);
    audio_spdif_set_enabled(true);
#endif
    return producer_pool;
}

MacroOscillator osc;
MacroOscillator osc2;
MacroOscillator osc3;
MacroOscillator osc4;
#define MULTI_UNITYGAIN 65536
static inline uint32_t signed_add_16_and_16(uint32_t a, uint32_t b)
{
    return a+b;
}
static void applyGainThenAdd(int16_t *data, const int16_t *in, int32_t mult)
{
	uint32_t *dst = (uint32_t *)data;
	const uint32_t *src = (uint32_t *)in;
	const uint32_t *end = (uint32_t *)(data + SAMPLES_PER_BUFFER);

	if (mult == MULTI_UNITYGAIN) {
		do {
			uint32_t tmp32 = *dst;
			*dst++ = signed_add_16_and_16(tmp32, *src++);
			tmp32 = *dst;
			*dst++ = signed_add_16_and_16(tmp32, *src++);
		} while (dst < end);
	} else {
		do {
			uint32_t tmp32 = *src++; // read 2 samples from *data
			int32_t val1 = signed_multiply_32x16b(mult, tmp32);
			int32_t val2 = signed_multiply_32x16t(mult, tmp32);
			val1 = signed_saturate_rshift(val1, 16, 0);
			val2 = signed_saturate_rshift(val2, 16, 0);
			tmp32 = pack_16b_16b(val2, val1);
			uint32_t tmp32b = *dst;
			*dst++ = signed_add_16_and_16(tmp32, tmp32b);
		} while (dst < end);
	}
}


int main() {
#if PICO_ON_DEVICE
#if USE_AUDIO_PWM
    set_sys_clock_48mhz();
#endif
#endif


    stdio_init_all();
    int counter = 0;
    struct audio_buffer_pool *ap = init_audio();
    osc.Init();
    osc.set_shape(MACRO_OSC_SHAPE_WAVETABLES);
    osc2.Init();
    osc2.set_shape(MACRO_OSC_SHAPE_SQUARE_SUB);
    osc3.Init();
    osc3.set_shape(MACRO_OSC_SHAPE_VOWEL_FOF);
    osc4.Init();
    osc4.set_shape(MACRO_OSC_SHAPE_CSAW);
    uint vol = 128;
    uint32_t step = 0x200000;

    while (true) {
        #if USE_AUDIO_PWM
        enum audio_correction_mode m = audio_pwm_get_correction_mode();
#endif
        int c = getchar_timeout_us(0);
        if (c >= 0) {
            if (c == '-' && vol) vol -= 4;
            if ((c == '=' || c == '+') && vol < 255) vol += 4;
            if (c == '[' && step > 0x10000) step -= 0x10000;
            if (c == ']' && step < (SINE_WAVE_TABLE_LEN / 16) * 0x20000) step += 0x10000;
            if (c == 'q') break;
#if USE_AUDIO_PWM
            if (c == 'c') {
                bool done = false;
                while (!done) {
                    if (m == none) m = fixed_dither;
                    else if (m == fixed_dither) m = dither;
                    else if (m == dither) m = noise_shaped_dither;
                    else if (m == noise_shaped_dither) m = none;
                    done = audio_pwm_set_correction_mode(m);
                }
            }
            printf("vol = %d, step = %d mode = %d      \r", vol, step >>16, m);
#else
            printf("vol = %d, step = %d      \r", vol, step >> 16);
#endif
        }

        counter = counter+10;
        struct audio_buffer *buffer = take_audio_buffer(ap, true);  
        //struct audio_buffer *mixBuffer = take_audio_buffer(ap, true);  
        memset(sync_buffer, 0, SAMPLES_PER_BUFFER);
        int i=counter;
        uint16_t tri = (i * 3);
        uint16_t tri2 = (i * 11);
        uint16_t ramp = i * 150;
        tri = tri > 32767 ? 65535 - tri : tri;
        tri2 = tri2 > 32767 ? 65535 - tri2 : tri2;
        osc.set_parameters(tri, 0);
        osc.set_pitch((48 << 7));
        osc.Render(sync_buffer, (int16_t *) buffer->buffer->bytes, buffer->max_sample_count);
        int16_t *samples = (int16_t *) buffer->buffer->bytes;
        for (uint i = 0; i < buffer->max_sample_count; i++) {
            // does a mix towards zero
            samples[i] = stmlib::Mix(samples[i], 0, 65535*0.8);
        }
        
        osc2.set_parameters(tri, 0);
        osc2.set_pitch((13 << 7));
        osc2.Render(sync_buffer, (int16_t *) sine_wave_table, buffer->max_sample_count);
        // lets just use the braids dsp for mix
        int16_t *samples2 = (int16_t *) sine_wave_table;
        for (uint i = 0; i < buffer->max_sample_count; i++) {
            samples[i] = stmlib::Mix(samples[i], samples2[i], 32767*0.2f);
        }
        memset(sine_wave_table, 0, SAMPLES_PER_BUFFER);
        osc3.set_parameters(tri, 0);
        osc3.set_pitch((43 << 7));
        osc3.Render(sync_buffer, (int16_t *) sine_wave_table, buffer->max_sample_count);
        for (uint i = 0; i < buffer->max_sample_count; i++) {
            samples[i] = samples[i]>>1;
            samples[i] = stmlib::Mix(samples[i], samples2[i], 32767*0.3f);
        }
        osc4.set_parameters(tri, 0);
        osc4.set_pitch((39 << 7));
        osc4.Render(sync_buffer, (int16_t *) sine_wave_table, buffer->max_sample_count);
        for (uint i = 0; i < buffer->max_sample_count; i++) {
            samples[i] = samples[i]>>1;
            samples[i] = stmlib::Mix(samples[i], samples2[i], 32767*0.3f);
        }


        // mix osc1 and osc2

        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(ap, buffer);
    }
    return 0;
}