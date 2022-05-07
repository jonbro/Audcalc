#include "ws2812.h"

#define FRAC_BITS 4
#define NUM_PIXELS 25
#define WS2812_PIN_BASE 22

// horrible temporary hack to avoid changing pattern code
static uint8_t *current_string_out;
static bool current_string_4color;

static inline void ws_put_pixel(uint32_t pixel_grb) {
    *current_string_out++ = pixel_grb & 0xffu;
    *current_string_out++ = (pixel_grb >> 8u) & 0xffu;
    *current_string_out++ = (pixel_grb >> 16u) & 0xffu;
    if (current_string_4color) {
        *current_string_out++ = 0; // todo adjust?
    }
}
static inline uint32_t ws_urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return
            ((uint32_t) (r) << 8) |
            ((uint32_t) (g) << 16) |
            (uint32_t) (b);
}


// Add FRAC_BITS planes of e to s and store in d
void add_error(value_bits_t *d, const value_bits_t *s, const value_bits_t *e) {
    uint32_t carry_plane = 0;
    // add the FRAC_BITS low planes
    for (int p = VALUE_PLANE_COUNT - 1; p >= 8; p--) {
        uint32_t e_plane = e->planes[p];
        uint32_t s_plane = s->planes[p];
        d->planes[p] = (e_plane ^ s_plane) ^ carry_plane;
        carry_plane = (e_plane & s_plane) | (carry_plane & (s_plane ^ e_plane));
    }
    // then just ripple carry through the non fractional bits
    for (int p = 7; p >= 0; p--) {
        uint32_t s_plane = s->planes[p];
        d->planes[p] = s_plane ^ carry_plane;
        carry_plane &= s_plane;
    }
}

// takes 8 bit color values, multiply by brightness and store in bit planes
void transform_strings(string_t **strings, uint num_strings, value_bits_t *values, uint value_length,
                       uint frac_brightness) {
    for (uint v = 0; v < value_length; v++) {
        memset(&values[v], 0, sizeof(values[v]));
        for (int i = 0; i < num_strings; i++) {
            if (v < strings[i]->data_len) {
                // todo clamp?
                uint32_t value = (strings[i]->data[v] * strings[i]->frac_brightness) >> 8u;
                value = (value * frac_brightness) >> 8u;
                for (int j = 0; j < VALUE_PLANE_COUNT && value; j++, value >>= 1u) {
                    if (value & 1u) values[v].planes[VALUE_PLANE_COUNT - 1 - j] |= 1u << i;
                }
            }
        }
    }
}

void dither_values(const value_bits_t *colors, value_bits_t *state, const value_bits_t *old_state, uint value_length) {
    for (uint i = 0; i < value_length; i++) {
        add_error(state + i, colors + i, old_state + i);
    }
}

// requested colors * 4 to allow for RGBW
static value_bits_t colors[NUM_PIXELS * 4];
// double buffer the state of the string, since we update next version in parallel with DMAing out old version
static value_bits_t states[2][NUM_PIXELS * 4];

// example - string 0 is RGB only
static uint8_t string0_data[NUM_PIXELS * 3];

string_t string0 = {
    .data = string0_data,
    .data_len = sizeof(string0_data),
    .frac_brightness = 0x100,
};


string_t *strings[] = {
    &string0,
};

// chain channel for configuring main dma channel to output from disjoint 8 word fragments of memory
#define DMA_CB_CHANNEL 1

uint32_t dma_channel_mask;
uint32_t dma_cb_channel_mask;
int dma_cb_channel;

// start of each value fragment (+1 for NULL terminator)
static uintptr_t fragment_start[NUM_PIXELS * 4 + 1];

// posted when it is safe to output a new set of values
static struct semaphore reset_delay_complete_sem;
// alarm handle for handling delay
alarm_id_t reset_delay_alarm_id;

int64_t reset_delay_complete(alarm_id_t id, void *user_data) {
    reset_delay_alarm_id = 0;
    sem_release(&reset_delay_complete_sem);
    // no repeat
    return 0;
}

void __not_in_flash_func(dma_complete_handler)() {
    if (dma_hw->ints0 & dma_channel_mask) {
        // clear IRQ
        dma_hw->ints0 = dma_channel_mask;
        // when the dma is complete we start the reset delay timer
        if (reset_delay_alarm_id) cancel_alarm(reset_delay_alarm_id);
        reset_delay_alarm_id = add_alarm_in_us(400, reset_delay_complete, NULL, true);
    }
}

void dma_init(PIO pio, uint sm) {    
    int dma_channel = dma_claim_unused_channel(false);
    dma_channel_mask = 1u << dma_channel;
    dma_cb_channel = dma_claim_unused_channel(true);
    dma_cb_channel_mask = 1u << dma_cb_channel;

    // main DMA channel outputs 8 word fragments, and then chains back to the chain channel
    dma_channel_config channel_config = dma_channel_get_default_config(dma_channel);
    channel_config_set_dreq(&channel_config, pio_get_dreq(pio, sm, true));
    channel_config_set_chain_to(&channel_config, dma_cb_channel);
    channel_config_set_irq_quiet(&channel_config, true);
    dma_channel_configure(dma_channel,
                          &channel_config,
                          &pio->txf[sm],
                          NULL, // set by chain
                          8, // 8 words for 8 bit planes
                          false);

    // chain channel sends single word pointer to start of fragment each time
    dma_channel_config chain_config = dma_channel_get_default_config(dma_cb_channel);
    dma_channel_configure(dma_cb_channel,
                          &chain_config,
                          &dma_channel_hw_addr(
                                  dma_channel)->al3_read_addr_trig,  // ch DMA config (target "ring" buffer size 4) - this is (read_addr trigger)
                          NULL, // set later
                          1,
                          false);

    irq_add_shared_handler(DMA_IRQ_0, dma_complete_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    dma_channel_set_irq0_enabled(dma_channel, true);
    irq_set_enabled(DMA_IRQ_0, true);
}

void output_strings_dma(value_bits_t *bits, uint value_length) {
    for (uint i = 0; i < value_length; i++) {
        fragment_start[i] = (uintptr_t) bits[i].planes; // MSB first
    }
    fragment_start[value_length] = 0;
    dma_channel_hw_addr(dma_cb_channel)->al3_read_addr_trig = (uintptr_t) fragment_start;
}

uint32_t my_colors[20];
uint current = 0;
int brightness = 0xff<<FRAC_BITS;

int ws2812_init() {
    // todo get free sm
    PIO wspio = pio0;
    int wssm = pio_claim_unused_sm(wspio, true);
    uint offset = pio_add_program(wspio, &ws2812_parallel_program);

    ws2812_parallel_program_init(wspio, wssm, offset, WS2812_PIN_BASE, count_of(strings), 800000);

    sem_init(&reset_delay_complete_sem, 1, 1); // initially posted so we don't block first time
    dma_init(wspio, wssm);
    memset(my_colors, 0, 4*20);
}
void ws2812_setColors(uint32_t *incoming_colors)
{
    memcpy(my_colors, incoming_colors, 20*4);
}
void ws2812_trigger()
{
    if(sem_available(&reset_delay_complete_sem) > 0)
    {
        current_string_out = string0.data;
        current_string_4color = false;
        for (size_t j = 0; j < 20; j++)
        {
            uint8_t r = my_colors[j] & 0xffu;
            uint8_t g = (my_colors[j]>>8u) & 0xffu;
            uint8_t b = (my_colors[j]>>16u) & 0xffu;
            ws_put_pixel(ws_urgb_u32(r,g,b));
        }
        transform_strings(strings, count_of(strings), colors, NUM_PIXELS * 4, brightness);
        dither_values(colors, states[current], states[current ^ 1], NUM_PIXELS * 4);
        sem_acquire_blocking(&reset_delay_complete_sem);
        output_strings_dma(states[current], NUM_PIXELS * 4);
        current ^= 1;
    }
}