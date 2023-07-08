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
#include "tlv320driver.h"

extern "C" {
#include "ssd1306.h"
//#include "usb_audio.h"
}
#include "m6x118pt7b.h"

#include "GrooveBox.h"
#include "audio/macro_oscillator.h"

#include "usb_microphone.h"

#include "filesystem.h"
#include "ws2812.h"
#include "hardware.h"
#include <pb_encode.h>
#include <pb_decode.h>
#include "ParamLockPoolInternal.pb.h"
#include "USBSerialDevice.h"
#include "diagnostics.h"
#include "multicore_support.h"
#include "GlobalDefines.h"

using namespace braids;


#define TLV_I2C_ADDR            0x18
#define TLV_REG_PAGESELECT	    0
#define TLV_REG_LDOCONTROL	    2
#define TLV_REG_RESET   	    1
#define TLV_REG_CLK_MULTIPLEX   	0x04

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

bool audioInReady = false;
bool audioOutReady = false;
uint32_t capture_buf[SAMPLES_PER_BLOCK*BLOCKS_PER_SEND*2];
uint32_t output_buf[SAMPLES_PER_BLOCK*BLOCKS_PER_SEND*2];

int outBufOffset = 0;
int inBufOffset = 0;
int audioGenOffset = 0;
GrooveBox gbox;
USBSerialDevice usbSerialDevice;
auto_init_mutex(audioProcessMutex);

int triCount = 0;
int note = 60;


/*
* double buffered audio - if the signal that the next frame of audio hasn't completed sending,
*/ 

void __not_in_flash_func(dma_input_handler)() {
    if (dma_hw->ints0 & (1u<<dma_chan_input)) {
        dma_hw->ints0 = (1u << dma_chan_input);
        dma_channel_set_write_addr(dma_chan_input, capture_buf+(inBufOffset)*SAMPLES_PER_SEND, true);
        uint32_t x;
        if(mutex_try_enter(&audioProcessMutex, &x))
        {
            inBufOffset = (inBufOffset+1)%2;
            audioInReady = true;
            mutex_exit(&audioProcessMutex);
        }
    }
}

void __not_in_flash_func(dma_output_handler)() {
    if (dma_hw->ints0 & (1u<<dma_chan_output)) {
        dma_hw->ints0 = 1u << dma_chan_output;
        dma_channel_set_read_addr(dma_chan_output, output_buf+(outBufOffset)*SAMPLES_PER_SEND, true);
        uint32_t x;
        if(mutex_try_enter(&audioProcessMutex, &x))
        {
            outBufOffset = (outBufOffset+1)%2;
            audioOutReady = true;
            mutex_exit(&audioProcessMutex);
        }
    }
}

int flashReadPos = 0;

static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
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
int drawY = 0;
void __not_in_flash_func(draw_screen)()
{
    // this needs to be called here, because the multicore launch relies on
    // the fifo being functional
    sleep_ms(20);
    multicore_lockout_victim_init();
    while(true)
    {
        queue_entry_t entry;
        if(queue_try_remove(&signal_queue, &entry))
        {
            if(entry.renderInstrument >= 0)
            {
                gbox.instruments[entry.renderInstrument].Render(entry.sync_buffer, entry.workBuffer, 64);
                queue_entry_complete_t result;
                result.screenFlipComplete = false;
                result.renderInstrumentComplete = true;
                queue_add_blocking(&renderCompleteQueue, &result);
            }
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
        SAMPLES_PER_SEND, // Number of transfers
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
        output_buf, // Source pointer
        SAMPLES_PER_SEND, // Number of transfers
        true// Start immediately
    );
    dma_channel_set_irq0_enabled(dma_chan_output, true);
    // Configure the processor to run dma_handler() when DMA IRQ 0 is asserted
    irq_add_shared_handler(DMA_IRQ_0, dma_output_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY-2);
    irq_set_enabled(DMA_IRQ_0, true);
}

uint8_t adc1_prev;
uint8_t adc2_prev;

#define LINE_IN_DETECT 24
#define HEADPHONE_DETECT 16

int main()
{
    stdio_init_all();
    hardware_init();
    {
        // if the user isn't holding the powerkey, 
        // or if holding power & esc then immediately shutdown
        if(!hardware_get_key_state(0,0) || hardware_get_key_state(3, 0))
        {
            hardware_shutdown();
        }
        if(hardware_get_key_state(4, 4) && hardware_get_key_state(0, 4))
        {
            Diagnostics diag;
            diag.run();
        } else if(hardware_get_key_state(4, 4))
        {
            hardware_reboot_usb();
        }
    }
    ws2812_init();
    
    queue_init(&signal_queue, sizeof(queue_entry_t), 3);
    queue_init(&complete_queue, sizeof(queue_entry_complete_t), 2);
    queue_init(&renderCompleteQueue, sizeof(queue_entry_complete_t), 2);
    queue_entry_complete_t result;
    result.screenFlipComplete = true;
    result.renderInstrumentComplete = false;
    queue_add_blocking(&complete_queue, &result);
    multicore_launch_core1(draw_screen);

    // if the user is holding down specific keys on powerup, then clear the full file system
    InitializeFilesystem(hardware_get_key_state(0,4) && hardware_get_key_state(3, 4));

    uint32_t color[25];
    memset(color, 0, 25 * sizeof(uint32_t));
    //usbSerialDevice.Init();
    gbox.init(color);

    memset(output_buf, SAMPLES_PER_SEND*2, sizeof(uint32_t));
    memset(capture_buf, SAMPLES_PER_SEND*2, sizeof(uint32_t));

    configure_audio_driver();

    tlvDriverInit();

    int step = 0;
    uint32_t keyState = 0;
    uint32_t lastKeyState = 0;

    struct repeating_timer timer;
    add_repeating_timer_ms(-33, repeating_timer_callback, NULL, &timer);
    struct repeating_timer timer2;

    adc_select_input(0);
    int16_t touchCounter = 0x7fff;
    int16_t headphoneCheck = 60;
    uint8_t brightnesscount = 0;
    int lostCount = 0;
    while(true)
    {
        if(audioOutReady && audioInReady)
        {
            mutex_enter_blocking(&audioProcessMutex); 
            for(int i=0;i<BLOCKS_PER_SEND;i++)
            {
                uint32_t *input = capture_buf+inBufOffset*SAMPLES_PER_SEND+SAMPLES_PER_BLOCK*i;
                uint32_t *output = output_buf+outBufOffset*SAMPLES_PER_SEND+SAMPLES_PER_BLOCK*i;
                gbox.Render((int16_t*)(output), (int16_t*)(input), SAMPLES_PER_BLOCK);
            }
            audioInReady = false;
            audioOutReady = false;
            mutex_exit(&audioProcessMutex);
        }
        else
        {
            hardware_get_all_key_state(&keyState);
            
            // act on keychanges
            for (size_t i = 0; i < 25; i++)
            {
                uint32_t s = keyState & (1ul<<i);
                if((keyState & (1ul<<i)) != (lastKeyState & (1ul<<i)))
                {
                    gbox.OnKeyUpdate(i, s>0); 
                }
            }
            lastKeyState = keyState;
            if(needsScreenupdate)
            {
                adc_select_input(1);
                uint16_t adc_val = adc_read();
                adc_select_input(0);
                // I think that even though adc_read returns 16 bits, the value is only in the top 12
                gbox.OnAdcUpdate(adc_val, adc_read());
                hardware_update_battery_level();
                queue_entry_complete_t result;
                gbox.UpdateDisplay(GetDisplay());
                ssd1306_show(GetDisplay());
                ws2812_setColors(color+5);
                needsScreenupdate = false;
                ws2812_trigger();
            }
        }
    }
    return 0;
}