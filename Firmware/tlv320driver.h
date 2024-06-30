#ifndef TLV320DRIVER_H
#define TLV320DRIVER_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

void tlvDriverInit();
bool readRegister(uint8_t page, uint8_t reg, uint8_t *rxdata);
void driver_set_mic(bool mic_state);
void driver_set_mute(bool mute);
void driver_set_hpvol(int8_t hpvol);

#ifdef __cplusplus
}
#endif

#endif /* TLV320DRIVER_H */