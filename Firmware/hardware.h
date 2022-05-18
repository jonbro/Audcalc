#ifndef HARDWARE_H_
#define HARDWARE_H_
#include "pico/stdlib.h"
#include "tlv320driver.h"

#ifdef __cplusplus
extern "C" {
#endif

void hardware_init();

void hardware_set_amp(bool amp_state);
void hardware_set_amp_force(bool amp_state, bool force);

void hardware_set_mic(bool mic_state);

#ifdef __cplusplus
}
#endif

#endif // HARDWARE_H_