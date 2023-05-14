#ifndef HARDWARE_H_
#define HARDWARE_H_
#define I2C_PORT i2c1

#include "pico/stdlib.h"
#include "tlv320driver.h"
#include "hardware/watchdog.h"
#include "hardware/adc.h"
#include "pico/bootrom.h"
#include "ws2812.h"
#include "i2c_dma.h"

#ifdef __cplusplus
extern "C" {
#endif

void hardware_init();
void hardware_input_init();
void hardware_shutdown();
void hardware_reboot_usb();
void hardware_check_i2c_pullups(bool *scl, bool *sda);
bool hardware_get_key_state(uint8_t x, uint8_t y);
void hardware_get_all_key_state(uint32_t *keystate);
i2c_dma_t* hardware_get_i2c();

uint8_t hardware_get_battery_level();
bool hardware_has_usb_power();
void hardware_update_battery_level();
void hardware_set_mic(bool mic_state);
bool hardware_line_in_detected();
bool hardware_headphone_detected();
bool hardware_disable_power_latch();

#ifdef __cplusplus
}
#endif

#endif // HARDWARE_H_