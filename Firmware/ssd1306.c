/*

Original code https://github.com/daschr/pico-ssd1306

Copyright (c) 2021 David Schramm

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

// jonathan brodsky modified the original code to add support for non-blocking / dma screen writing

#include <pico/stdlib.h>
#include <hardware/i2c.h>
#include <pico/binary_info.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "ssd1306.h"
#include "font.h"
#include "i2c_dma.h"
#include "hardware.h"
#define I2C_DMA_CHANNEL_WRITE 11

ssd1306_t disp;

ssd1306_t* GetDisplay()
{
    return &disp;
}

inline static void fancy_write(i2c_inst_t *i2c, uint8_t addr, uint8_t *src, size_t len, char *name) {
    i2c_get_hw(i2c)->enable = 0;
    i2c_get_hw(i2c)->tar = addr;
    i2c_get_hw(i2c)->enable = 1;
    dma_channel_config c = dma_channel_get_default_config(I2C_DMA_CHANNEL_WRITE);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_dreq(&c, i2c_get_dreq(i2c, true));
    dma_channel_configure(I2C_DMA_CHANNEL_WRITE, &c,
                          &i2c_get_hw(i2c)->data_cmd, src, len, true);
    // i2c_write_blocking(i2c, addr, src, len, false);

}

inline static void ssd1306_write(ssd1306_t *p, uint8_t val) {
    uint8_t d[2]= {0x00, val};
    i2c_write_blocking(p->i2c_i, p->address, d, 2, false);
}

bool ssd1306_init(ssd1306_t *p, uint16_t width, uint16_t height, uint8_t address, i2c_inst_t *i2c_instance) {
    p->width=width;
    p->height=height;
    p->pages=height/8;
    p->address=address;
    p->string_invert=false;
    p->i2c_i=i2c_instance;


    p->bufsize=(p->pages)*(p->width);
    if((p->buffer=malloc(p->bufsize+1))==NULL) {
        p->bufsize=0;
        return false;
    }
    if((p->sendBuffer=malloc((p->bufsize+2)*sizeof(uint16_t)))==NULL) {
        p->bufsize=0;
        return false;
    }
    
    ++(p->buffer);

	// from https://github.com/makerportal/rpi-pico-ssd1306
    int8_t cmds[]= {
        SET_DISP | 0x00,  // off
        // address setting
        SET_MEM_ADDR,
        0x00,  // horizontal
        // resolution and layout
        SET_DISP_START_LINE | 0x00,
        SET_SEG_REMAP | 0x01,  // column addr 127 mapped to SEG0
        SET_MUX_RATIO,
        height - 1,
        SET_COM_OUT_DIR | 0x08,  // scan from COM[N] to COM0
        SET_DISP_OFFSET,
        0x00,
        SET_COM_PIN_CFG,
        width>2*height?0x02:0x12,
        // timing and driving scheme
        SET_DISP_CLK_DIV,
        0x80,
        SET_PRECHARGE,
        p->external_vcc?0x22:0xF1,
        SET_VCOM_DESEL,
        0x30,  // 0.83*Vcc
        // display
        SET_CONTRAST,
        0xFF,  // maximum
        SET_ENTIRE_ON,  // output follows RAM contents
        SET_NORM_INV,  // not inverted
        // charge pump
        SET_CHARGE_PUMP,
        p->external_vcc?0x10:0x14,
        SET_DISP | 0x01
    };

    for(size_t i=0; i<sizeof(cmds); ++i)
        ssd1306_write(p, cmds[i]);

    return true;
}

inline void ssd1306_deinit(ssd1306_t *p) {
    free(p->buffer-1);
}

inline void ssd1306_poweroff(ssd1306_t *p) {
    ssd1306_write(p, SET_DISP|0x00);
}

inline void ssd1306_poweron(ssd1306_t *p) {
    ssd1306_write(p, SET_DISP|0x01);
}

inline void ssd1306_contrast(ssd1306_t *p, uint8_t val) {
    ssd1306_write(p, SET_CONTRAST);
    ssd1306_write(p, val);
}

inline void ssd1306_invert(ssd1306_t *p, uint8_t inv) {
    ssd1306_write(p, SET_NORM_INV | (inv & 1));
}

inline void ssd1306_clear(ssd1306_t *p) {
    memset(p->buffer, 0, p->bufsize);
}

void ssd1306_draw_pixel(ssd1306_t *p, uint32_t x, uint32_t y) {
	if(x>=p->width || y>=p->height) return;
    x = 127-x;
    y = 31-y;
    p->buffer[x+p->width*(y>>3)]|=0x1<<(y&0x07); // y>>3==y/8 && y&0x7==y%8
}


void ssd1306_clear_pixel(ssd1306_t *p, uint32_t x, uint32_t y) {
	if(x>=p->width || y>=p->height) return;
    x = 127-x;
    y = 31-y;
    p->buffer[x+p->width*(y>>3)]&=~(0x1<<(y&0x07)); // y>>3==y/8 && y&0x7==y%8
}

void ssd1306_draw_line(ssd1306_t *p, int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    if(x1==x2)
    {
        for(int32_t i=y1; i<=y2; ++i) {
            ssd1306_draw_pixel(p, (uint32_t)x1, (uint32_t) i);
        }
        return;
    }
    if(x1>x2) {
        uint32_t t=x1;
        x1=x2;
        x2=t;
        t=y1;
        y1=y2;
        y2=t;
    }

    float m=(float) (y2-y1) / (float) (x2-x1);

    for(int32_t i=x1; i<=x2; ++i) {
        float y=m*(float) (i-x1)+(float) y1;
        ssd1306_draw_pixel(p, (uint32_t) i, (uint32_t) y);
    }
}

void ssd1306_draw_square(ssd1306_t *p, uint32_t x, uint32_t y, uint32_t width, uint32_t height){
	for(uint32_t i=0;i<width;++i)
		for(uint32_t j=0;j<height;++j)
			ssd1306_draw_pixel(p, x+i, y+j);
}
void ssd1306_draw_square_rounded(ssd1306_t *p, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    for(uint32_t i=0;i<width;++i)
    {
        for(uint32_t j=0;j<height;++j)
        {
            if((i==0||i==width-1)&&(j==0||j==height-1))
                continue;
            ssd1306_draw_pixel(p, x+i, y+j);
        }
    }
}

void ssd1306_clear_square(ssd1306_t *p, uint32_t x, uint32_t y, uint32_t width, uint32_t height){
	for(uint32_t i=0;i<width;++i)
		for(uint32_t j=0;j<height;++j)
			ssd1306_clear_pixel(p, x+i, y+j);
}
void ssd1306_clear_square_rounded(ssd1306_t *p, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    for(uint32_t i=0;i<width;++i)
    {
        for(uint32_t j=0;j<height;++j)
        {
            if((i==0||i==width-1)&&(j==0||j==height-1))
                continue;
            ssd1306_clear_pixel(p, x+i, y+j);
        }
    }
}

void ssd1306_draw_empty_square(ssd1306_t *p, uint32_t x, uint32_t y, uint32_t width, uint32_t height){
	ssd1306_draw_line(p, x, y, x+width, y);
	ssd1306_draw_line(p, x, y+height, x+width, y+height);
	ssd1306_draw_line(p, x, y, x, y+height);
	ssd1306_draw_line(p, x+width, y, x+width, y+height);
}

void ssd1306_draw_char_with_font(ssd1306_t *p, uint32_t x, uint32_t y, uint32_t scale, const uint8_t *font, char c) {
    if(c > '~')
        return;

    for(uint8_t i=0; i<font[1]; ++i) {
        uint8_t line=(uint8_t)(font[(c-0x20)*font[1]+i+2]);

        for(int8_t j=0; j<font[0]; ++j, line>>=1) {
            if(line & 1 ==1)
                p->string_invert?ssd1306_clear_square(p, x+i*scale, y+j*scale, scale, scale):ssd1306_draw_square(p, x+i*scale, y+j*scale, scale, scale);
        }
    }
}

void ssd1306_draw_string_with_font(ssd1306_t *p, uint32_t x, uint32_t y, uint32_t scale, const uint8_t *font, const char *s) {
    for(int32_t x_n=x; *s; x_n+=font[0]*scale) {
        ssd1306_draw_char_with_font(p, x_n, y, scale, font, *(s++));
    }
}

void ssd1306_draw_char(ssd1306_t *p, uint32_t x, uint32_t y, uint32_t scale, char c) {
    ssd1306_draw_char_with_font(p, x, y, scale, font_8x5, c);
}

void ssd1306_draw_string(ssd1306_t *p, uint32_t x, uint32_t y, uint32_t scale, const char *s) {
    ssd1306_draw_string_with_font(p, x, y, scale, font_8x5, s);
}

void ssd1306_show(ssd1306_t *p) {
    // the last item in the payload sets the startline to zero
    uint8_t payload[]= {SET_COL_ADDR, 0, p->width-1, SET_PAGE_ADDR, 0, p->pages-1, 0x40};
    if(p->width==64) {
        payload[1]+=32;
        payload[2]+=32;
    }

    for(size_t i=0; i<sizeof(payload); ++i){
        ssd1306_write(p, payload[i]);
    }

    // prefix the buffer with something?
    memset(p->sendBuffer, 0, 2*(p->bufsize+2));

    // signal that we are about to write ram / data
    p->sendBuffer[0]=0x40;
    // copy the bits into the sendbuffer
    for(int i=0;i<p->bufsize;i++)
    {
        p->sendBuffer[i+1] = *(p->buffer+i);
    }
    // p->sendBuffer[1] = 0xff;
    // p->sendBuffer[2] = 0xff;
    p->sendBuffer[0] |= I2C_IC_DATA_CMD_RESTART_BITS;
    p->sendBuffer[p->bufsize] |= I2C_IC_DATA_CMD_STOP_BITS;

    fancy_write(p->i2c_i, p->address, (uint8_t*)p->sendBuffer, p->bufsize+1, "ssd1306_show");
}
bool ssd1306_show_more(ssd1306_t *p)
{
    if(p->writeRemain == 0)
    {
        return true;
    }
    size_t writeAmountAvailable = i2c_get_write_available(p->i2c_i);
    size_t toWriteAmt = p->writeRemain<writeAmountAvailable?p->writeRemain:writeAmountAvailable;
    if(toWriteAmt == 0)
        return false;
    *(p->writeBuffer-1)=0x40;

    fancy_write(p->i2c_i, p->address, p->writeBuffer-1, toWriteAmt+1, "ssd1306_show");
    p->writeBuffer+=toWriteAmt;
    p->writeRemain-=toWriteAmt;
    return p->writeRemain==0;
}
void ssd1306_set_string_color(ssd1306_t *p, bool invert)
{
    p->string_invert = invert;
}

#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#endif
#ifndef pgm_read_word
#define pgm_read_word(addr) (*(const unsigned short *)(addr))
#endif
#ifndef pgm_read_dword
#define pgm_read_dword(addr) (*(const unsigned long *)(addr))
#endif
#if !defined(__INT_MAX__) || (__INT_MAX__ > 0xFFFF)
#define pgm_read_pointer(addr) ((void *)pgm_read_dword(addr))
#else
#define pgm_read_pointer(addr) ((void *)pgm_read_word(addr))
#endif

inline GFXglyph *pgm_read_glyph_ptr(const GFXfont *gfxFont, uint8_t c) {
#ifdef __AVR__
  return &(((GFXglyph *)pgm_read_pointer(&gfxFont->glyph))[c]);
#else
  // expression in __AVR__ section may generate "dereferencing type-punned
  // pointer will break strict-aliasing rules" warning In fact, on other
  // platforms (such as STM32) there is no need to do this pointer magic as
  // program memory may be read in a usual way So expression may be simplified
  return gfxFont->glyph + c;
#endif //__AVR__
}

inline uint8_t *pgm_read_bitmap_ptr(const GFXfont *gfxFont) {
#ifdef __AVR__
  return (uint8_t *)pgm_read_pointer(&gfxFont->bitmap);
#else
  // expression in __AVR__ section generates "dereferencing type-punned pointer
  // will break strict-aliasing rules" warning In fact, on other platforms (such
  // as STM32) there is no need to do this pointer magic as program memory may
  // be read in a usual way So expression may be simplified
  return gfxFont->bitmap;
#endif //__AVR__
}

void ssd1306_draw_string_gfxfont(ssd1306_t *p, int16_t x, int16_t y, const char *s,
                            bool white, uint8_t size_x,
                            uint8_t size_y, const GFXfont *gfxFont)
{
    int16_t cursor_x = x;
    uint8_t first = pgm_read_byte(&gfxFont->first);
    while(*s) {
        if ((*s < first) || (*s > (uint8_t)pgm_read_byte(&gfxFont->last)))
        {
            s++;
            continue;
        }    
        GFXglyph *glyph = pgm_read_glyph_ptr(gfxFont, *s - first);
        uint8_t w = pgm_read_byte(&glyph->width),
                h = pgm_read_byte(&glyph->height);
        if ((w > 0) && (h > 0)) { // Is there an associated bitmap?
            ssd1306_draw_char_gfxfont(p, cursor_x, y, *s,
                        white, size_x,
                        size_y, gfxFont);
        }
        cursor_x += (uint8_t)pgm_read_byte(&glyph->xAdvance);
        s++;
    }
}

void ssd1306_draw_char_gfxfont(ssd1306_t *p, int16_t x, int16_t y, unsigned char c,
                            bool white, uint8_t size_x,
                            uint8_t size_y, const GFXfont *gfxFont)
{
    // Character is assumed previously filtered by write() to eliminate
    // newlines, returns, non-printable characters, etc.  Calling
    // drawChar() directly with 'bad' characters of font may cause mayhem!

    c -= (uint8_t)pgm_read_byte(&gfxFont->first);
    GFXglyph *glyph = pgm_read_glyph_ptr(gfxFont, c);
    uint8_t *bitmap = pgm_read_bitmap_ptr(gfxFont);

    uint16_t bo = pgm_read_word(&glyph->bitmapOffset);
    uint8_t w = pgm_read_byte(&glyph->width), h = pgm_read_byte(&glyph->height);
    int8_t xo = pgm_read_byte(&glyph->xOffset),
            yo = pgm_read_byte(&glyph->yOffset);
    uint8_t xx, yy, bits = 0, bit = 0;
    int16_t xo16 = 0, yo16 = 0;

    if (size_x > 1 || size_y > 1) {
        xo16 = xo;
        yo16 = yo;
    }

    // Todo: Add character clipping here

    // NOTE: THERE IS NO 'BACKGROUND' COLOR OPTION ON CUSTOM FONTS.
    // THIS IS ON PURPOSE AND BY DESIGN.  The background color feature
    // has typically been used with the 'classic' font to overwrite old
    // screen contents with new data.  This ONLY works because the
    // characters are a uniform size; it's not a sensible thing to do with
    // proportionally-spaced fonts with glyphs of varying sizes (and that
    // may overlap).  To replace previously-drawn text when using a custom
    // font, use the getTextBounds() function to determine the smallest
    // rectangle encompassing a string, erase the area with fillRect(),
    // then draw new text.  This WILL infortunately 'blink' the text, but
    // is unavoidable.  Drawing 'background' pixels will NOT fix this,
    // only creates a new set of problems.  Have an idea to work around
    // this (a canvas object type for MCUs that can afford the RAM and
    // displays supporting setAddrWindow() and pushColors()), but haven't
    // implemented this yet.

    // startWrite();
    for (yy = 0; yy < h; yy++) {
        for (xx = 0; xx < w; xx++) {
            if (!(bit++ & 7)) {
                bits = pgm_read_byte(&bitmap[bo++]);
            }
            if (bits & 0x80) {
                if (size_x == 1 && size_y == 1) {
                    if(white)
                        ssd1306_draw_pixel(p, x + xo + xx, y + yo + yy);
                    else
                        ssd1306_clear_pixel(p, x + xo + xx, y + yo + yy);
                } else {
                    if(white)
                        ssd1306_draw_square(p, x + (xo16 + xx) * size_x, y + (yo16 + yy) * size_y,
                                size_x, size_y);
                    else
                        ssd1306_clear_square(p, x + (xo16 + xx) * size_x, y + (yo16 + yy) * size_y,
                                size_x, size_y);
                }
            }
            bits <<= 1;
        }
    }
    // endWrite();
}