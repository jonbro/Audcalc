/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef WS2812_H_
#define WS2812_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/sem.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "ws2812.pio.h"

#define FRAC_BITS 4
#define NUM_PIXELS 25
#define WS2812_PIN_BASE 22

#ifdef __cplusplus
extern "C" {
#endif

static inline void ws_put_pixel(uint32_t pixel_grb);
static inline uint32_t ws_urgb_u32(uint8_t r, uint8_t g, uint8_t b);

#define VALUE_PLANE_COUNT (8 + FRAC_BITS)
// we store value (8 bits + fractional bits of a single color (R/G/B/W) value) for multiple
// strings, in bit planes. bit plane N has the Nth bit of each string.
typedef struct {
    // stored MSB first
    uint32_t planes[VALUE_PLANE_COUNT];
} value_bits_t;

// Add FRAC_BITS planes of e to s and store in d
void add_error(value_bits_t *d, const value_bits_t *s, const value_bits_t *e);

typedef struct {
    uint8_t *data;
    uint data_len;
    uint frac_brightness; // 256 = *1.0;
} string_t;

// takes 8 bit color values, multiply by brightness and store in bit planes
void transform_strings(string_t **strings, uint num_strings, value_bits_t *values, uint value_length,
                       uint frac_brightness);
void dither_values(const value_bits_t *colors, value_bits_t *state, const value_bits_t *old_state, uint value_length);



int64_t reset_delay_complete(alarm_id_t id, void *user_data);

void __isr dma_complete_handler();

void dma_init(PIO pio, uint sm);

void output_strings_dma(value_bits_t *bits, uint value_length);

int ws2812_init();

void ws2812_setColors(uint32_t *incoming_colors);

void ws2812_trigger();

#ifdef __cplusplus
}
#endif
#endif // WS2812_H_