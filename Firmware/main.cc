#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/sleep.h"
#include "hardware/i2c.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/pio.h"
#include "hardware/adc.h"
#include "hardware/flash.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"
#include "hardware/structs/scb.h"
#include "hardware/rosc.h"

#include <math.h>

#include "hardware/clocks.h"
#include "hardware/structs/clocks.h"
#include "input_output_copy_i2s.pio.h"
#include "ws2812.pio.h"

extern "C" {
#include "tlv320driver.h"
#include "ssd1306.h"
//#include "usb_audio.h"
}
#include "GrooveBox.h"
#include "audio/macro_oscillator.h"

#include "usb_microphone.h"

#include "filesystem.h"
#include "ws2812.h"
#include "hardware.h"

#define POCKETMOD_IMPLEMENTATION
//#include "pocketmod.h"
//#include "sundown.h"
//#include "bananasplit.h"

using namespace braids;

// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define I2C_PORT i2c1
#define I2C_SDA 2
#define I2C_SCL 3
#define SUBSYSTEM_RESET_PIN 21

#define TLV_I2C_ADDR            0x18
#define TLV_REG_PAGESELECT	    0
#define TLV_REG_LDOCONTROL	    2
#define TLV_REG_RESET   	    1
#define TLV_REG_CLK_MULTIPLEX   	0x04

#define SAMPLES_PER_BUFFER 128
#define USING_DEMO_BOARD 0
// TDM board defines
#if USING_DEMO_BOARD
    #define I2S_DATA_PIN 26
    #define I2S_BCLK_PIN 27
#else
    #define I2S_DATA_PIN 19
    #define I2S_BCLK_PIN 17
#endif
// demo board defines

#define BLINK_PIN_LED 25
#define row_pin_base 11
#define col_pin_base 6

void on_usb_microphone_tx_ready();

static float clip(float value)
{
    value = value < -1.0f ? -1.0f : value;
    value = value > +1.0f ? +1.0f : value;
    return value;
}

int dma_chan_input;
int dma_chan_output;
uint sm;
uint32_t capture_buf[SAMPLES_PER_BUFFER*2];
int capture_buf_offset = 0;
int output_buf_offset = 0;

uint32_t output_buf[SAMPLES_PER_BUFFER*3];
uint32_t silence_buf[SAMPLES_PER_BUFFER*2];
bool flipFlop = false;
int dmacount = 0;
extern uint32_t __flash_binary_end;

bool initial_sample = false;
int input_position = 0;
int initial_sample_count = 0;

GrooveBox *gbox;
uint16_t work_buf[SAMPLES_PER_BUFFER];
void __not_in_flash_func(dma_input_handler)() {
    if (dma_hw->ints0 & (1u<<dma_chan_input)) {
        uint32_t *next_capture_buf = capture_buf+((capture_buf_offset+1)%2)*SAMPLES_PER_BUFFER;
        dma_hw->ints0 = (1u << dma_chan_input);
        dma_channel_set_write_addr(dma_chan_input, next_capture_buf, true);
        capture_buf_offset = (capture_buf_offset+1)%2;
        uint32_t *input = capture_buf+capture_buf_offset*SAMPLES_PER_BUFFER;
        uint32_t *output = output_buf+output_buf_offset*SAMPLES_PER_BUFFER;
        if(!gbox->erasing)
        {
            gbox->Render((int16_t*)(output), (int16_t*)(input), SAMPLES_PER_BUFFER);
        }
        else
        {
            // todo monitor / passthrough here
            // for now, just clear it
            memset(output, 0, SAMPLES_PER_BUFFER*4);
        }
        capture_buf_offset = (capture_buf_offset+1)%2;
    }
}
bool hi = false;
int16_t out_count = 0;
int flipflopout = 0;

int triCount = 0;
int note = 60;

int needsNewAudioBuffer = 0;

void __not_in_flash_func(dma_output_handler)() {
    if (dma_hw->ints0 & (1u<<dma_chan_output)) {
        dma_hw->ints0 = 1u << dma_chan_output;
        dma_channel_set_read_addr(dma_chan_output, output_buf+output_buf_offset*SAMPLES_PER_BUFFER, true);
        output_buf_offset = (output_buf_offset+1)%3;
    }
}

int flashReadPos = 0;

static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return
            ((uint32_t) (r) << 8) |
            ((uint32_t) (g) << 16) |
            (uint32_t) (b);
}
bool needsScreenupdate;
bool repeating_timer_callback(struct repeating_timer *t) {
    needsScreenupdate = true;
    return true;
}

bool usb_timer_callback(struct repeating_timer *t) {
    //usbaudio_update();
    return true;
}

bool screen_flip_ready = false;
ssd1306_t disp;
int drawY = 0;
typedef struct
{
    bool screen_flip_ready;
} queue_entry_t;
queue_t signal_queue;
queue_t complete_queue;
void draw_screen()
{
    // this needs to be called here, because the multicore launch relies on
    // the fifo being functional
    multicore_lockout_victim_init();
    disp.external_vcc=false;
    ssd1306_init(&disp, 128, 32, 0x3C, I2C_PORT);
    ssd1306_poweroff(&disp);
    sleep_ms(3);
    ssd1306_poweron(&disp);
    ssd1306_clear(&disp);
    ssd1306_contrast(&disp, 0x7f); // lower brightness / power requirement
    while(true)
    {
        queue_entry_t entry;
        // this should spinlock here
        queue_remove_blocking(&signal_queue, &entry);
        if(entry.screen_flip_ready)
        {
            ssd1306_show(&disp);
            bool result = true;
            queue_add_blocking(&complete_queue, &result);
        }
    }
}


void configure_audio_driver()
{
    /**/
    PIO pio = pio1;
    uint offset = pio_add_program(pio, &input_output_copy_i2s_program);
    printf("loaded program at offset: %i\n", offset);
    sm = pio_claim_unused_sm(pio, true);
    printf("claimed sm: %i\n", sm); //I2S_DATA_PIN
    int sample_freq = 32000;
    printf("setting pio freq %d\n", (int) sample_freq);
    uint32_t system_clock_frequency = clock_get_hz(clk_sys);
    assert(system_clock_frequency < 0x40000000);

    uint32_t divider = system_clock_frequency*2 / sample_freq; // avoid arithmetic overflow
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
        SAMPLES_PER_BUFFER, // Number of transfers
        true// Start immediately
    );
    // Tell the DMA to raise IRQ line 0 when the channel finishes a block
    dma_channel_set_irq0_enabled(dma_chan_input, true);

    // Configure the processor to run dma_handler() when DMA IRQ 0 is asserted
    irq_add_shared_handler(DMA_IRQ_0, dma_input_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY-1);
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
        SAMPLES_PER_BUFFER, // Number of transfers
        true// Start immediately
    );
    dma_channel_set_irq0_enabled(dma_chan_output, true);
    // Configure the processor to run dma_handler() when DMA IRQ 0 is asserted
    irq_add_shared_handler(DMA_IRQ_0, dma_output_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY-2);
    irq_set_enabled(DMA_IRQ_0, true);
}
// see https://ghubcoder.github.io/posts/awaking-the-pico/ to explain why we need this recovery code
void recover_from_sleep(uint scb_orig, uint clock0_orig, uint clock1_orig){

    //Re-enable ring Oscillator control
    rosc_write(&rosc_hw->ctrl, ROSC_CTRL_ENABLE_BITS);

    //reset procs back to default
    scb_hw->scr = scb_orig;
    clocks_hw->sleep_en0 = clock0_orig;
    clocks_hw->sleep_en1 = clock1_orig;

    //reset clocks
    clocks_init();
    stdio_init_all();
    return;
}

void sleep()
{
    // TODO: copy all critical state to flash and shut down ram prior to sleep
    // set all columns high
    for (size_t i = 0; i < 5; i++)
    {
        gpio_init(col_pin_base+i);
        gpio_set_dir(col_pin_base+i, GPIO_OUT);
        gpio_disable_pulls(col_pin_base+i);
        gpio_init(row_pin_base+i);
        gpio_set_dir(row_pin_base+i, GPIO_IN);
        gpio_pull_down(row_pin_base+i);
        // the mask here are the gpio pins for the colums
        gpio_put(col_pin_base+i, true);
        sleep_us(3);
    }
    uint scb_orig = scb_hw->scr;
    uint clock0_orig = clocks_hw->sleep_en0;
    uint clock1_orig = clocks_hw->sleep_en1;

    multicore_reset_core1();
    ssd1306_poweroff(&disp);
    gpio_put(SUBSYSTEM_RESET_PIN, 0);
    sleep_ms(10);

    uart_default_tx_wait_blocking();
    gpio_put(25, false);
    sleep_run_from_xosc();
    sleep_goto_dormant_until_level_high(row_pin_base);
    recover_from_sleep(scb_orig, clock0_orig, clock1_orig); 
    ssd1306_poweron(&disp);
    multicore_launch_core1(draw_screen);
    gpio_put(SUBSYSTEM_RESET_PIN, 1);
    sleep_ms(10);
    tlvDriverInit();

    gpio_put(25, true);
}


uint8_t adc1_prev;
uint8_t adc2_prev;

#define LINE_IN_DETECT 24
#define HEADPHONE_DETECT 16

int main()
{
    gpio_init(BLINK_PIN_LED);
    gpio_set_dir(BLINK_PIN_LED, GPIO_OUT);
    gpio_put(BLINK_PIN_LED, true);
    sleep_ms(100);
    set_sys_clock_khz(240000, true);
    SetDisplay(&disp);
    stdio_init_all();
    //
    // TestFileSystem();
    // TestVoiceData();
    // ParamLockPoolTest testLockPool = ParamLockPoolTest();
    // testLockPool.RunTest();
    // return 0;
    ws2812_init();

    gpio_init(SUBSYSTEM_RESET_PIN);
    gpio_set_dir(SUBSYSTEM_RESET_PIN, GPIO_OUT);

    gpio_put(SUBSYSTEM_RESET_PIN, 1);
    sleep_ms(10);
    gpio_put(SUBSYSTEM_RESET_PIN, 0);
    sleep_ms(40);
    gpio_put(SUBSYSTEM_RESET_PIN, 1);
    sleep_ms(20);
    gpio_put(BLINK_PIN_LED, false);
    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400*1000);
    queue_init(&signal_queue, sizeof(queue_entry_t), 2);
    queue_init(&complete_queue, sizeof(bool), 2);
    bool r = true;
    queue_add_blocking(&complete_queue, &r);
    multicore_launch_core1(draw_screen);
    multicore_lockout_start_timeout_us(500);
    multicore_lockout_end_timeout_us(500);
    
    {
        // power hold on
        gpio_init(23);
        gpio_set_dir(23, GPIO_IN);
        gpio_set_pulls(23, true, false);
        
        gpio_init(LINE_IN_DETECT);
        gpio_set_dir(LINE_IN_DETECT, GPIO_IN);
        gpio_pull_down(LINE_IN_DETECT);

        gpio_init(HEADPHONE_DETECT);
        gpio_set_dir(HEADPHONE_DETECT, GPIO_IN);
        gpio_pull_up(HEADPHONE_DETECT);
        
        //enable amp
        hardware_init();
        hardware_set_amp_force(false, true);
    }

    // setup the rows / colums for input
    for (size_t i = 0; i < 5; i++)
    {
        gpio_init(col_pin_base+i);
        gpio_set_dir(col_pin_base+i, GPIO_OUT);
        gpio_disable_pulls(col_pin_base+i);
        gpio_init(row_pin_base+i);
        gpio_set_dir(row_pin_base+i, GPIO_IN);
        gpio_pull_down(row_pin_base+i);
    }
    gpio_put_masked(0x7c0, 1<<(col_pin_base));
    bool key13 = gpio_get(row_pin_base+4);
    gpio_put_masked(0x7c0, 1<<(col_pin_base+3));
    bool key16 = gpio_get(row_pin_base+4);
    printf("keys held %x %x\n", key13, key16);
    // if the user is holding down 13 & 16 during startup, then clear filesystem
    InitializeFilesystem(key13&&key16);

    adc_init();
    adc_gpio_init(26);
    adc_gpio_init(27);
    uint32_t color[25];
    memset(color, 0, 25 * sizeof(uint32_t));

    gbox = new GrooveBox(color);
    //gbox->Deserialize();
    // fill the silence buffer so we get something out
    for(int i=0;i<SAMPLES_PER_BUFFER;i++)
    {
        uint16_t* chan = (uint16_t*)(silence_buf+i);
        chan[0] = 0;
        chan[1] = 0;
    }

    configure_audio_driver();

    tlvDriverInit();

    int step = 0;
    uint32_t keyState = 0;
    uint32_t lastKeyState = 0;

    struct repeating_timer timer;
     add_repeating_timer_ms(-16, repeating_timer_callback, NULL, &timer);
    struct repeating_timer timer2;
    //add_repeating_timer_us(-50,usb_timer_callback, NULL, &timer2);
    // Select ADC input 0 (GPIO26)
    adc_select_input(0);
    int16_t touchCounter = 0x7fff;
    int16_t headphoneCheck = 60;
    uint8_t brightnesscount = 0;
   // usbaudio_init();
    bool requestSerialize = false;
    bool requestDeserialize = false;

      // initialize the USB microphone interface
    // usb_microphone_set_tx_ready_handler(on_usb_microphone_tx_ready);

    while(true)
    {
        gpio_put(col_pin_base, true);
        // read keys
        for (size_t i = 0; i < 5; i++)
        {
            // the mask here are the gpio pins for the colums
            gpio_put_masked(0x7c0, 1<<(col_pin_base+i));
            sleep_us(2); // should put this on 
            for (size_t j = 0; j < 5; j++)
            {
                int index = (i*5+j);
                bool keyVal = index==0?!gpio_get(23):gpio_get(j+row_pin_base);
                if(keyVal)
                {
                    keyState |= 1ul << index;
                }
                else
                {
                    keyState &= ~(1ul << index);
                }
            }
        }
        
        // act on keychanges
        for (size_t i = 0; i < 25; i++)
        {
            uint32_t s = keyState & (1ul<<i);
            if((keyState & (1ul<<i)) != (lastKeyState & (1ul<<i)))
            {
                touchCounter = 0x7fff;
                touchCounter = 0x1f4;
                gbox->OnKeyUpdate(i, s>0); 
                if(s>0)
                {
                    requestSerialize = i==21;
                    requestDeserialize = i==22;
                } 
            }
        }
        lastKeyState = keyState;
        if(needsScreenupdate)
        {
            touchCounter--;
            if(touchCounter <= 0)
            {
                // store song here
                // gbox->Serialize();
                //gpio_set_pulls(23, false, true);
            }
            adc_select_input(1);
            uint16_t adc_val = adc_read();
            adc_select_input(0);
            // I think that even though adc_read returns 16 bits, the value is only in the top 12
            gbox->OnAdcUpdate(adc_val >> 4, adc_read()>>4);
            adc_select_input(2);
            // printf("battery: %i \n", adc_read());
            bool flip_complete = false;
            if(queue_try_remove(&complete_queue, &flip_complete))
            {
                gbox->UpdateDisplay(&disp);
                queue_entry_t entry = {true};
                queue_add_blocking(&signal_queue, &entry);
            }

            ws2812_setColors(color+5);
            needsScreenupdate = false;
            ws2812_trigger();
        }
    }
    return 0;
}
