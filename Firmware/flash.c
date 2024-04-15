/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2021, Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <string.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"

//--------------------------------------------------------------------+
// MACRO TYPEDEF CONSTANT ENUM DECLARATION
//--------------------------------------------------------------------+

#define FLASH_CACHE_SIZE          (4*1024U)
#define FLASH_CACHE_INVALID_ADDR  0xffffffff

static uint32_t _fl_addr = FLASH_CACHE_INVALID_ADDR;
static uint8_t _fl_buf[FLASH_CACHE_SIZE] __attribute__((aligned(4)));

//------------- IMPLEMENTATION -------------//

void flash_read(uint32_t addr, void* buffer, uint32_t len)
{
  memcpy(buffer, (void*) (XIP_BASE + addr), len);
}

void flash_flush(void)
{
  if ( _fl_addr == FLASH_CACHE_INVALID_ADDR ) return;

  // Only erase and write if contents does not matches
  if ( 0 != memcmp(_fl_buf, (void*) (XIP_BASE + _fl_addr), FLASH_CACHE_SIZE) )
  {
    //printf("Erase and Write at 0x%08X", _fl_addr);

    flash_range_erase(_fl_addr, FLASH_CACHE_SIZE);
    flash_range_program(_fl_addr, _fl_buf, FLASH_CACHE_SIZE);
  }

  _fl_addr = FLASH_CACHE_INVALID_ADDR;
}

void flash_write (uint32_t addr, void const *data, uint32_t len)
{
  uint32_t new_addr = addr & ~(FLASH_CACHE_SIZE - 1);

  if ( new_addr != _fl_addr )
  {
    flash_flush();

    _fl_addr = new_addr;
    flash_read(new_addr, _fl_buf, FLASH_CACHE_SIZE);
  }

  memcpy(_fl_buf + (addr & (FLASH_CACHE_SIZE - 1)), data, len);
}