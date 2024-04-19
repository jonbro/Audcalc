#ifndef PRINTWRAPPER_H_
#define PRINTWRAPPER_H_
#define I2C_PORT i2c1

#include "pico/stdio.h"

#ifdef __cplusplus
extern "C" {
#endif

extern stdio_driver_t stdio_usb;
void usb_printf_init();

#ifdef __cplusplus
}
#endif

#endif // PRINTWRAPPER_H_