/*
            .data_pin = I2S_DATA_PIN,
            .clock_pin_base = I2S_BCLK_PIN,
*/

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include <math.h>

#include "hardware/clocks.h"
#include "hardware/structs/clocks.h"
#include "input_output_copy_i2s.pio.h"
#include "ws2812.pio.h"

#define POCKETMOD_IMPLEMENTATION
#include "pocketmod.h"
//#include "sundown.h"
#include "bananasplit.h"

#if USE_AUDIO_I2S
#include "pico/audio_i2s.h"
#elif USE_AUDIO_PWM
#include "pico/audio_pwm.h"
#elif USE_AUDIO_SPDIF
#include "pico/audio_spdif.h"
#endif

// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define I2C_PORT i2c1
#define I2C_SDA 26
#define I2C_SCL 27
#define TLV_RESET_PIN 21

#define TLV_I2C_ADDR            0x18
#define TLV_REG_PAGESELECT	    0
#define TLV_REG_LDOCONTROL	    2
#define TLV_REG_RESET   	    1
#define TLV_REG_CLK_MULTIPLEX   	0x04

#define SAMPLES_PER_BUFFER 512
#define I2S_DATA_PIN 19
#define I2S_BCLK_PIN 17

#define BLINK_PIN_LED 25

#define SINE_WAVE_TABLE_LEN 2048

static int16_t sine_wave_table[SINE_WAVE_TABLE_LEN];


struct audio_buffer_pool *init_audio() {

    static audio_format_t audio_format = {
            .sample_freq = 44100,
            .format = AUDIO_BUFFER_FORMAT_PCM_S16,
            .channel_count = 2
    };

    static struct audio_buffer_format producer_format = {
            .format = &audio_format,
            .sample_stride = 4
    };

    struct audio_buffer_pool *producer_pool = audio_new_producer_pool(&producer_format, 3,
                                                                      SAMPLES_PER_BUFFER*2); // todo correct size
    bool __unused ok;
    const struct audio_format *output_format;

    struct audio_i2s_config config = {
            /*
            .data_pin = PICO_AUDIO_I2S_DATA_PIN,
            .clock_pin_base = PICO_AUDIO_I2S_CLOCK_PIN_BASE,
            */
            .data_pin = I2S_DATA_PIN,
            .clock_pin_base = I2S_BCLK_PIN,
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
    return producer_pool;
}

uint8_t currentPage = -1;
bool writeRegister(uint8_t reg, uint8_t val)
{
    uint8_t data[2];
    data[0] = reg;
    data[1] = val;
	int attempt=0;
	while (1) {
		attempt++;
        
        int ret = i2c_write_blocking(I2C_PORT, TLV_I2C_ADDR, data, 2, false);
		if (ret == 2) {
            printf("TLV320 write ok, %d tries\n", attempt);
            return true;
        }
        if (attempt >= 12) {
            printf("TLV320 write failed, %d tries\n", attempt);
            return false;
        }
        sleep_us(80);
    }
}
bool readRegister(uint8_t reg)
{
	int attempt=0;
	while (1) {
		attempt++;
        
        int ret = i2c_write_blocking(I2C_PORT, TLV_I2C_ADDR, &reg, 1, true);
        sleep_ms(5);
		if (ret == 1) {
            int ret;
            uint8_t rxdata;
            attempt = 0;
            ret = i2c_read_blocking(I2C_PORT, TLV_I2C_ADDR, &rxdata, 1, false);
            if(ret == 1)
            {
                printf("reg: %x val: %x\n", reg, rxdata);
                return true;
            }
            else
            {
                printf("failed read %i\n", ret);
                return false;
            }
        }
        if (attempt >= 12) {
            printf("TLV320 read failed, %d tries\n", attempt);
            return false;
        }
        sleep_us(80);
    }
}
bool write(uint8_t page, uint8_t reg, uint8_t val)
{
    if(currentPage != page)
    {
        if(!writeRegister(0, page))
        {
            printf("failed to go to page %i\n", page);
            return false;
        }
        currentPage = page;
    }
    return writeRegister(reg, val);
}
static float clip(float value)
{
    value = value < -1.0f ? -1.0f : value;
    value = value > +1.0f ? +1.0f : value;
    return value;
}

int dma_chan_input;
int dma_chan_output;
uint sm;
uint32_t capture_buf[256];
uint32_t silence_buf[256];
bool flipFlop = false;
int dmacount = 0;
int inputBufCount = 0;
int inputBufOffset = 0;
void dma_input_handler() {
    dma_hw->ints0 = 1u << dma_chan_input;
    uint32_t* bufWrite = capture_buf;
    uint32_t* readAddr = capture_buf;
    if(flipFlop)
    {
        inputBufOffset = 1;
        flipFlop = false;
        readAddr+=128;
    }
    else
    {
        inputBufOffset = 0;
        bufWrite+=128;
        flipFlop = true;
    }
    inputBufCount++;
    dma_channel_set_write_addr(dma_chan_input, bufWrite, true);
}
bool hi = false;
uint32_t output_buf[256];
uint16_t out_count = 0;
int flipflopout = 0;
uint16_t halffourfourty = 654;
void dma_output_handler() {
    dma_hw->ints1 = 1u << dma_chan_output;
    /*
    uint32_t* output = output_buf+(flipflopout)*128;
    dma_channel_set_read_addr(dma_chan_output, output, true);
    flipflopout = (flipflopout+1)%2;
    output = output_buf+(flipflopout)*128;
    // fill the other buffer
    for(int i=0;i<128;i++)
    {
        uint16_t* chan = (uint16_t*)(output+i);
        chan[0] = out_count;
        chan[1] = out_count;
        out_count+=halffourfourty;
    }
    */
    
    // this is where we would check to see if there is an input buffer ready to point at
    if(inputBufCount>0)
    {
        uint32_t *output = capture_buf+(inputBufOffset==0?0:128);
        for(int i=0;i<128;i++)
        {
            uint16_t* chan = (uint16_t*)(output+i);
            uint16_t lowerVolumeOut = (out_count>>8)+0x7fff;
            // chan[0] += lowerVolumeOut;
            // chan[1] += lowerVolumeOut;
            out_count+=halffourfourty;
        }
        if(inputBufOffset==0)
        {
            dma_channel_set_read_addr(dma_chan_output, capture_buf, true);
        }
        else
        {
            dma_channel_set_read_addr(dma_chan_output, capture_buf+128, true);
        }
        inputBufCount--;
    }
    else
    {
        dma_channel_set_read_addr(dma_chan_output, silence_buf, true);
    }
    /**/
}

static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return
            ((uint32_t) (r) << 8) |
            ((uint32_t) (g) << 16) |
            (uint32_t) (b);
}

void pattern_snakes(uint len, uint t) {
    for (uint i = 0; i < len; ++i) {
        uint x = (i + (t >> 1)) % 64;
        if (x < 10)
            put_pixel(urgb_u32(0xff, 0, 0));
        else if (x >= 15 && x < 25)
            put_pixel(urgb_u32(0, 0xff, 0));
        else if (x >= 30 && x < 40)
            put_pixel(urgb_u32(0, 0, 0xff));
        else
            put_pixel(0);
    }
}

void pattern_random(uint len, uint t) {
    if (t % 8)
        return;
    for (int i = 0; i < len; ++i)
        put_pixel(rand());
}

void pattern_sparkle(uint len, uint t) {
    if (t % 8)
        return;
    for (int i = 0; i < len; ++i)
        put_pixel(rand() % 16 ? 0 : 0xffffffff);
}

void pattern_greys(uint len, uint t) {
    int max = 100; // let's not draw too much current!
    t %= max;
    for (int i = 0; i < len; ++i) {
        put_pixel(t * 0x10101);
        if (++t >= max) t = 0;
    }
}

typedef void (*pattern)(uint len, uint t);
const struct {
    pattern pat;
    const char *name;
} pattern_table[] = {
        {pattern_snakes,  "Snakes!"},
        {pattern_random,  "Random data"},
        {pattern_sparkle, "Sparkles"},
        {pattern_greys,   "Greys"},
};
int main()
{
    set_sys_clock_khz(200000, true); 
    stdio_init_all();
    // fill the silence buffer so we get something out
    for(int i=0;i<128;i++)
    {
        uint16_t* chan = (uint16_t*)(silence_buf+i);
        chan[0] = 0;
        chan[1] = 0;

        //chan[0] = i<<9;
        //chan[1] = i<<9;
    }

    // I2C Initialisation. Using it at 100Khz.
    i2c_init(I2C_PORT, 100*1000);

    //struct audio_buffer_pool *ap = init_audio();
    
    sleep_ms(2000);
    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    gpio_init(TLV_RESET_PIN);
    gpio_set_dir(TLV_RESET_PIN, GPIO_OUT);

    // hardware reset
    gpio_put(TLV_RESET_PIN, 1);
    sleep_ms(250);
    gpio_put(TLV_RESET_PIN, 0);
    sleep_ms(250);
    gpio_put(TLV_RESET_PIN, 1);
    sleep_ms(250);


    /**/
    PIO pio = pio1;
    uint offset = pio_add_program(pio, &input_output_copy_i2s_program);
    printf("loaded program at offset: %i\n", offset);
    sm = pio_claim_unused_sm(pio, true);
    printf("claimed sm: %i\n", sm); //I2S_DATA_PIN
    int sample_freq = 44100;
    printf("setting pio freq %d\n", (int) sample_freq);
    uint32_t system_clock_frequency = clock_get_hz(clk_sys);
    assert(system_clock_frequency < 0x40000000);
    // we want this to run at 3*32*sample_freq
    // I really don't understand how this works! because when there is one less operation, the number here is 4, rather than 3...
    // I did the math by hand, and it seems to line up tho :0
    // had to add a wait instruction in the pio - I don't love it, but whatcha gonna do... fix it correctly !?!??!
    uint32_t divider = system_clock_frequency * 2 / sample_freq; // avoid arithmetic overflow
    // uint32_t divider = system_clock_frequency/(sample_freq*3*32);
    printf("System clock at %u, I2S clock divider 0x%x/256\n", (uint) system_clock_frequency, (uint)divider);
    assert(divider < 0x1000000);
    input_output_copy_i2s_program_init(pio, sm, offset, I2S_DATA_PIN, I2S_DATA_PIN+1, I2S_BCLK_PIN);
    pio_sm_set_enabled(pio, sm, true);
    pio_sm_set_clkdiv_int_frac(pio, sm, divider >> 8u, divider & 0xffu);

    // just use a dma to pull the data out. Whee!
    dma_chan_input = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(dma_chan_input);
    channel_config_set_read_increment(&c,false);
    // increment on write
    channel_config_set_write_increment(&c,true);
    channel_config_set_dreq(&c,pio_get_dreq(pio,sm,false));
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);

    dma_channel_configure(
        dma_chan_input,
        &c,
        capture_buf, // Destination pointer
        &pio->rxf[sm], // Source pointer
        128, // Number of transfers
        true// Start immediately
    );
    // Tell the DMA to raise IRQ line 0 when the channel finishes a block
    dma_channel_set_irq0_enabled(dma_chan_input, true);

    // Configure the processor to run dma_handler() when DMA IRQ 0 is asserted
    irq_set_exclusive_handler(DMA_IRQ_0, dma_input_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    // need another dma channel for output
    dma_chan_output = dma_claim_unused_channel(true);
    dma_channel_config cc = dma_channel_get_default_config(dma_chan_output);
    // increment on read, but not on write
    channel_config_set_read_increment(&cc,true);
    channel_config_set_write_increment(&cc,false);
    channel_config_set_dreq(&cc,pio_get_dreq(pio,sm,true));
    channel_config_set_transfer_data_size(&cc, DMA_SIZE_32);
    dma_channel_configure(
        dma_chan_output,
        &cc,
        &pio->txf[sm], // Destination pointer
        silence_buf, // Source pointer
        128, // Number of transfers
        true// Start immediately
    );
    dma_channel_set_irq1_enabled(dma_chan_output, true);
    // Configure the processor to run dma_handler() when DMA IRQ 0 is asserted
    irq_set_exclusive_handler(DMA_IRQ_1, dma_output_handler);
    irq_set_enabled(DMA_IRQ_1, true);
    // write initial values into the output, so we don't block
    //pio_sm_put(pio, sm, 0);
    


    // reset
    write(0, 0x01, 0x01);

    // configure pll dividers
    // our bitclock runs at 32x(sample_freq)
    // so with 44100 we use the following values
    // pllp 1 pllr 3 pllj 20 plld 0 MADC 3 NADC 5 AOSR 128 MDAC 3 NDAC 5 DOSR 128
    // 44100*32*3*20.0

    // use bitclock for pll input, pll for clock input
    write(0, 0x04, 0x07);

    // pll p & r & powerup
    write(0, 0x05, (0x01<<7)|(0x01<<4)|0x03);
    // pll j
    write(0, 0x06, 0x14);


    // ndac &p powerup
    write(0, 0x0b, 0x80|0x05);
    // mdac & powerup
    write(0, 0x0c, 0x80|0x03);

    // adc clocks should be driven by dac clocks, so no power necessary, but need to be set correctly
    // but you still need to set them correctly
    write(0, 0x12, 0x05);
    write(0, 0x13, 0x03);

    // dosr 64
    write(0, 0x0d, 0x00);
    write(0, 0x0e, 0x40);

    // adc osr 64
    write(0, 0x14, 0x40);
    // Select ADC PRB_R7
    write(0, 0x3d, 0x07);

    // word length
    // processing block? default doesn't have the beep generator :(
    // printf("setup clocks\n");
    // sleep_ms(1000);
    // printf("one second to power\n");
    // sleep_ms(1000);
    // printf("power\n");
    // power enable
    // disable internal avdd prior to powering up internal LDO
    write(1, 0x01, 0x08);
    // enable analog power blocks

    // common mode voltage
    //
    // set ref to slow powerup
    write(1, 0x7b, 0x00);
    // HP softstepping settingsfor optimal pop performance at power up
    // Rpopusedis 6k withN = 6 and softstep= 20usec.Thisshouldworkwith47uFcoupling
    // capacitor.Can try N=5, 6 or 7 time constants as well.Trade-offdelay vs “pop” sound.
    write(1, 0x14, 0x25);
    // enable LDO
    write(1, 0x02, 0x01);

    // setup headphones to be powered by ldo
    // and set LDO input to 1.8-3.3
    write(1, 0x0a, 0x0B);

    // Set MicPGA startup delay to 3.1ms
    write(1, 0x47, 0x32);
    
    /**/
    // disable micbias
    write(1, 0x33, 0);
    // route IN2L to MicPGAL positive with 20k input impedance
    write(1, 0x34, 0x20);
    // route Commonmode to MicPGAL neg with 20k input impedance    
    write(1, 0x36, 0x80);

    // route IN2R to MicPGAR positive with 20k input impedance
    write(1, 0x37, 0x20);
    // route Commonmode to MicPGAR neg with 20k input impedance    
    write(1, 0x39, 0x80);
    // might need 0x3b / 0x3c gain control for MICPGA - leaving it at 0 gain for now
    write(1, 0x3b, 0x0c);
    write(1, 0x3c, 0x0c);
    
    /*
    write(1, 0x34, 0x80);
    write(1, 0x36, 0x80);
    write(1, 0x37, 0x80);
    write(1, 0x39, 0x80);
    write(1, 0x3b, 0x0c);
    write(1, 0x3c, 0x0c);
    */
    // analog bypass & dac routed to headphones
    
    // write(1, 0x0c, 0x0a);
    // write(1, 0x0d, 0x0a);

    // dac & mixer to headphones
    write(1, 0x0c, 0x0a);
    write(1, 0x0d, 0x0a);
    // route dac only to headphones
    write(1, 0x0c, 0x08);
    write(1, 0x0d, 0x08);

    write(1, 0x03, 0x00);
    write(1, 0x04, 0x00);
    // Set the ADC PTM Mode to PTM_R1
    write(1, 0x3d, 0xff);

    // Set the HPL gainto 0dBw 30
    // set gain to whatever the heck this is?
    write(1, 0x10, 0x00);
    write(1, 0x11, 0x00);
    
    // power up headphones & mixer amp
    write(1, 0x09, 0x33);

    // Powerup the Left and Right DAC Channels
    write(0, 0x3f, 0xd6);
    // Unmutethe DAC digital volume control 
    write(0, 0x40, 0x00);
    sleep_ms(200);
    printf("powering up adc, lets go\n");
    sleep_ms(200);
    printf("powering up adc, now\n");
    // power up ADC
    write(0, 0x51, 0xc0);
    write(0, 0x52, 0x00);


    // Waitfor 1 sec for softsteppingto takeeffect
    sleep_ms(1000);
    /*
    uint16_t counter = 0;

    uint32_t step = 0x50000;
    uint32_t pos = 0;
    uint32_t pos_max = 0x10000 * SINE_WAVE_TABLE_LEN;
    uint vol = 128;
    for (int i = 0; i < SINE_WAVE_TABLE_LEN; i++) { 
        sine_wave_table[i] = 32767 * cosf(i * 2 * (float) (M_PI / SINE_WAVE_TABLE_LEN));
    }
    pocketmod_context pm_c;
    pocketmod_init(&pm_c, bananasplit, 19035*16, 44100);

    float renderTarget[SAMPLES_PER_BUFFER*16];
    while(true)
    {
        
        struct audio_buffer *buffer = take_audio_buffer(ap, true);
        
        int16_t *samples = (int16_t *) buffer->buffer->bytes;
        // 4 bytes per channel * 2 channels

        pocketmod_render(&pm_c, renderTarget, buffer->max_sample_count*2*4);
        
        for (uint i = 0; i < buffer->max_sample_count*2; i++) {
            samples[i] = 0x7fff*clip(renderTarget[i]);
            samples[i] = 0x7fff;
        }
       // printf("sample value: %i\n", samples[0]);
        
        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(ap, buffer);
    }
    */

    gpio_init(BLINK_PIN_LED);
    gpio_set_dir(BLINK_PIN_LED, GPIO_OUT);
    int count = 0;
    // set all the pins to output
    uint8_t data[2];
    data[0] = 0x06;
    data[1] = 0;
    int ret = i2c_write_blocking(I2C_PORT, 0x20, data, 2, false);
    data[0] = 0x07;
    data[1] = 0;
    ret = i2c_write_blocking(I2C_PORT, 0x20, data, 2, false);

    PIO wspio = pio0;
    sm = pio_claim_unused_sm(wspio, true);

    uint wsoffset = pio_add_program(wspio, &ws2812_program);

    int t = 0;
    ws2812_program_init(wspio, sm, wsoffset, 4, 800000, true);
    while(true)
    {
        sleep_ms(2);


        int pat = rand() % count_of(pattern_table);
        int dir = (rand() >> 30) & 1 ? 1 : -1;
        puts(pattern_table[pat].name);
        puts(dir == 1 ? "(forward)" : "(backward)");
        for (int i = 0; i < 1000; ++i) {
            pattern_table[pat].pat(150, t);
            sleep_ms(10);
            t += dir;
        }

    }
    /**/
    return 0;
}
