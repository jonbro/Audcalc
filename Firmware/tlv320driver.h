#ifndef TLV320DRIVER_H
#define TLV320DRIVER_H

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

void tlvDriverInit();
bool readRegister(uint8_t page, uint8_t reg, uint8_t *rxdata);

#endif /* TLV320DRIVER_H */